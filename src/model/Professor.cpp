#include "model/Professor.h"

namespace scheduler::model {

void Professor::initDefaults() {
    for (size_t i = 0; i < 5; ++i) {
        peso_dias_[i] = 5; // Valor neutro padrão (seção 2.3)
    }
    peso_janela_ = 8;
    for (auto d : allDays()) {
        turnos_pretendidos_[d] = allWorkShifts();
    }
}

bool Professor::isDisponivelNoIntervalo(DayOfWeek d, int start_slot, int end_slot) const {
    auto it = disponibilidade_.find(d);
    if (it == disponibilidade_.end() || it->second.empty()) {
        return false; // Sem disponibilidade cadastrada para o dia
    }

    TimeInterval target(start_slot, end_slot);
    for (const auto& interval : it->second) {
        if (interval.contains(target)) {
            return true;
        }
    }
    return false;
}

const std::vector<ShiftType>& Professor::turnosPretendidos(DayOfWeek d) const {
    static const std::vector<ShiftType> empty_shifts;
    auto it = turnos_pretendidos_.find(d);
    if (it != turnos_pretendidos_.end()) {
        return it->second;
    }
    return empty_shifts;
}

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

void Professor::setTurnosPretendidos(DayOfWeek d, std::vector<ShiftType> turnos) {
    turnos_pretendidos_[d] = std::move(turnos);
}

void Professor::addTurnoPretendido(DayOfWeek d, ShiftType shift) {
    auto& v = turnos_pretendidos_[d];
    for (auto s : v) {
        if (s == shift) return;
    }
    v.push_back(shift);
}

void Professor::clearTurnosPretendidos(DayOfWeek d) {
    turnos_pretendidos_.erase(d);
}

} // namespace scheduler::model
