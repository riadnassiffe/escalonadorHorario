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
 * @brief Representa uma combinação admissível pré-processada de dia, horário e salas para uma aula.
 *
 * Gerada na fase de pré-processamento respeitando a jornada da turma, disponibilidade de professores,
 * turnos e compatibilidade de salas físicas. É a unidade de decisão booleana utilizada no modelo de otimização.
 */
struct CandidateAllocation {
    std::string aula_id;            ///< Identificador único da ocorrência de aula (ex: "T01_MAT_1").
    model::DayOfWeek dia{model::DayOfWeek::SEG}; ///< Dia da semana proposto para a aula.
    int start_slot{0};               ///< Slot inicial proposto (0 a 287).
    int duration_slots{0};           ///< Duração da aula em slots de 5 minutos.
    int duration_minutes{0};         ///< Duração total da aula em minutos.
    std::vector<std::string> salas;  ///< Conjunto de salas físicas atribuídas (seção 2.1).
    int custo_preferencia{0};        ///< Penalidade horária associada a este slot (seção 4.3).

    /**
     * @brief Retorna o slot final da alocação candidata.
     * @return Índice do slot final (exclusivo).
     */
    int end_slot() const noexcept { return start_slot + duration_slots; }

    /**
     * @brief Retorna o intervalo temporal da alocação candidata.
     * @return TimeInterval correspondente a [start_slot, end_slot).
     */
    model::TimeInterval intervalo() const noexcept {
        return model::TimeInterval(start_slot, end_slot());
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
