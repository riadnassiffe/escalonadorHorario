/**
 * @file ScheduleSolver.cpp
 * @brief Orquestrador dos algoritmos de resolução e auditoria pós-otimização de grade horária.
 */

#include "solver/ScheduleSolver.h"
#include "solver/SolverCommon.h"
#include <algorithm>
#include <string>
#include <vector>

namespace scheduler::solver {

/**
 * @brief Resolve o escalonamento chamando o algoritmo configurado na instituição.
 * @param data Conjunto de dados acadêmicos e configuração de otimização.
 * @param callback Callback opcional para notificação de progresso e status.
 * @return Grade horária gerada (Timetable) com as alocações e estatísticas.
 */
model::Timetable ScheduleSolver::solve(const model::InstitutionData& data,
                                       ProgressCallback callback) {
    switch (data.config().algoritmo) {
        case model::SolverAlgorithm::GREEDY_HEURISTIC:
            return solveGreedyHeuristic(data, callback);
        case model::SolverAlgorithm::SCIP:
            return solveScip(data, callback);
        case model::SolverAlgorithm::HIGHS:
            return solveHighs(data, callback);
        case model::SolverAlgorithm::MILP:
            return solveMilp(data, callback);
        case model::SolverAlgorithm::CUOPT:
            return solveCuOpt(data, callback);
        case model::SolverAlgorithm::CP_SAT:
        default:
            return solveCpSat(data, callback);
    }
}

namespace detail {

/**
 * @brief Realiza auditoria pós-resolução para identificar choques de horário.
 *
 * Avalia pares de aulas alocadas no mesmo dia e intervalo de tempo sobreposto,
 * verificando choques de professores, turmas ou salas no mesmo semestre letivo.
 * Em caso de choques, adiciona mensagens de aviso detalhadas no diagnóstico da grade.
 *
 * @param data Dados acadêmicos da instituição.
 * @param timetable Grade horária gerada a ser auditada e complementada com mensagens.
 */
void auditarConflitosEntreAulas(const model::InstitutionData& data, model::Timetable& timetable) {
    const auto& alocs = timetable.alocacoes();
    auto time_overlaps = [](const model::TimeInterval& t1, const model::TimeInterval& t2) {
        return std::max(t1.start_min, t2.start_min) < std::min(t1.end_min, t2.end_min);
    };
    auto semestres_conflitam = [](const model::AllocatedLesson& a1, const model::AllocatedLesson& a2) {
        if (a1.regime == model::RegimeDisciplina::ANUAL || a2.regime == model::RegimeDisciplina::ANUAL) return true;
        int s1 = (a1.semestre == model::SemestreLetivo::SEMESTRE_2 || a1.semestre_alocado == 2) ? 2 : 1;
        int s2 = (a2.semestre == model::SemestreLetivo::SEMESTRE_2 || a2.semestre_alocado == 2) ? 2 : 1;
        return s1 == s2;
    };

    std::vector<std::string> conflitos_detectados;

    for (size_t i = 0; i < alocs.size(); ++i) {
        for (size_t j = i + 1; j < alocs.size(); ++j) {
            const auto& a1 = alocs[i];
            const auto& a2 = alocs[j];
            if (a1.dia == a2.dia && semestres_conflitam(a1, a2) && time_overlaps(a1.intervalo, a2.intervalo)) {
                // Professor
                for (const auto& p1 : a1.professores) {
                    if (std::find(a2.professores.begin(), a2.professores.end(), p1) != a2.professores.end()) {
                        const auto* p = data.findProfessor(p1);
                        conflitos_detectados.push_back("Choque de Professor: Aula '" + a1.disciplina_nome + "' (" + a1.aula_id + ") conflita com aula '" + a2.disciplina_nome + "' (" + a2.aula_id + ") do Prof. " + (p ? p->nome() : p1) + " na " + model::dayToLongString(a1.dia) + " às " + a1.horarioFormatado() + ".");
                    }
                }
                // Turma
                for (const auto& t1 : a1.turmas) {
                    if (std::find(a2.turmas.begin(), a2.turmas.end(), t1) != a2.turmas.end()) {
                        const auto* t = data.findTurma(t1);
                        conflitos_detectados.push_back("Choque de Turma: Aula '" + a1.disciplina_nome + "' (" + a1.aula_id + ") conflita com aula '" + a2.disciplina_nome + "' (" + a2.aula_id + ") da Turma " + (t ? t->nome() : t1) + " na " + model::dayToLongString(a1.dia) + " às " + a1.horarioFormatado() + ".");
                    }
                }
                // Sala
                for (const auto& r1 : a1.salas) {
                    if (std::find(a2.salas.begin(), a2.salas.end(), r1) != a2.salas.end()) {
                        const auto* r = data.findSala(r1);
                        conflitos_detectados.push_back("Choque de Sala: Aula '" + a1.disciplina_nome + "' (" + a1.aula_id + ") conflita com aula '" + a2.disciplina_nome + "' (" + a2.aula_id + ") na Sala " + (r ? r->nome() : r1) + " na " + model::dayToLongString(a1.dia) + " às " + a1.horarioFormatado() + ".");
                    }
                }
            }
        }
    }

    if (!conflitos_detectados.empty()) {
        timetable.addMensagemDiagnostico("[AUDITORIA DE GRADE] Foram detectados " + std::to_string(conflitos_detectados.size()) + " conflito(s) entre aulas alocadas:");
        for (const auto& c : conflitos_detectados) {
            timetable.addMensagemDiagnostico("  ⚠️ " + c);
        }
    }
}

} // namespace detail

} // namespace scheduler::solver
