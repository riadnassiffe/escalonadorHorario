#include "model/Turma.h"
#include <algorithm>

namespace scheduler::model {

JornadaTurma::JornadaTurma(const std::string& ini, const std::string& fim,
                           const std::string& int1_ini, const std::string& int1_fim,
                           const std::string& int2_ini, const std::string& int2_fim)
    : jornada_total(ini, fim),
      intervalo_1(int1_ini, int1_fim),
      int1_ativo(true),
      intervalo_2(int2_ini, int2_fim),
      int2_ativo(true),
      intervalo_3("11:00", "11:15"),
      int3_ativo(false) {
}

JornadaTurma::JornadaTurma(const std::string& ini, const std::string& fim,
                           const std::string& int1_ini, const std::string& int1_fim, bool a1,
                           const std::string& int2_ini, const std::string& int2_fim, bool a2,
                           const std::string& int3_ini, const std::string& int3_fim, bool a3)
    : jornada_total(ini, fim),
      intervalo_1(int1_ini, int1_fim),
      int1_ativo(a1),
      intervalo_2(int2_ini, int2_fim),
      int2_ativo(a2),
      intervalo_3(int3_ini, int3_fim),
      int3_ativo(a3) {
}

std::vector<TimeInterval> JornadaTurma::intervalosAtivos() const {
    std::vector<TimeInterval> ativos;
    if (int1_ativo && intervalo_1.start_slot < intervalo_1.end_slot) {
        ativos.push_back(intervalo_1);
    }
    if (int2_ativo && intervalo_2.start_slot < intervalo_2.end_slot) {
        ativos.push_back(intervalo_2);
    }
    if (int3_ativo && intervalo_3.start_slot < intervalo_3.end_slot) {
        ativos.push_back(intervalo_3);
    }
    std::sort(ativos.begin(), ativos.end(), [](const TimeInterval& a, const TimeInterval& b) {
        return a.start_slot < b.start_slot;
    });
    return ativos;
}

bool JornadaTurma::isValid() const noexcept {
    if (jornada_total.start_slot >= jornada_total.end_slot) return false;

    std::vector<const TimeInterval*> ativos;
    if (int1_ativo) ativos.push_back(&intervalo_1);
    if (int2_ativo) ativos.push_back(&intervalo_2);
    if (int3_ativo) ativos.push_back(&intervalo_3);

    if (ativos.empty()) return true;

    int prev_end = jornada_total.start_slot;
    for (size_t i = 0; i < ativos.size(); ++i) {
        const auto& inter = *ativos[i];
        if (inter.start_slot >= inter.end_slot) return false;
        if (i == 0) {
            if (inter.start_slot <= prev_end) return false;
        } else {
            if (inter.start_slot < prev_end) return false;
        }
        prev_end = inter.end_slot;
    }
    if (prev_end >= jornada_total.end_slot) return false;

    return true;
}

std::vector<TimeInterval> JornadaTurma::blocosLetivos() const {
    std::vector<TimeInterval> blocos;
    if (!isValid()) return blocos;

    auto ativos = intervalosAtivos();
    if (ativos.empty()) {
        if (jornada_total.start_slot < jornada_total.end_slot) {
            blocos.emplace_back(jornada_total.start_slot, jornada_total.end_slot);
        }
        return blocos;
    }

    int current = jornada_total.start_slot;
    for (const auto& inter : ativos) {
        if (inter.start_slot > current) {
            blocos.emplace_back(current, inter.start_slot);
        }
        current = std::max(current, inter.end_slot);
    }
    if (jornada_total.end_slot > current) {
        blocos.emplace_back(current, jornada_total.end_slot);
    }
    return blocos;
}

bool JornadaTurma::isSlotEmIntervalo(int slot) const noexcept {
    if (int1_ativo && intervalo_1.contains(slot)) return true;
    if (int2_ativo && intervalo_2.contains(slot)) return true;
    if (int3_ativo && intervalo_3.contains(slot)) return true;
    return false;
}

Turma::Turma(std::string id, std::string nome, ShiftType turno)
    : id_(std::move(id)), nome_(std::move(nome)), turno_(turno) {
    // Configuração padrão: 08:00 as 12:30 (240 min letivos + 2 intervalos de 15 min)
    // Bloco 1: 08:00 - 09:40 (100 min -> 2x50m)
    // Intervalo 1: 09:40 - 09:55
    // Bloco 2: 09:55 - 11:25 (90 min -> 2x45m)
    // Intervalo 2: 11:25 - 11:40
    // Bloco 3: 11:40 - 12:30 (50 min -> 1x50m)
    jornada_padrao_ = JornadaTurma("08:00", "12:30",
                                   "09:40", "09:55", true,
                                   "11:25", "11:40", true,
                                   "12:00", "12:15", false);
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
