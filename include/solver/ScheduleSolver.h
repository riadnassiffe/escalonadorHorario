#pragma once

#include "model/InstitutionData.h"
#include "model/Timetable.h"
#include "solver/Preprocessor.h"
#include <atomic>
#include <functional>
#include <string>

namespace scheduler::solver {

using ProgressCallback = std::function<void(const std::string& status_message, double progress_ratio)>;

class ScheduleSolver {
public:
    ScheduleSolver() = default;

    model::Timetable solve(const model::InstitutionData& data,
                           ProgressCallback callback = nullptr);

    void requestStop() {
        stop_requested_.store(true);
    }

private:
    std::atomic<bool> stop_requested_{false};
};

} // namespace scheduler::solver
