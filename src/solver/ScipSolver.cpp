/**
 * @file ScipSolver.cpp
 * @brief Implementação do algoritmo de escalonamento via solver nativo SCIP.
 */

#include "solver/ScheduleSolver.h"
#include "solver/MilpModel.h"
#include "solver/SolverCommon.h"
#include <scip/scip.h>
#include <scip/scipdefplugins.h>
#include <chrono>
#include <string>
#include <vector>

namespace scheduler::solver {

/**
 * @brief Resolve o escalonamento utilizando o solver nativo SCIP (Solving Constraint Integer Programs).
 *
 * Constrói um programa linear inteiro misto / por restrições completo na API nativa do SCIP,
 * com suporte a cortes de clique por slot, salas teóricas fixas, janelas e Warm Start.
 *
 * @param data Dados acadêmicos da instituição.
 * @param callback Callback opcional para notificação de progresso.
 * @return Grade horária gerada pelo SCIP.
 */
model::Timetable ScheduleSolver::solveScip(const model::InstitutionData& data,
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

    detail::MilpProblem milp;
    if (!detail::buildMilpProblem(data, milp, timetable, callback, "SCIP", seed_ptr)) {
        return timetable;
    }

    if (callback) callback("SCIP: Inicializando motor SCIP (Branch-Cut-and-Price)...", 0.55);

    SCIP* scip = nullptr;
    if (SCIPcreate(&scip) != SCIP_OKAY || scip == nullptr) {
        model::TimetableStatistics stats;
        stats.status = "UNKNOWN";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[ERRO] Não foi possível inicializar a instância nativa do SCIP.");
        return timetable;
    }

    SCIPincludeDefaultPlugins(scip);
    SCIPcreateProbBasic(scip, "EscalonadorHorarios_SCIP");
    SCIPsetObjsense(scip, SCIP_OBJSENSE_MINIMIZE);

    // Parâmetros de execução do SCIP
    SCIPsetIntParam(scip, "display/verblevel", data.config().log_search_progress ? 4 : 0);
    if (!data.config().sem_limite_tempo && data.config().tempo_limite_segundos > 0.0) {
        SCIPsetRealParam(scip, "limits/time", data.config().tempo_limite_segundos);
    }
    if (!data.config().sem_limite_memoria && data.config().max_memoria_mb > 0) {
        SCIPsetRealParam(scip, "limits/memory", static_cast<SCIP_Real>(data.config().max_memoria_mb));
    }
    if (data.config().num_threads > 0) {
        SCIPsetIntParam(scip, "parallel/maxnthreads", data.config().num_threads);
    }

    // Criação das variáveis binárias no SCIP
    std::vector<SCIP_VAR*> scip_vars(milp.vars.size(), nullptr);
    for (size_t i = 0; i < milp.vars.size(); ++i) {
        const auto& v = milp.vars[i];
        SCIPcreateVarBasic(scip, &scip_vars[i], v.name.c_str(), v.lb, v.ub, v.obj, SCIP_VARTYPE_BINARY);
        SCIPaddVar(scip, scip_vars[i]);
    }

    // Adição das restrições lineares no SCIP
    for (const auto& r : milp.rows) {
        if (r.cols.empty()) continue;
        std::vector<SCIP_VAR*> row_vars(r.cols.size());
        std::vector<SCIP_Real> row_vals(r.vals.begin(), r.vals.end());
        for (size_t j = 0; j < r.cols.size(); ++j) {
            row_vars[j] = scip_vars[r.cols[j]];
        }
        SCIP_Real lhs = (r.lhs <= -1e19) ? -SCIPinfinity(scip) : r.lhs;
        SCIP_Real rhs = (r.rhs >= 1e19) ? SCIPinfinity(scip) : r.rhs;

        SCIP_CONS* cons = nullptr;
        SCIPcreateConsBasicLinear(scip, &cons, r.name.c_str(),
                                  static_cast<int>(row_vars.size()), row_vars.data(), row_vals.data(),
                                  lhs, rhs);
        SCIPaddCons(scip, cons);
        SCIPreleaseCons(scip, &cons);
    }

    // Injeção de Solução Inicial (Warm Start) no SCIP (estágio PROBLEM -> SCIPaddSolFree)
    if (milp.has_warm_start && milp.warm_start_vals.size() == scip_vars.size()) {
        SCIP_SOL* init_sol = nullptr;
        if (SCIPcreateSol(scip, &init_sol, nullptr) == SCIP_OKAY && init_sol != nullptr) {
            for (size_t i = 0; i < scip_vars.size(); ++i) {
                SCIPsetSolVal(scip, init_sol, scip_vars[i], milp.warm_start_vals[i]);
            }
            SCIP_Bool stored = FALSE;
            SCIPaddSolFree(scip, &init_sol, &stored);
        }
    }

    if (callback) callback("Executando solver SCIP...", 0.65);

    SCIPsolve(scip);

    auto end_time = std::chrono::steady_clock::now();
    double tempo_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    SCIP_STATUS scip_status = SCIPgetStatus(scip);
    SCIP_SOL* best_sol = SCIPgetBestSol(scip);
    long long n_nodes = SCIPgetNNodes(scip);

    std::string status_str = "UNKNOWN";
    if (scip_status == SCIP_STATUS_OPTIMAL) {
        status_str = "OPTIMAL";
    } else if (best_sol != nullptr) {
        status_str = "FEASIBLE";
    } else if (scip_status == SCIP_STATUS_INFEASIBLE) {
        status_str = "INFEASIBLE";
    }

    if (status_str == "OPTIMAL" || status_str == "FEASIBLE") {
        if (callback) callback("SCIP: Extraindo solução encontrada...", 0.90);
        std::vector<double> sol_vals(scip_vars.size(), 0.0);
        for (size_t i = 0; i < scip_vars.size(); ++i) {
            sol_vals[i] = SCIPgetSolVal(scip, best_sol, scip_vars[i]);
        }
        double obj_val = SCIPgetSolOrigObj(scip, best_sol);
        detail::extractMilpSolution(data, milp, sol_vals, obj_val, tempo_ms, status_str, timetable);
        timetable.addMensagemDiagnostico(
            "[SCIP] Solução obtida via SCIP v" + std::to_string(SCIPmajorVersion()) + "." +
            std::to_string(SCIPminorVersion()) + "." + std::to_string(SCIPtechVersion()) +
            " (" + std::to_string(milp.vars.size()) + " variáveis, " +
            std::to_string(milp.rows.size()) + " restrições, " +
            std::to_string(n_nodes) + " nós B&B).");
        if (callback) callback("Grade gerada com sucesso pelo SCIP!", 1.0);
    } else if (status_str == "INFEASIBLE") {
        model::TimetableStatistics stats;
        stats.tempo_execucao_ms = tempo_ms;
        stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
        stats.status = "INFEASIBLE";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[INVIABILIDADE] O solver SCIP comprovou que o conjunto de restrições não possui solução viável.");
        auto diag_tt = solveGreedyHeuristic(data, nullptr);
        for (const auto& msg : diag_tt.mensagensDiagnostico()) {
            if (msg.find("Não foi possível alocar a aula") != std::string::npos) {
                timetable.addMensagemDiagnostico(msg);
            }
        }
        if (callback) callback("SCIP: Problema inviável para as restrições informadas.", 1.0);
    } else {
        model::TimetableStatistics stats;
        stats.tempo_execucao_ms = tempo_ms;
        stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
        stats.status = "UNKNOWN";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[TEMPO_LIMITE] O solver SCIP atingiu o limite de tempo/memória antes de encontrar uma solução viável completa.");
        if (callback) callback("SCIP: Limite atingido sem solução viável.", 1.0);
    }

    for (size_t i = 0; i < scip_vars.size(); ++i) {
        if (scip_vars[i] != nullptr) {
            SCIPreleaseVar(scip, &scip_vars[i]);
        }
    }
    SCIPfree(&scip);

    detail::auditarConflitosEntreAulas(data, timetable);
    return timetable;
}

} // namespace scheduler::solver
