/**
 * @file TimeSlot.cpp
 * @brief Implementação das funções utilitárias de manipulação de dias, turnos e intervalos em minutos.
 */

#include "model/TimeSlot.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

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

const std::vector<ShiftType>& allWorkShifts() {
    static const std::vector<ShiftType> shifts = {
        ShiftType::MANHA,
        ShiftType::TARDE,
        ShiftType::NOITE
    };
    return shifts;
}

ShiftType timeToShift(int minutes) {
    constexpr int MIN_12_30 = 12 * 60 + 30; // 750 min
    constexpr int MIN_18_00 = 18 * 60;      // 1080 min
    if (minutes < MIN_12_30) return ShiftType::MANHA;
    if (minutes < MIN_18_00) return ShiftType::TARDE;
    return ShiftType::NOITE;
}

TimeInterval::TimeInterval(const std::string& start_str, const std::string& end_str)
    : start_min(timeStringToMinutes(start_str)), end_min(timeStringToMinutes(end_str)) {
    if (start_min > end_min) {
        throw std::invalid_argument("TimeInterval: start cannot be after end: " + start_str + " > " + end_str);
    }
}

int TimeInterval::timeStringToMinutes(const std::string& time_str) {
    int h = 0, m = 0;
    if (std::sscanf(time_str.c_str(), "%d:%d", &h, &m) != 2) {
        throw std::invalid_argument("Invalid time string: " + time_str);
    }
    if (h == 24 && m == 0) {
        return MINUTES_PER_DAY;
    }
    if (h < 0 || h > 23 || m < 0 || m > 59) {
        throw std::invalid_argument("Time out of range: " + time_str);
    }
    return h * 60 + m;
}

std::string TimeInterval::minutesToTimeString(int minutes) {
    if (minutes < 0) minutes = 0;
    if (minutes >= MINUTES_PER_DAY) return "24:00";
    int h = minutes / 60;
    int m = minutes % 60;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
    return std::string(buf);
}

std::string TimeInterval::startString() const {
    return minutesToTimeString(start_min);
}

std::string TimeInterval::endString() const {
    return minutesToTimeString(end_min);
}

std::string TimeInterval::toString() const {
    return startString() + " - " + endString();
}

std::string formatDurationSeconds(double total_seconds) {
    long long total = (!std::isfinite(total_seconds) || total_seconds <= 0.0)
        ? 0LL
        : static_cast<long long>(std::llround(total_seconds));
    if (total < 0) total = 0;
    long long h = total / 3600;
    long long m = (total % 3600) / 60;
    long long s = total % 60;
    char buf[48];
    std::snprintf(buf, sizeof(buf), "%02lldh %02lldm %02llds", h, m, s);
    return std::string(buf);
}

std::string formatDurationMs(double duration_ms) {
    if (!std::isfinite(duration_ms) || duration_ms <= 0.0) {
        return "00h 00m 00s";
    }
    if (duration_ms < 995.0) {
        long long hundredths = std::clamp(static_cast<long long>(std::llround(duration_ms / 10.0)), 1LL, 99LL);
        char buf[48];
        std::snprintf(buf, sizeof(buf), "00h 00m 00.%02llds", hundredths);
        return std::string(buf);
    }
    return formatDurationSeconds(std::round(duration_ms / 1000.0));
}

void secondsToHms(double total_seconds, int& hours, int& minutes, int& seconds) {
    long long total = (!std::isfinite(total_seconds) || total_seconds <= 0.0)
        ? 0LL
        : static_cast<long long>(std::llround(total_seconds));
    if (total < 0) total = 0;
    hours = static_cast<int>(total / 3600);
    minutes = static_cast<int>((total % 3600) / 60);
    seconds = static_cast<int>(total % 60);
}

double hmsToSeconds(int hours, int minutes, int seconds) {
    long long total = static_cast<long long>(std::max(0, hours)) * 3600LL +
                      static_cast<long long>(std::max(0, minutes)) * 60LL +
                      static_cast<long long>(std::max(0, seconds));
    return static_cast<double>(std::max(1LL, total));
}

} // namespace scheduler::model
