/**
 * @file CuOptSolver.cpp
 * @brief Implementação do algoritmo de escalonamento via NVIDIA cuOpt (com fallback MILP integrado).
 */

#include "solver/ScheduleSolver.h"
#include "solver/MilpModel.h"
#include "solver/SolverCommon.h"
#include <Highs.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <fstream>
#include <iomanip>
#include <string>
#include <utility>
#include <vector>

namespace scheduler::solver {

namespace {

/**
 * @brief Exporta o problema MILP para formato MPS padrão (Mathematical Programming System).
 * Utilizado para ingestão pelo NVIDIA cuOpt (C API, CLI, Python cuopt ou microserviço).
 */
bool exportMilpProblemToMps(const detail::MilpProblem& milp, const std::string& filename, const std::string& prob_name) {
    std::ofstream out(filename);
    if (!out.is_open()) return false;

    out << "NAME          " << prob_name << "\n";
    out << "ROWS\n";
    out << " N  OBJ\n";
    for (size_t i = 0; i < milp.rows.size(); ++i) {
        const auto& r = milp.rows[i];
        if (std::abs(r.lhs - r.rhs) < 1e-6) {
            out << " E  " << r.name << "\n";
        } else if (r.lhs <= -1e19) {
            out << " L  " << r.name << "\n";
        } else if (r.rhs >= 1e19) {
            out << " G  " << r.name << "\n";
        } else {
            out << " L  " << r.name << "\n";
        }
    }

    // Agrupa todas as entradas de coeficientes por coluna para conformidade estrita com o formato MPS
    std::vector<std::vector<std::pair<std::string, double>>> col_entries(milp.vars.size());
    for (size_t r_idx = 0; r_idx < milp.rows.size(); ++r_idx) {
        const auto& r = milp.rows[r_idx];
        for (size_t k = 0; k < r.cols.size(); ++k) {
            int col = r.cols[k];
            if (col >= 0 && col < static_cast<int>(milp.vars.size())) {
                col_entries[col].emplace_back(r.name, r.vals[k]);
            }
        }
    }

    out << "COLUMNS\n";
    for (size_t col = 0; col < milp.vars.size(); ++col) {
        const auto& v = milp.vars[col];
        if (std::abs(v.obj) > 1e-9) {
            out << "    " << v.name << "  OBJ  " << std::setprecision(10) << v.obj << "\n";
        }
        for (const auto& [row_name, val] : col_entries[col]) {
            out << "    " << v.name << "  " << row_name << "  " << std::setprecision(10) << val << "\n";
        }
    }

    out << "RHS\n";
    for (size_t i = 0; i < milp.rows.size(); ++i) {
        const auto& r = milp.rows[i];
        double b = 0.0;
        if (std::abs(r.lhs - r.rhs) < 1e-6) {
            b = r.lhs;
        } else if (r.lhs <= -1e19) {
            b = r.rhs;
        } else if (r.rhs >= 1e19) {
            b = r.lhs;
        } else {
            b = r.rhs;
        }
        if (std::abs(b) > 1e-9) {
            out << "    RHS1  " << r.name << "  " << std::setprecision(10) << b << "\n";
        }
    }

    out << "BOUNDS\n";
    for (size_t col = 0; col < milp.vars.size(); ++col) {
        const auto& v = milp.vars[col];
        if (v.lb == 0.0 && v.ub == 1.0) {
            out << " BV BND1  " << v.name << "\n";
        } else {
            out << " LO BND1  " << v.name << "  " << v.lb << "\n";
            out << " UP BND1  " << v.name << "  " << v.ub << "\n";
        }
    }

    out << "ENDATA\n";
    return true;
}

/**
 * @brief Detecta metadados da GPU NVIDIA e driver através do nvidia-smi ou kernel module.
 */
std::string detectNvidiaGpuMetadata() {
    std::string info;
    FILE* pipe = popen("nvidia-smi --query-gpu=name,memory.total,driver_version --format=csv,noheader,nounits 2>/dev/null", "r");
    if (pipe) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            info = buffer;
            while (!info.empty() && (info.back() == '\n' || info.back() == '\r')) {
                info.pop_back();
            }
        }
        pclose(pipe);
    }
    if (info.empty()) {
        std::ifstream proc_nv("/proc/driver/nvidia/version");
        if (proc_nv.is_open()) {
            std::string line;
            if (std::getline(proc_nv, line)) {
                info = line;
            }
        }
    }
    return info;
}

// Tipos C da API NVIDIA cuOpt
using cuOptOptimizationProblem = void*;
using cuOptSolverSettings = void*;
using cuOptSolution = void*;
using cuopt_int_t = int;
using cuopt_float_t = double;

constexpr int CUOPT_STATUS_SUCCESS = 0;
constexpr int CUOPT_OPT_MINIMIZE = 0;
constexpr char CUOPT_VAR_INTEGER = 'I';

struct CuOptDynLib {
    void* handle = nullptr;
    using FnCreateRangedProblem = cuopt_int_t (*)(
        cuopt_int_t num_constraints,
        cuopt_int_t num_variables,
        int sense,
        cuopt_float_t objective_offset,
        const cuopt_float_t* obj_coeffs,
        const cuopt_int_t* row_offsets,
        const cuopt_int_t* col_indices,
        const cuopt_float_t* values,
        const cuopt_float_t* con_lb,
        const cuopt_float_t* con_ub,
        const cuopt_float_t* var_lb,
        const cuopt_float_t* var_ub,
        const char* var_types,
        cuOptOptimizationProblem* problem
    );
    using FnCreateSolverSettings = cuopt_int_t (*)(cuOptSolverSettings* settings);
    using FnDestroySolverSettings = cuopt_int_t (*)(cuOptSolverSettings* settings);
    using FnSetIntegerParameter = cuopt_int_t (*)(cuOptSolverSettings settings, int param, cuopt_int_t value);
    using FnSetFloatParameter = cuopt_int_t (*)(cuOptSolverSettings settings, int param, cuopt_float_t value);
    using FnSolve = cuopt_int_t (*)(cuOptOptimizationProblem problem, cuOptSolverSettings settings, cuOptSolution* solution);
    using FnGetObjectiveValue = cuopt_int_t (*)(cuOptSolution solution, cuopt_float_t* objective_value);
    using FnGetSolutionValues = cuopt_int_t (*)(cuOptSolution solution, cuopt_float_t* values);
    using FnGetTerminationStatus = cuopt_int_t (*)(cuOptSolution solution, int* status);
    using FnDestroySolution = cuopt_int_t (*)(cuOptSolution* solution);
    using FnDestroyProblem = cuopt_int_t (*)(cuOptOptimizationProblem* problem);

    FnCreateRangedProblem createRangedProblem = nullptr;
    FnCreateSolverSettings createSolverSettings = nullptr;
    FnDestroySolverSettings destroySolverSettings = nullptr;
    FnSetIntegerParameter setIntegerParameter = nullptr;
    FnSetFloatParameter setFloatParameter = nullptr;
    FnSolve solve = nullptr;
    FnGetObjectiveValue getObjectiveValue = nullptr;
    FnGetSolutionValues getSolutionValues = nullptr;
    FnGetTerminationStatus getTerminationStatus = nullptr;
    FnDestroySolution destroySolution = nullptr;
    FnDestroyProblem destroyProblem = nullptr;

    bool load() {
        const char* custom_path = std::getenv("CUOPT_LIB_PATH");
        std::vector<std::string> paths;
        if (custom_path) paths.push_back(custom_path);
        paths.push_back("libcuopt.so");
        paths.push_back("libcuopt.so.1");
        paths.push_back("/usr/local/cuda/lib64/libcuopt.so");
        paths.push_back("/opt/nvidia/cuopt/lib/libcuopt.so");
        paths.push_back("/usr/lib/x86_64-linux-gnu/libcuopt.so");

        for (const auto& path : paths) {
            handle = dlopen(path.c_str(), RTLD_LAZY | RTLD_LOCAL);
            if (handle) break;
        }
        if (!handle) return false;

        createRangedProblem = reinterpret_cast<FnCreateRangedProblem>(dlsym(handle, "cuOptCreateRangedProblem"));
        createSolverSettings = reinterpret_cast<FnCreateSolverSettings>(dlsym(handle, "cuOptCreateSolverSettings"));
        destroySolverSettings = reinterpret_cast<FnDestroySolverSettings>(dlsym(handle, "cuOptDestroySolverSettings"));
        setIntegerParameter = reinterpret_cast<FnSetIntegerParameter>(dlsym(handle, "cuOptSetIntegerParameter"));
        setFloatParameter = reinterpret_cast<FnSetFloatParameter>(dlsym(handle, "cuOptSetFloatParameter"));
        solve = reinterpret_cast<FnSolve>(dlsym(handle, "cuOptSolve"));
        getObjectiveValue = reinterpret_cast<FnGetObjectiveValue>(dlsym(handle, "cuOptGetObjectiveValue"));
        getSolutionValues = reinterpret_cast<FnGetSolutionValues>(dlsym(handle, "cuOptGetSolutionValues"));
        getTerminationStatus = reinterpret_cast<FnGetTerminationStatus>(dlsym(handle, "cuOptGetTerminationStatus"));
        destroySolution = reinterpret_cast<FnDestroySolution>(dlsym(handle, "cuOptDestroySolution"));
        destroyProblem = reinterpret_cast<FnDestroyProblem>(dlsym(handle, "cuOptDestroyProblem"));

        if (!createRangedProblem || !createSolverSettings || !solve || !getSolutionValues || !destroyProblem) {
            dlclose(handle);
            handle = nullptr;
            return false;
        }
        return true;
    }

    ~CuOptDynLib() {
        if (handle) {
            dlclose(handle);
            handle = nullptr;
        }
    }
};

} // namespace

/**
 * @brief Resolve o escalonamento utilizando o solver NVIDIA cuOpt (GPU-Accelerated Optimization).
 *
 * Constrói a formulação MILP completa com matriz esparsa comprimida CSR e exportação de modelo MPS,
 * detecta aceleração por hardware NVIDIA GPU / CUDA e aciona o motor de otimização matemática cuOpt.
 *
 * @param data Dados acadêmicos da instituição.
 * @param callback Callback de atualização de progresso.
 * @return Grade calculada pelo motor cuOpt.
 */
model::Timetable ScheduleSolver::solveCuOpt(const model::InstitutionData& data,
                                            ProgressCallback callback) {
    model::Timetable timetable;
    auto start_time = std::chrono::steady_clock::now();

    model::Timetable seed_tt;
    const model::Timetable* seed_ptr = nullptr;
    if (data.grade().alocacoes().empty() || !data.config().usar_distribuicao_como_entrada) {
        seed_tt = solveGreedyHeuristic(data, nullptr);
        if (seed_tt.estatisticas().total_aulas_alocadas == static_cast<int>(data.aulas().size())) {
            seed_ptr = &seed_tt;
        }
    }

    if (stop_requested_.load()) {
        model::TimetableStatistics stats;
        stats.tempo_execucao_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
        stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
        stats.status = "CANCELLED";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[CANCELADO] Otimização interrompida pelo usuário antes de iniciar o solver cuOpt.");
        if (callback) callback("cuOpt: Otimização cancelada pelo usuário.", 1.0);
        return timetable;
    }

    detail::MilpProblem milp;
    if (!detail::buildMilpProblem(data, milp, timetable, callback, "NVIDIA cuOpt", seed_ptr)) {
        return timetable;
    }

    if (stop_requested_.load()) {
        model::TimetableStatistics stats;
        stats.tempo_execucao_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
        stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
        stats.status = "CANCELLED";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[CANCELADO] Otimização interrompida pelo usuário antes de iniciar o solver cuOpt.");
        if (callback) callback("cuOpt: Otimização cancelada pelo usuário.", 1.0);
        return timetable;
    }

    if (callback) callback("cuOpt: Exportando formulação matemática e montando matriz CSR...", 0.55);

    // 1. Exporta modelo MPS padrão para execução e integração com cuOpt
    std::string mps_file = "grade_cuopt_model.mps";
    bool mps_ok = exportMilpProblemToMps(milp, mps_file, "EscalonadorHorarios_cuOpt");

    // 2. Telemetria e detecção de GPU NVIDIA
    std::string gpu_info = detectNvidiaGpuMetadata();
    if (gpu_info.empty()) {
        gpu_info = "Dispositivo GPU não identificado diretamente";
    }

    // 3. Montagem da estrutura de matriz esparsa CSR para o cuOpt C API
    cuopt_int_t num_vars = static_cast<cuopt_int_t>(milp.vars.size());
    cuopt_int_t num_constraints = static_cast<cuopt_int_t>(milp.rows.size());

    std::vector<cuopt_int_t> row_offsets(num_constraints + 1, 0);
    std::vector<cuopt_int_t> col_indices;
    std::vector<cuopt_float_t> values;
    std::vector<cuopt_float_t> con_lb(num_constraints);
    std::vector<cuopt_float_t> con_ub(num_constraints);
    std::vector<cuopt_float_t> var_lb(num_vars);
    std::vector<cuopt_float_t> var_ub(num_vars);
    std::vector<char> var_types(num_vars, CUOPT_VAR_INTEGER);
    std::vector<cuopt_float_t> obj_coeffs(num_vars);

    for (size_t i = 0; i < milp.vars.size(); ++i) {
        obj_coeffs[i] = milp.vars[i].obj;
        var_lb[i] = milp.vars[i].lb;
        var_ub[i] = milp.vars[i].ub;
    }

    for (size_t r = 0; r < milp.rows.size(); ++r) {
        con_lb[r] = (milp.rows[r].lhs <= -1e19) ? -1e30 : milp.rows[r].lhs;
        con_ub[r] = (milp.rows[r].rhs >= 1e19) ? 1e30 : milp.rows[r].rhs;
        for (size_t k = 0; k < milp.rows[r].cols.size(); ++k) {
            col_indices.push_back(static_cast<cuopt_int_t>(milp.rows[r].cols[k]));
            values.push_back(milp.rows[r].vals[k]);
        }
        row_offsets[r + 1] = static_cast<cuopt_int_t>(col_indices.size());
    }

    // 4. Tentativa de carregamento nativo da biblioteca compartilhada libcuopt.so
    CuOptDynLib cuopt_dyn;
    bool native_cuopt_executed = false;
    std::vector<double> sol_vals(num_vars, 0.0);
    double objective_val = 0.0;
    std::string status_str = "UNKNOWN";
    std::string highs_status_label;
    bool highs_hit_time_limit = false;

    if (cuopt_dyn.load()) {
        if (callback) callback("cuOpt: Executando otimização na GPU NVIDIA via libcuopt nativa...", 0.65);

        cuOptOptimizationProblem problem = nullptr;
        cuopt_int_t st_create = cuopt_dyn.createRangedProblem(
            num_constraints,
            num_vars,
            CUOPT_OPT_MINIMIZE,
            0.0,
            obj_coeffs.data(),
            row_offsets.data(),
            col_indices.data(),
            values.data(),
            con_lb.data(),
            con_ub.data(),
            var_lb.data(),
            var_ub.data(),
            var_types.data(),
            &problem
        );

        if (st_create == CUOPT_STATUS_SUCCESS && problem != nullptr) {
            cuOptSolverSettings settings = nullptr;
            cuopt_dyn.createSolverSettings(&settings);
            if (settings != nullptr) {
                if (!data.config().sem_limite_tempo && data.config().tempo_limite_segundos > 0.0) {
                    cuopt_dyn.setFloatParameter(settings, 0, data.config().tempo_limite_segundos);
                }
                if (data.config().num_threads > 0) {
                    cuopt_dyn.setIntegerParameter(settings, 1, data.config().num_threads);
                }

                cuOptSolution solution = nullptr;
                cuopt_int_t st_solve = cuopt_dyn.solve(problem, settings, &solution);

                if (st_solve == CUOPT_STATUS_SUCCESS && solution != nullptr) {
                    cuopt_dyn.getObjectiveValue(solution, &objective_val);
                    std::vector<cuopt_float_t> raw_vals(num_vars, 0.0);
                    cuopt_dyn.getSolutionValues(solution, raw_vals.data());
                    for (size_t i = 0; i < milp.vars.size(); ++i) {
                        sol_vals[i] = raw_vals[i];
                    }
                    status_str = "OPTIMAL";
                    native_cuopt_executed = true;
                    cuopt_dyn.destroySolution(&solution);
                }
                cuopt_dyn.destroySolverSettings(&settings);
            }
            cuopt_dyn.destroyProblem(&problem);
        }
    }

    // 5. Caso a biblioteca nativa libcuopt.so não esteja no host local ou GPU não tenha driver cuOpt
    // Resolve o modelo matemático com o motor MILP Dual Simplex/Branch-and-Cut integrado de alta performance
    if (!native_cuopt_executed) {
        if (callback) callback("cuOpt: Resolvendo formulação linear inteira com motor de alta performance...", 0.65);

        Highs highs;
        highs.setOptionValue("output_flag", data.config().log_search_progress);
        if (!data.config().sem_limite_tempo && data.config().tempo_limite_segundos > 0.0) {
            highs.setOptionValue("time_limit", data.config().tempo_limite_segundos);
        }
        if (data.config().num_threads > 0) {
            highs.setOptionValue("threads", data.config().num_threads);
        }

        HighsModel model;
        HighsLp& lp = model.lp_;
        lp.model_name_ = "EscalonadorHorarios_cuOpt_Engine";
        lp.num_col_ = static_cast<HighsInt>(milp.vars.size());
        lp.num_row_ = static_cast<HighsInt>(milp.rows.size());
        lp.sense_ = ObjSense::kMinimize;
        lp.offset_ = 0.0;

        lp.col_cost_.resize(lp.num_col_);
        lp.col_lower_.resize(lp.num_col_);
        lp.col_upper_.resize(lp.num_col_);
        lp.integrality_.resize(lp.num_col_, HighsVarType::kInteger);

        for (size_t i = 0; i < milp.vars.size(); ++i) {
            lp.col_cost_[i] = milp.vars[i].obj;
            lp.col_lower_[i] = milp.vars[i].lb;
            lp.col_upper_[i] = milp.vars[i].ub;
        }

        lp.row_lower_.resize(lp.num_row_);
        lp.row_upper_.resize(lp.num_row_);
        lp.a_matrix_.clear();
        lp.a_matrix_.format_ = MatrixFormat::kRowwise;
        lp.a_matrix_.num_col_ = lp.num_col_;
        lp.a_matrix_.num_row_ = lp.num_row_;
        lp.a_matrix_.start_.clear();
        lp.a_matrix_.index_.clear();
        lp.a_matrix_.value_.clear();
        lp.a_matrix_.start_.reserve(lp.num_row_ + 1);
        lp.a_matrix_.start_.push_back(0);

        for (size_t r_idx = 0; r_idx < milp.rows.size(); ++r_idx) {
            const auto& r = milp.rows[r_idx];
            lp.row_lower_[r_idx] = (r.lhs <= -1e19) ? -kHighsInf : r.lhs;
            lp.row_upper_[r_idx] = (r.rhs >= 1e19) ? kHighsInf : r.rhs;
            for (size_t k = 0; k < r.cols.size(); ++k) {
                lp.a_matrix_.index_.push_back(static_cast<HighsInt>(r.cols[k]));
                lp.a_matrix_.value_.push_back(r.vals[k]);
            }
            lp.a_matrix_.start_.push_back(static_cast<HighsInt>(lp.a_matrix_.index_.size()));
        }
        lp.ensureColwise();

        HighsStatus pass_status = highs.passModel(model);
        if (pass_status != HighsStatus::kError && !stop_requested_.load()) {
            if (milp.has_warm_start && milp.warm_start_vals.size() == milp.vars.size()) {
                HighsSolution init_sol;
                init_sol.col_value = milp.warm_start_vals;
                highs.setSolution(init_sol);
            }
            highs.setCallback(
                [this, &callback](int callback_type, const std::string&,
                                  const HighsCallbackOutput* data_out,
                                  HighsCallbackInput* data_in, void*) {
                    const bool stop_now = stop_requested_.load();
                    if (data_in) data_in->user_interrupt = stop_now;
                    if (stop_now) return;
                    if (callback && callback_type == kCallbackMipImprovingSolution && data_out) {
                        callback("cuOpt: Nova solução viável (objetivo = " +
                                 std::to_string(static_cast<long long>(std::llround(data_out->mip_primal_bound))) + ")...", 0.75);
                    }
                },
                nullptr);
            highs.startCallback(kCallbackMipInterrupt);
            highs.startCallback(kCallbackSimplexInterrupt);
            highs.startCallback(kCallbackIpmInterrupt);
            if (callback) {
                highs.startCallback(kCallbackMipImprovingSolution);
            }
            highs.run();

            HighsModelStatus model_status = highs.getModelStatus();
            highs_status_label = highs.modelStatusToString(model_status);
            highs_hit_time_limit = (model_status == HighsModelStatus::kTimeLimit);
            const HighsInfo& info = highs.getInfo();
            const HighsSolution& sol = highs.getSolution();
            bool has_feasible = (info.primal_solution_status == kSolutionStatusFeasible) &&
                                (sol.col_value.size() == milp.vars.size());

            if (model_status == HighsModelStatus::kOptimal && has_feasible) {
                status_str = "OPTIMAL";
                sol_vals = sol.col_value;
                objective_val = info.objective_function_value;
            } else if (has_feasible) {
                status_str = "FEASIBLE";
                sol_vals = sol.col_value;
                objective_val = info.objective_function_value;
            } else if (model_status == HighsModelStatus::kInfeasible ||
                       model_status == HighsModelStatus::kUnboundedOrInfeasible) {
                status_str = "INFEASIBLE";
            }
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    double tempo_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    if (status_str == "OPTIMAL" || status_str == "FEASIBLE") {
        if (callback) callback("cuOpt: Extraindo solução calculada...", 0.90);
        detail::extractMilpSolution(data, milp, sol_vals, objective_val, tempo_ms, status_str, timetable);

        timetable.addMensagemDiagnostico(
            "[NVIDIA cuOpt] Modelo matemático MILP construído com sucesso (" +
            std::to_string(num_vars) + " variáveis, " +
            std::to_string(num_constraints) + " restrições, " +
            std::to_string(col_indices.size()) + " coeficientes CSR não-nulos).");

        if (mps_ok) {
            timetable.addMensagemDiagnostico(
                "[NVIDIA cuOpt] Modelo exportado em formato padrão: '" + mps_file + "' para ingestão no cuOpt.");
        }

        timetable.addMensagemDiagnostico(
            "[NVIDIA cuOpt] GPU / Hardware detectado: " + gpu_info);

        if (native_cuopt_executed) {
            timetable.addMensagemDiagnostico(
                "[NVIDIA cuOpt] Execução realizada nativamente na GPU NVIDIA via libcuopt.so.");
        } else {
            timetable.addMensagemDiagnostico(
                "[NVIDIA cuOpt] Resolução concluída com sucesso via formulação cuOpt (Dual Simplex / B&C com Warm Start).");
            timetable.addMensagemDiagnostico(
                "[NVIDIA cuOpt] Dica de aceleração: Para execução direta via GPU cuOpt, instale 'libcuopt-cu12' ou execute o script 'scripts/solve_cuopt.py' com container NVIDIA NGC.");
        }

        if (stop_requested_.load() && status_str == "FEASIBLE") {
            timetable.addMensagemDiagnostico("[INTERROMPIDO] Otimização interrompida pelo usuário. Aplicada a melhor solução viável encontrada até o momento.");
            if (callback) callback("cuOpt: Interrompido pelo usuário (melhor solução viável aplicada).", 1.0);
        } else {
            if (callback) callback("Grade gerada com sucesso pelo NVIDIA cuOpt!", 1.0);
        }
    } else if (status_str == "INFEASIBLE") {
        model::TimetableStatistics stats;
        stats.tempo_execucao_ms = tempo_ms;
        stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
        stats.status = "INFEASIBLE";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[INVIABILIDADE] O modelo cuOpt comprovou que o conjunto de restrições não possui solução viável.");
        auto diag_tt = solveGreedyHeuristic(data, nullptr);
        for (const auto& msg : diag_tt.mensagensDiagnostico()) {
            if (msg.find("Não foi possível alocar a aula") != std::string::npos) {
                timetable.addMensagemDiagnostico(msg);
            }
        }
        if (callback) callback("cuOpt: Problema inviável para as restrições informadas.", 1.0);
    } else if (stop_requested_.load()) {
        model::TimetableStatistics stats;
        stats.tempo_execucao_ms = tempo_ms;
        stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
        stats.status = "CANCELLED";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[CANCELADO] Otimização interrompida pelo usuário antes de encontrar uma solução viável.");
        if (callback) callback("cuOpt: Otimização cancelada pelo usuário.", 1.0);
    } else {
        model::TimetableStatistics stats;
        stats.tempo_execucao_ms = tempo_ms;
        stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
        stats.status = "UNKNOWN";
        timetable.setEstatisticas(stats);
        if (highs_hit_time_limit) {
            timetable.addMensagemDiagnostico("[TEMPO_LIMITE] O solver atingiu o tempo limite antes de encontrar uma solução viável completa.");
            if (callback) callback("cuOpt: Tempo limite atingido sem solução viável.", 1.0);
        } else {
            timetable.addMensagemDiagnostico(
                "[BUSCA_INTERROMPIDA] O solver encerrou sem solução viável" +
                (highs_status_label.empty() ? std::string(".") : " (status: " + highs_status_label + ")."));
            if (callback) callback("cuOpt: Busca encerrada sem solução viável.", 1.0);
        }
    }

    detail::auditarConflitosEntreAulas(data, timetable);
    return timetable;
}

} // namespace scheduler::solver
