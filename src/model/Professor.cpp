/**
 * @file Professor.cpp
 * @brief Implementação dos métodos da classe Professor.
 */

#include "model/Professor.h"

namespace scheduler::model {

/**
 * @brief Inicializa as configurações padrão do professor (pesos neutros e turnos disponíveis).
 */
void Professor::initDefaults() {
    for (size_t i = 0; i < 5; ++i) {
        peso_dias_[i] = 5; // Valor neutro padrão (seção 2.3)
    }
    peso_janela_ = 8;
    for (auto d : allDays()) {
        turnos_pretendidos_[d] = allWorkShifts();
    }
}

/**
 * @brief Verifica se o professor está disponível ao longo de todo o intervalo [start_min, end_min).
 * @param d Dia da semana verificado.
 * @param start_min Minuto inicial do intervalo.
 * @param end_min Minuto final do intervalo.
 * @return true se houver algum intervalo de disponibilidade que contenha totalmente [start_min, end_min).
 */
bool Professor::isDisponivelNoIntervalo(DayOfWeek d, int start_min, int end_min) const {
    auto it = disponibilidade_.find(d);
    if (it == disponibilidade_.end() || it->second.empty()) {
        return false; // Sem disponibilidade cadastrada para o dia
    }

    TimeInterval target(start_min, end_min);
    for (const auto& interval : it->second) {
        if (interval.contains(target)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Retorna os turnos de trabalho pretendidos pelo professor no dia especificado.
 * @param d Dia da semana consultado.
 * @return Referência constante para o vetor de ShiftType do dia (ou vetor vazio se não houver).
 */
const std::vector<ShiftType>& Professor::turnosPretendidos(DayOfWeek d) const {
    static const std::vector<ShiftType> empty_shifts;
    auto it = turnos_pretendidos_.find(d);
    if (it != turnos_pretendidos_.end()) {
        return it->second;
    }
    return empty_shifts;
}

/**
 * @brief Verifica se o professor deseja lecionar no turno indicado para o dia especificado.
 * @param d Dia da semana consultado.
 * @param shift Turno consultado.
 * @return true se o turno estiver entre os pretendidos ou se não houver restrição registrada.
 */
bool Professor::pretendeTurno(DayOfWeek d, ShiftType shift) const {
    auto it = turnos_pretendidos_.find(d);
    if (it == turnos_pretendidos_.end() || it->second.empty()) {
        return true; // Se não houver preferência explicitada, considera apto
    }
    for (auto s : it->second) {
        if (s == shift) return true;
    }
    return false;
}

/**
 * @brief Define os turnos pretendidos pelo professor para determinado dia.
 * @param d Dia da semana a ser configurado.
 * @param turnos Vetor de turnos pretendidos.
 */
void Professor::setTurnosPretendidos(DayOfWeek d, std::vector<ShiftType> turnos) {
    turnos_pretendidos_[d] = std::move(turnos);
}

/**
 * @brief Adiciona um turno pretendido para o dia especificado (evitando duplicatas).
 * @param d Dia da semana a ser configurado.
 * @param shift Turno a ser adicionado.
 */
void Professor::addTurnoPretendido(DayOfWeek d, ShiftType shift) {
    auto& v = turnos_pretendidos_[d];
    for (auto s : v) {
        if (s == shift) return;
    }
    v.push_back(shift);
}

/**
 * @brief Remove todas as preferências de turnos cadastradas para o dia especificado.
 * @param d Dia da semana a ser limpo.
 */
void Professor::clearTurnosPretendidos(DayOfWeek d) {
    turnos_pretendidos_.erase(d);
}

} // namespace scheduler::model
