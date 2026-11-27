/**
 * @file HighsSolver.cpp
 * @brief Implementação do algoritmo de escalonamento via solver nativo HiGHS.
 */

#include "solver/ScheduleSolver.h"
#include "solver/MilpModel.h"
#include "solver/SolverCommon.h"
#include <Highs.h>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

namespace scheduler::solver {

/**
 * @brief Resolve o escalonamento utilizando o solver nativo HiGHS (High Performance Optimization Software).
 *
 * Constrói a matriz esparsa comprimida (CSR) do problema linear inteiro misto na API C++ do HiGHS,
 * aplicando pré-processamento algébrico, planos de corte, Dual Simplex e Branch-and-Cut paralelo.
 *
 * @param data Dados acadêmicos da instituição.
 * @param callback Callback opcional para notificação de progresso.
 * @return Grade horária gerada pelo HiGHS.
 */
model::Timetable ScheduleSolver::solveHighs(const model::InstitutionData& data,
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
        timetable.addMensagemDiagnostico("[CANCELADO] Otimização interrompida pelo usuário antes de iniciar o solver HiGHS.");
        if (callback) callback("HiGHS: Otimização cancelada pelo usuário.", 1.0);
        return timetable;
    }

    detail::MilpProblem milp;
    if (!detail::buildMilpProblem(data, milp, timetable, callback, "HiGHS", seed_ptr)) {
        return timetable;
    }

    if (callback) callback("HiGHS: Montando matriz esparsa e inicializando MILP...", 0.55);

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
    lp.model_name_ = "EscalonadorHorarios_HiGHS";
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
    if (pass_status == HighsStatus::kError) {
        model::TimetableStatistics stats;
        stats.status = "UNKNOWN";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[ERRO] Falha ao transferir o modelo linear para o motor HiGHS.");
        return timetable;
    }

    if (stop_requested_.load()) {
        model::TimetableStatistics stats;
        stats.tempo_execucao_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
        stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
        stats.status = "CANCELLED";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[CANCELADO] Otimização interrompida pelo usuário antes de iniciar o solver HiGHS.");
        if (callback) callback("HiGHS: Otimização cancelada pelo usuário.", 1.0);
        return timetable;
    }

    // Injeção de Solução Inicial (Warm Start) no HiGHS
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
                callback("HiGHS: Nova solução viável (objetivo = " +
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

    if (callback) callback("Executando solver HiGHS (Branch-and-Cut)...", 0.65);

    highs.run();

    auto end_time = std::chrono::steady_clock::now();
    double tempo_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    HighsModelStatus model_status = highs.getModelStatus();
    const HighsInfo& info = highs.getInfo();
    const HighsSolution& sol = highs.getSolution();
    bool has_feasible_sol = (info.primal_solution_status == kSolutionStatusFeasible) &&
                            (sol.col_value.size() == milp.vars.size());

    std::string status_str = "UNKNOWN";
    if (model_status == HighsModelStatus::kOptimal && has_feasible_sol) {
        status_str = "OPTIMAL";
    } else if (has_feasible_sol) {
        status_str = "FEASIBLE";
    } else if (model_status == HighsModelStatus::kInfeasible ||
               model_status == HighsModelStatus::kUnboundedOrInfeasible) {
        status_str = "INFEASIBLE";
    }

    if (status_str == "OPTIMAL" || status_str == "FEASIBLE") {
        if (callback) callback("HiGHS: Extraindo solução encontrada...", 0.90);
        detail::extractMilpSolution(data, milp, sol.col_value, info.objective_function_value, tempo_ms, status_str, timetable);
        timetable.addMensagemDiagnostico(
            "[HiGHS] Solução obtida via HiGHS v" + std::string(highsVersion()) +
            " (" + std::to_string(milp.vars.size()) + " variáveis, " +
            std::to_string(milp.rows.size()) + " restrições, " +
            std::to_string(info.mip_node_count) + " nós B&C).");
        if (stop_requested_.load() && status_str == "FEASIBLE") {
            timetable.addMensagemDiagnostico("[INTERROMPIDO] Otimização interrompida pelo usuário. Aplicada a melhor solução viável encontrada até o momento.");
            if (callback) callback("HiGHS: Interrompido pelo usuário (melhor solução viável aplicada).", 1.0);
        } else {
            if (callback) callback("Grade gerada com sucesso pelo HiGHS!", 1.0);
        }
    } else if (status_str == "INFEASIBLE") {
        model::TimetableStatistics stats;
        stats.tempo_execucao_ms = tempo_ms;
        stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
        stats.status = "INFEASIBLE";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[INVIABILIDADE] O solver HiGHS comprovou que o conjunto de restrições não possui solução viável.");
        auto diag_tt = solveGreedyHeuristic(data, nullptr);
        for (const auto& msg : diag_tt.mensagensDiagnostico()) {
            if (msg.find("Não foi possível alocar a aula") != std::string::npos) {
                timetable.addMensagemDiagnostico(msg);
            }
        }
        if (callback) callback("HiGHS: Problema inviável para as restrições informadas.", 1.0);
    } else if (stop_requested_.load()) {
        model::TimetableStatistics stats;
        stats.tempo_execucao_ms = tempo_ms;
        stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
        stats.status = "CANCELLED";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[CANCELADO] Otimização interrompida pelo usuário antes de encontrar uma solução viável.");
        if (callback) callback("HiGHS: Otimização cancelada pelo usuário.", 1.0);
    } else {
        model::TimetableStatistics stats;
        stats.tempo_execucao_ms = tempo_ms;
        stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
        stats.status = "UNKNOWN";
        timetable.setEstatisticas(stats);
        if (model_status == HighsModelStatus::kTimeLimit) {
            timetable.addMensagemDiagnostico("[TEMPO_LIMITE] O solver HiGHS atingiu o tempo limite antes de encontrar uma solução viável completa.");
            if (callback) callback("HiGHS: Tempo limite atingido sem solução viável.", 1.0);
        } else {
            timetable.addMensagemDiagnostico(
                "[BUSCA_INTERROMPIDA] O solver HiGHS encerrou sem solução viável (status: " +
                highs.modelStatusToString(model_status) + ").");
            if (callback) callback("HiGHS: Busca encerrada sem solução viável.", 1.0);
        }
    }

    detail::auditarConflitosEntreAulas(data, timetable);
    return timetable;
}

} // namespace scheduler::solver
