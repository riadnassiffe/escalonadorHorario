#include "model/Professor.h"

namespace scheduler::model {

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

} // namespace scheduler::model
