#pragma once

#include "model/TimeSlot.h"
#include <string>
#include <vector>
#include <map>
#include <optional>

namespace scheduler::model {

struct JornadaTurma {
    TimeInterval jornada_total; // [T_ini, T_fim)
    TimeInterval intervalo_1;   // [b1, e1)
    bool int1_ativo{true};
    TimeInterval intervalo_2;   // [b2, e2)
    bool int2_ativo{true};
    TimeInterval intervalo_3;   // [b3, e3)
    bool int3_ativo{false};

    JornadaTurma() = default;
    JornadaTurma(const std::string& ini, const std::string& fim,
                 const std::string& int1_ini, const std::string& int1_fim,
                 const std::string& int2_ini, const std::string& int2_fim);
    JornadaTurma(const std::string& ini, const std::string& fim,
                 const std::string& int1_ini, const std::string& int1_fim, bool int1_ativo,
                 const std::string& int2_ini, const std::string& int2_fim, bool int2_ativo,
                 const std::string& int3_ini, const std::string& int3_fim, bool int3_ativo);

    bool isValid() const noexcept;
    std::vector<TimeInterval> blocosLetivos() const;
    std::vector<TimeInterval> intervalosAtivos() const;
    bool isSlotEmIntervalo(int slot) const noexcept;
};

class Turma {
public:
    Turma() = default;
    Turma(std::string id, std::string nome, ShiftType turno = ShiftType::MANHA);

    const std::string& id() const noexcept { return id_; }
    void setId(std::string id) { id_ = std::move(id); }

    const std::string& nome() const noexcept { return nome_; }
    void setNome(std::string nome) { nome_ = std::move(nome); }

    ShiftType turno() const noexcept { return turno_; }
    void setTurno(ShiftType turno) noexcept { turno_ = turno; }

    const std::vector<DayOfWeek>& diasLetivos() const noexcept { return dias_letivos_; }
    void setDiasLetivos(std::vector<DayOfWeek> dias) { dias_letivos_ = std::move(dias); }
    bool funcionaNoDia(DayOfWeek d) const noexcept;

    const JornadaTurma& jornadaPadrao() const noexcept { return jornada_padrao_; }
    void setJornadaPadrao(JornadaTurma jornada) { jornada_padrao_ = std::move(jornada); }

    const std::map<DayOfWeek, JornadaTurma>& excecoesPorDia() const noexcept { return excecoes_por_dia_; }
    void setExcecaoDia(DayOfWeek d, JornadaTurma jornada) { excecoes_por_dia_[d] = std::move(jornada); }
    void removeExcecaoDia(DayOfWeek d) { excecoes_por_dia_.erase(d); }

    JornadaTurma jornadaParaDia(DayOfWeek d) const;

    // Sala teórica fixa da turma (seção 3.6)
    const std::string& salaTeoricaId() const noexcept { return sala_teorica_id_; }
    void setSalaTeoricaId(std::string sala_id) { sala_teorica_id_ = std::move(sala_id); }
    bool temSalaTeoricaPredefinida() const noexcept { return !sala_teorica_id_.empty(); }

    const std::vector<std::string>& salasTeoricasPermitidas() const noexcept { return salas_teoricas_permitidas_; }
    void setSalasTeoricasPermitidas(std::vector<std::string> salas) { salas_teoricas_permitidas_ = std::move(salas); }
    void addSalaTeoricaPermitida(std::string sala_id) { salas_teoricas_permitidas_.push_back(std::move(sala_id)); }

private:
    std::string id_;
    std::string nome_;
    ShiftType turno_{ShiftType::MANHA};
    std::vector<DayOfWeek> dias_letivos_{allDays()};
    JornadaTurma jornada_padrao_;
    std::map<DayOfWeek, JornadaTurma> excecoes_por_dia_;
    std::string sala_teorica_id_; // Se vazio, solver escolhe de salas_teoricas_permitidas
    std::vector<std::string> salas_teoricas_permitidas_;
};

} // namespace scheduler::model
