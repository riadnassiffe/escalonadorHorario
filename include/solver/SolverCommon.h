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

/**
 * @brief Calcula o total de pares de aulas consecutivas realizados na grade para disciplinas
 *        configuradas com exigir_aulas_consecutivas = true.
 *
 * Considera consecutivos dois slots adjacentes na jornada ordenada da turma primária
 * dentro do mesmo turno (timeToShift(slot[k].start_min) == timeToShift(slot[k+1].start_min)).
 *
 * @param data Dados acadêmicos da instituição.
 * @param timetable Grade horária avaliada.
 * @return Quantidade total de pares consecutivos atendidos.
 */
int calcularBeneficioConsecutivas(const model::InstitutionData& data, const model::Timetable& timetable);

} // namespace scheduler::solver::detail
