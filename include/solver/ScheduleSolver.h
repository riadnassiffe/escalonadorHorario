#pragma once

/**
 * @file ScheduleSolver.h
 * @brief Orquestrador dos algoritmos de escalonamento automático de horários.
 */

#include "model/InstitutionData.h"
#include "model/Timetable.h"
#include "solver/Preprocessor.h"
#include <atomic>
#include <functional>
#include <string>

namespace scheduler::solver {

/**
 * @brief Callback para atualização de progresso da otimização para a interface gráfica.
 * @param status_message Mensagem de status informando a etapa atual.
 * @param progress_ratio Progresso normalizado entre 0.0 (início) e 1.0 (concluído).
 */
using ProgressCallback = std::function<void(const std::string& status_message, double progress_ratio)>;

/**
 * @class ScheduleSolver
 * @brief Motor de otimização responsável por resolver o problema de alocação de horários escolares.
 *
 * Suporta múltiplos motores de resolução configuráveis (Google OR-Tools CP-SAT, Heurística Construtiva Gulosa,
 * MILP e Algoritmo Genético), permitindo cancelamento assíncrono e notificação contínua de progresso.
 */
class ScheduleSolver {
public:
    /**
     * @brief Construtor padrão da classe ScheduleSolver.
     */
    ScheduleSolver() = default;

    /**
     * @brief Executa o escalonamento automático com base no algoritmo e parâmetros configurados.
     * @param data Conjunto de dados e restrições da instituição de ensino.
     * @param callback Função opcional para reportar progresso e status para a UI.
     * @return Timetable contendo a grade horária gerada e as métricas de qualidade.
     */
    model::Timetable solve(const model::InstitutionData& data,
                           ProgressCallback callback = nullptr);

    /**
     * @brief Solicita interrupção antecipada da execução do algoritmo de otimização.
     */
    void requestStop() {
        stop_requested_.store(true);
    }

private:
    /**
     * @brief Resolve o escalonamento utilizando programação por restrições via Google OR-Tools CP-SAT.
     * @param data Dados institucionais e parâmetros.
     * @param callback Callback de atualização de progresso.
     * @return Grade calculada pelo CP-SAT.
     */
    model::Timetable solveCpSat(const model::InstitutionData& data, ProgressCallback callback);

    /**
     * @brief Resolve o escalonamento utilizando algoritmo construtivo guloso de alta performance.
     * @param data Dados institucionais e parâmetros.
     * @param callback Callback de atualização de progresso.
     * @return Grade calculada pela heurística gulosa.
     */
    model::Timetable solveGreedyHeuristic(const model::InstitutionData& data, ProgressCallback callback);

    /**
     * @brief Resolve o escalonamento utilizando formulação de Programação Linear Inteira Mista (MILP).
     * @param data Dados institucionais e parâmetros.
     * @param callback Callback de atualização de progresso.
     * @return Grade calculada pelo modelo MILP.
     */
    model::Timetable solveMilp(const model::InstitutionData& data, ProgressCallback callback);

    /**
     * @brief Resolve o escalonamento utilizando meta-heurística de Algoritmo Genético.
     * @param data Dados institucionais e parâmetros.
     * @param callback Callback de atualização de progresso.
     * @return Grade calculada pelo Algoritmo Genético.
     */
    model::Timetable solveGeneticAlgorithm(const model::InstitutionData& data, ProgressCallback callback);

    std::atomic<bool> stop_requested_{false};
};

} // namespace scheduler::solver
