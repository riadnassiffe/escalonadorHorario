#pragma once

#include "model/TimeSlot.h"
#include <string>
#include <vector>

namespace scheduler::solver {

struct CandidateAllocation {
    std::string aula_id;
    model::DayOfWeek dia{model::DayOfWeek::SEG};
    int start_slot{0};
    int duration_slots{0};
    int duration_minutes{0};
    std::vector<std::string> salas; // Conjunto b de salas (seção 2.1)
    int custo_preferencia{0};       // Penalidade horária (seção 4.3)

    int end_slot() const noexcept { return start_slot + duration_slots; }
    model::TimeInterval intervalo() const noexcept {
        return model::TimeInterval(start_slot, end_slot());
    }

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
