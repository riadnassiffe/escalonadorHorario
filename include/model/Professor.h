#pragma once

#include "model/TimeSlot.h"
#include <string>
#include <vector>
#include <map>
#include <array>

namespace scheduler::model {

class Professor {
public:
    Professor() {
        for (size_t i = 0; i < 5; ++i) {
            peso_dias_[i] = 5; // Valor neutro padrão (seção 2.3)
        }
    }

    Professor(std::string id, std::string nome)
        : id_(std::move(id)), nome_(std::move(nome)) {
        for (size_t i = 0; i < 5; ++i) {
            peso_dias_[i] = 5;
        }
    }

    const std::string& id() const noexcept { return id_; }
    void setId(std::string id) { id_ = std::move(id); }

    const std::string& nome() const noexcept { return nome_; }
    void setNome(std::string nome) { nome_ = std::move(nome); }

    // Pesos de preferência para cada dia: 0 a 10 (10 = maior preferência, 0 = menor)
    int pesoDia(DayOfWeek d) const noexcept {
        return peso_dias_[static_cast<size_t>(d)];
    }
    void setPesoDia(DayOfWeek d, int peso) {
        if (peso < 0) peso = 0;
        if (peso > 10) peso = 10;
        peso_dias_[static_cast<size_t>(d)] = peso;
    }

    // Penalidade por alocar professor no dia (seção 4.4): c_p,d = 10 - omega_p,d
    int penalidadeDia(DayOfWeek d) const noexcept {
        return 10 - pesoDia(d);
    }

    // Disponibilidade de horários por dia (seção 3.8)
    const std::vector<TimeInterval>& disponibilidade(DayOfWeek d) const {
        static const std::vector<TimeInterval> empty_intervals;
        auto it = disponibilidade_.find(d);
        if (it != disponibilidade_.end()) {
            return it->second;
        }
        return empty_intervals;
    }

    const std::map<DayOfWeek, std::vector<TimeInterval>>& todasDisponibilidades() const noexcept {
        return disponibilidade_;
    }

    void setDisponibilidade(DayOfWeek d, std::vector<TimeInterval> intervals) {
        disponibilidade_[d] = std::move(intervals);
    }

    void addIntervaloDisponivel(DayOfWeek d, const TimeInterval& interval) {
        disponibilidade_[d].push_back(interval);
    }

    void clearDisponibilidade(DayOfWeek d) {
        disponibilidade_.erase(d);
    }

    bool isDisponivelNoIntervalo(DayOfWeek d, int start_slot, int end_slot) const;

    // Carga horária máxima diária em minutos (seção 3.9)
    int cargaMaximaDiariaMinutos() const noexcept { return carga_maxima_diaria_minutos_; }
    void setCargaMaximaDiariaMinutos(int minutos) noexcept { carga_maxima_diaria_minutos_ = minutos; }

    int maxAulasPorDia() const noexcept { return max_aulas_por_dia_; }
    void setMaxAulasPorDia(int max_aulas) noexcept { max_aulas_por_dia_ = max_aulas; }

private:
    std::string id_;
    std::string nome_;
    std::array<int, 5> peso_dias_{5, 5, 5, 5, 5};
    std::map<DayOfWeek, std::vector<TimeInterval>> disponibilidade_;
    int carga_maxima_diaria_minutos_{0}; // 0 = sem limite
    int max_aulas_por_dia_{0};           // 0 = sem limite
};

} // namespace scheduler::model
