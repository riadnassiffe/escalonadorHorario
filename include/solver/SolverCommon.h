#pragma once

/**
 * @file SolverCommon.h
 * @brief Utilitários compartilhados entre os diferentes algoritmos de escalonamento.
 */

#include "model/InstitutionData.h"
#include "model/Timetable.h"

namespace scheduler::solver::detail {

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
void auditarConflitosEntreAulas(const model::InstitutionData& data, model::Timetable& timetable);

} // namespace scheduler::solver::detail
