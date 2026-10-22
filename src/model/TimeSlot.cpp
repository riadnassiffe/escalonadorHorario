#include "model/TimeSlot.h"
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace scheduler::model {

std::string dayToString(DayOfWeek day) {
    switch (day) {
        case DayOfWeek::SEG: return "SEG";
        case DayOfWeek::TER: return "TER";
        case DayOfWeek::QUA: return "QUA";
        case DayOfWeek::QUI: return "QUI";
        case DayOfWeek::SEX: return "SEX";
    }
    return "SEG";
}

std::string dayToLongString(DayOfWeek day) {
    switch (day) {
        case DayOfWeek::SEG: return "Segunda-feira";
        case DayOfWeek::TER: return "Terça-feira";
        case DayOfWeek::QUA: return "Quarta-feira";
        case DayOfWeek::QUI: return "Quinta-feira";
        case DayOfWeek::SEX: return "Sexta-feira";
    }
    return "Segunda-feira";
}

DayOfWeek stringToDay(const std::string& str) {
    if (str == "SEG" || str == "seg" || str == "Segunda" || str == "Segunda-feira") return DayOfWeek::SEG;
    if (str == "TER" || str == "ter" || str == "Terca" || str == "Terça" || str == "Terça-feira") return DayOfWeek::TER;
    if (str == "QUA" || str == "qua" || str == "Quarta" || str == "Quarta-feira") return DayOfWeek::QUA;
    if (str == "QUI" || str == "qui" || str == "Quinta" || str == "Quinta-feira") return DayOfWeek::QUI;
    if (str == "SEX" || str == "sex" || str == "Sexta" || str == "Sexta-feira") return DayOfWeek::SEX;
    return DayOfWeek::SEG;
}

const std::vector<DayOfWeek>& allDays() {
    static const std::vector<DayOfWeek> days = {
        DayOfWeek::SEG,
        DayOfWeek::TER,
        DayOfWeek::QUA,
        DayOfWeek::QUI,
        DayOfWeek::SEX
    };
    return days;
}

std::string shiftToString(ShiftType shift) {
    switch (shift) {
        case ShiftType::MANHA: return "MANHA";
        case ShiftType::TARDE: return "TARDE";
        case ShiftType::NOITE: return "NOITE";
        case ShiftType::INTEGRAL: return "INTEGRAL";
    }
    return "MANHA";
}

ShiftType stringToShift(const std::string& str) {
    if (str == "MANHA" || str == "Manhã" || str == "manha") return ShiftType::MANHA;
    if (str == "TARDE" || str == "Tarde" || str == "tarde") return ShiftType::TARDE;
    if (str == "NOITE" || str == "Noite" || str == "noite") return ShiftType::NOITE;
    if (str == "INTEGRAL" || str == "Integral" || str == "integral") return ShiftType::INTEGRAL;
    return ShiftType::MANHA;
}

TimeInterval::TimeInterval(const std::string& start_str, const std::string& end_str)
    : start_slot(timeStringToSlot(start_str)), end_slot(timeStringToSlot(end_str)) {
    if (start_slot > end_slot) {
        throw std::invalid_argument("TimeInterval: start cannot be after end: " + start_str + " > " + end_str);
    }
}

int TimeInterval::timeStringToSlot(const std::string& time_str) {
    int h = 0, m = 0;
    if (std::sscanf(time_str.c_str(), "%d:%d", &h, &m) != 2) {
        throw std::invalid_argument("Invalid time string: " + time_str);
    }
    if (h < 0 || h > 23 || m < 0 || m > 59) {
        throw std::invalid_argument("Time out of range: " + time_str);
    }
    int total_minutes = h * 60 + m;
    return total_minutes / SLOT_MINUTES;
}

std::string TimeInterval::slotToTimeString(int slot) {
    if (slot < 0) slot = 0;
    int total_minutes = slot * SLOT_MINUTES;
    int h = (total_minutes / 60) % 24;
    int m = total_minutes % 60;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
    return std::string(buf);
}

std::string TimeInterval::startString() const {
    return slotToTimeString(start_slot);
}

std::string TimeInterval::endString() const {
    return slotToTimeString(end_slot);
}

std::string TimeInterval::toString() const {
    return startString() + " - " + endString();
}

} // namespace scheduler::model
