/**
 * @file Turma.cpp
 * @brief Implementação dos métodos das classes e estruturas JornadaTurma e Turma.
 */

#include "model/Turma.h"
#include <algorithm>

namespace scheduler::model {

static TimeInterval makeSafeInterval(const std::string& ini, const std::string& fim) {
    try {
        int s = TimeInterval::timeStringToMinutes(ini);
        int e = TimeInterval::timeStringToMinutes(fim);
        return TimeInterval(s, e);
    } catch (...) {
        return TimeInterval();
    }
}

static std::vector<TimeInterval> computeBlocksFromLegacy(
    const TimeInterval& total,
    const std::vector<TimeInterval>& intervalos) {
    std::vector<TimeInterval> blocos;
    if (total.start_min >= total.end_min) return blocos;

    auto ativos = intervalos;
    std::sort(ativos.begin(), ativos.end());

    int current = total.start_min;
    for (const auto& inter : ativos) {
        if (inter.start_min > current && inter.start_min < total.end_min) {
            blocos.emplace_back(current, std::min(inter.start_min, total.end_min));
        }
        current = std::max(current, inter.end_min);
    }
    if (total.end_min > current) {
        blocos.emplace_back(current, total.end_min);
    }
    return blocos;
}

JornadaTurma::JornadaTurma(std::vector<TimeInterval> slots_list)
    : slots_letivos(std::move(slots_list)) {
    ordenarSlots();
}

JornadaTurma::JornadaTurma(const std::string& ini, const std::string& fim,
                           const std::string& int1_ini, const std::string& int1_fim,
                           const std::string& int2_ini, const std::string& int2_fim)
    : JornadaTurma(ini, fim, int1_ini, int1_fim, true, int2_ini, int2_fim, true, "11:00", "11:15", false) {
}

JornadaTurma::JornadaTurma(const std::string& ini, const std::string& fim,
                           const std::string& int1_ini, const std::string& int1_fim, bool a1,
                           const std::string& int2_ini, const std::string& int2_fim, bool a2,
                           const std::string& int3_ini, const std::string& int3_fim, bool a3) {
    TimeInterval total = makeSafeInterval(ini, fim);
    std::vector<TimeInterval> ints;
    if (a1) ints.push_back(makeSafeInterval(int1_ini, int1_fim));
    if (a2) ints.push_back(makeSafeInterval(int2_ini, int2_fim));
    if (a3) ints.push_back(makeSafeInterval(int3_ini, int3_fim));
    auto blocos = computeBlocksFromLegacy(total, ints);
    slots_letivos = gerarSlotsDeBlocos(blocos, 50);
    ordenarSlots();
}

void JornadaTurma::ordenarSlots() {
    std::sort(slots_letivos.begin(), slots_letivos.end());
}

bool JornadaTurma::isValid() const noexcept {
    if (slots_letivos.empty()) return false;
    for (size_t i = 0; i < slots_letivos.size(); ++i) {
        if (slots_letivos[i].start_min < 0 || slots_letivos[i].end_min > MINUTES_PER_DAY) return false;
        if (slots_letivos[i].start_min >= slots_letivos[i].end_min) return false;
        if (i > 0 && slots_letivos[i - 1].end_min > slots_letivos[i].start_min) return false;
    }
    return true;
}

TimeInterval JornadaTurma::jornadaTotal() const noexcept {
    if (slots_letivos.empty()) return TimeInterval();
    return TimeInterval(slots_letivos.front().start_min, slots_letivos.back().end_min);
}

std::vector<TimeInterval> JornadaTurma::intervalosAtivos() const {
    std::vector<TimeInterval> ativos;
    if (slots_letivos.size() < 2) return ativos;
    for (size_t i = 0; i + 1 < slots_letivos.size(); ++i) {
        if (slots_letivos[i].end_min < slots_letivos[i + 1].start_min) {
            ativos.emplace_back(slots_letivos[i].end_min, slots_letivos[i + 1].start_min);
        }
    }
    return ativos;
}

int JornadaTurma::minutosLetivos() const noexcept {
    int total = 0;
    for (const auto& s : slots_letivos) {
        total += s.durationMinutes();
    }
    return total;
}

bool JornadaTurma::contemSlotExato(const TimeInterval& target) const noexcept {
    for (const auto& s : slots_letivos) {
        if (s == target) return true;
    }
    return false;
}

bool JornadaTurma::isMinutoEmIntervalo(int minute) const noexcept {
    for (size_t i = 0; i + 1 < slots_letivos.size(); ++i) {
        if (minute >= slots_letivos[i].end_min && minute < slots_letivos[i + 1].start_min) {
            return true;
        }
    }
    return false;
}

std::vector<TimeInterval> JornadaTurma::defaultSlotsForShift(ShiftType shift) {
    switch (shift) {
        case ShiftType::MANHA:
            // 07:30 - 11:45: 3x50m + 2x45m com intervalo 09:50 - 10:05
            return {
                TimeInterval("08:00", "08:45"), // 45m
                TimeInterval("08:45", "09:30"), // 45m
                TimeInterval("09:45", "10:30"), // 45m
                // Recreio: 09:55 - 10:10 (15m)
                TimeInterval("10:30", "11:15"), // 45m
                TimeInterval("11:15", "12:00")  // 45m
            };
        case ShiftType::TARDE:
            // 13:15 - 17:30: 3x50m + 2x45m com intervalo 15:35 - 15:50
            return {
                TimeInterval("13:15", "14:05"), // 50m
                TimeInterval("14:05", "14:55"), // 50m
                TimeInterval("14:55", "15:40"), // 45m
                // Recreio: 15:40 - 15:55 (15m)
                TimeInterval("15:55", "16:45"), // 50m
                TimeInterval("16:45", "17:30")  // 45m
            };
        case ShiftType::NOITE:
            // 18:30 - 22:15: 4x50m (ou 2x50 + 2x45) com intervalo 20:10 - 20:25
            return {
                TimeInterval("18:30", "19:20"), // 50m
                TimeInterval("19:20", "20:10"), // 50m
                // Recreio: 20:10 - 20:25 (15m)
                TimeInterval("20:25", "21:15"), // 50m
                TimeInterval("21:15", "22:05")  // 50m
            };
        case ShiftType::INTEGRAL:
            return {
                TimeInterval("08:00", "08:45"), // 45m
                TimeInterval("08:45", "09:30"), // 45m
                TimeInterval("09:45", "10:30"), // 45m
                // Recreio: 09:55 - 10:10 (15m)
                TimeInterval("10:30", "11:15"), // 45m
                TimeInterval("11:15", "12:00"), // 45m
                // Almoço: 11:45 - 13:15
                TimeInterval("13:15", "14:00"),
                TimeInterval("14:00", "14:45"),
                TimeInterval("14:45", "15:30"),
                // Recreio tarde: 15:40 - 15:55
                TimeInterval("15:45", "16:30"),
                TimeInterval("16:30", "17:15")
            };
    }
    return defaultSlotsForShift(ShiftType::MANHA);
}

std::vector<TimeInterval> JornadaTurma::gerarSlotsDeBlocos(const std::vector<TimeInterval>& blocos,
                                                           int duracao_preferencial) {
    std::vector<TimeInterval> result;
    int primary = (duracao_preferencial == 45) ? 45 : 50;
    int secondary = (primary == 50) ? 45 : 50;

    for (const auto& b : blocos) {
        int L = b.durationMinutes();
        if (L <= 0) continue;

        // Tenta decompor L = a * primary + b * secondary de forma exata
        int best_a = -1;
        int best_b = -1;
        for (int a = L / primary; a >= 0; --a) {
            int rem = L - a * primary;
            if (rem % secondary == 0) {
                best_a = a;
                best_b = rem / secondary;
                break;
            }
        }

        if (best_a >= 0) {
            int cur = b.start_min;
            for (int i = 0; i < best_a; ++i) {
                result.emplace_back(cur, cur + primary);
                cur += primary;
            }
            for (int i = 0; i < best_b; ++i) {
                result.emplace_back(cur, cur + secondary);
                cur += secondary;
            }
        } else if (L > primary) {
            // Preenche com a duração preferencial enquanto couber; resto vira slot residual se >= 30
            int cur = b.start_min;
            while (cur + primary <= b.end_min) {
                result.emplace_back(cur, cur + primary);
                cur += primary;
            }
            if (b.end_min - cur > 0) {
                result.emplace_back(cur, b.end_min);
            }
        } else {
            result.push_back(b);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

Turma::Turma(std::string id, std::string nome, ShiftType turno)
    : id_(std::move(id)), nome_(std::move(nome)), turno_(turno) {
    jornada_padrao_ = JornadaTurma(JornadaTurma::defaultSlotsForShift(turno));
}

bool Turma::funcionaNoDia(DayOfWeek d) const noexcept {
    return std::find(dias_letivos_.begin(), dias_letivos_.end(), d) != dias_letivos_.end();
}

JornadaTurma Turma::jornadaParaDia(DayOfWeek d) const {
    auto it = excecoes_por_dia_.find(d);
    if (it != excecoes_por_dia_.end()) {
        return it->second;
    }
    return jornada_padrao_;
}

} // namespace scheduler::model
