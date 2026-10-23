#pragma once

#include <string>
#include <vector>
#include <stdexcept>

namespace scheduler::model {

constexpr int SLOT_MINUTES = 5;
constexpr int SLOTS_PER_HOUR = 60 / SLOT_MINUTES; // 12
constexpr int SLOTS_PER_DAY = 24 * SLOTS_PER_HOUR; // 288

enum class DayOfWeek {
    SEG = 0,
    TER = 1,
    QUA = 2,
    QUI = 3,
    SEX = 4
};

std::string dayToString(DayOfWeek day);
std::string dayToLongString(DayOfWeek day);
DayOfWeek stringToDay(const std::string& str);
const std::vector<DayOfWeek>& allDays();

enum class ShiftType {
    MANHA,
    TARDE,
    NOITE,
    INTEGRAL
};

std::string shiftToString(ShiftType shift);
ShiftType stringToShift(const std::string& str);
const std::vector<ShiftType>& allWorkShifts();
ShiftType slotToShift(int slot);

struct TimeInterval {
    int start_slot{0}; // 0-indexed 5-minute interval [start_slot, end_slot)
    int end_slot{0};

    TimeInterval() = default;
    TimeInterval(int start, int end) : start_slot(start), end_slot(end) {
        if (start > end) {
            throw std::invalid_argument("TimeInterval: start_slot cannot be greater than end_slot");
        }
    }
    TimeInterval(const std::string& start_str, const std::string& end_str);

    int durationSlots() const noexcept { return end_slot - start_slot; }
    int durationMinutes() const noexcept { return durationSlots() * SLOT_MINUTES; }

    bool contains(int slot) const noexcept {
        return slot >= start_slot && slot < end_slot;
    }

    bool contains(const TimeInterval& other) const noexcept {
        return other.start_slot >= start_slot && other.end_slot <= end_slot;
    }

    bool overlaps(const TimeInterval& other) const noexcept {
        return start_slot < other.end_slot && other.start_slot < end_slot;
    }

    std::string startString() const;
    std::string endString() const;
    std::string toString() const;

    static int timeStringToSlot(const std::string& time_str);
    static std::string slotToTimeString(int slot);
};

} // namespace scheduler::model
