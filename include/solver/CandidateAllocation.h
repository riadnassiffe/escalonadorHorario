#pragma once

/**
 * @file CandidateAllocation.h
 * @brief Definição da estrutura CandidateAllocation, que representa uma alocação viável candidata para uma aula.
 */

#include "model/TimeSlot.h"
#include <string>
#include <vector>

namespace scheduler::solver {

/**
 * @struct CandidateAllocation
 * @brief Representa uma combinação admissível pré-processada de dia, slot horário e salas para uma aula.
 *
 * Gerada na fase de pré-processamento selecionando um slot cadastrado compatível com a duração
 * da aula, respeitando todas as turmas envolvidas, disponibilidade de professores, turnos e salas físicas.
 */
struct CandidateAllocation {
    std::string aula_id;            ///< Identificador único da ocorrência de aula (ex: "T01_MAT_1").
    model::DayOfWeek dia{model::DayOfWeek::SEG}; ///< Dia da semana proposto para a aula.
    int start_min{0};                ///< Minuto inicial do slot proposto (0 a 1439).
    int duration_minutes{0};         ///< Duração exata da aula/slot em minutos.
    std::vector<std::string> salas;  ///< Conjunto de salas físicas atribuídas.
    int custo_preferencia{0};        ///< Penalidade horária associada a este horário.

    /**
     * @brief Retorna o minuto final da alocação candidata (exclusivo).
     * @return Minuto final desde 00:00.
     */
    int end_min() const noexcept { return start_min + duration_minutes; }

    /**
     * @brief Retorna o intervalo temporal da alocação candidata.
     * @return TimeInterval correspondente a [start_min, end_min).
     */
    model::TimeInterval intervalo() const noexcept {
        return model::TimeInterval(start_min, end_min());
    }

    /**
     * @brief Retorna uma descrição textual formatada da alocação para depuração e logs.
     * @return String formatada contendo dia, horário e salas atribuídas.
     */
    std::string descricao() const {
        std::string s = model::dayToString(dia) + " " + intervalo().toString() + " [";
        for (size_t i = 0; i < salas.size(); ++i) {
            if (i > 0) s += ", ";
            s += salas[i];
        }
        s += "]";
        return s;
    }
};

} // namespace scheduler::solver
