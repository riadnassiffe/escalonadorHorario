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
        initDefaults();
    }

    Professor(std::string id, std::string nome)
        : id_(std::move(id)), nome_(std::move(nome)) {
        initDefaults();
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

    // Aversão a janelas entre aulas (0 = indiferente/aceita janelas, 10 = máxima aversão)
    int pesoJanela() const noexcept { return peso_janela_; }
    void setPesoJanela(int peso) noexcept {
        if (peso < 0) peso = 0;
        if (peso > 10) peso = 10;
        peso_janela_ = peso;
    }

    // Turnos que o professor pretende trabalhar por dia da semana
    const std::vector<ShiftType>& turnosPretendidos(DayOfWeek d) const;
    bool pretendeTurno(DayOfWeek d, ShiftType shift) const;
    void setTurnosPretendidos(DayOfWeek d, std::vector<ShiftType> turnos);
    void addTurnoPretendido(DayOfWeek d, ShiftType shift);
    void clearTurnosPretendidos(DayOfWeek d);
    const std::map<DayOfWeek, std::vector<ShiftType>>& todasPreferenciasTurnos() const noexcept {
        return turnos_pretendidos_;
    }

private:
    void initDefaults();

    std::string id_;
    std::string nome_;
    std::array<int, 5> peso_dias_{5, 5, 5, 5, 5};
    int peso_janela_{8}; // Aversão a janelas individual do professor
    std::map<DayOfWeek, std::vector<TimeInterval>> disponibilidade_;
    std::map<DayOfWeek, std::vector<ShiftType>> turnos_pretendidos_;
    int carga_maxima_diaria_minutos_{0}; // 0 = sem limite
    int max_aulas_por_dia_{0};           // 0 = sem limite
};

} // namespace scheduler::model
