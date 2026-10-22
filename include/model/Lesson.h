#pragma once

#include "model/TimeSlot.h"
#include <string>
#include <vector>
#include <optional>

namespace scheduler::model {

class Disciplina {
public:
    Disciplina() = default;
    Disciplina(std::string id, std::string nome, std::string tipo = "TEORICA",
               int duracao_minutos = 50, int aulas_semanais = 4)
        : id_(std::move(id)), nome_(std::move(nome)), tipo_(std::move(tipo)),
          duracao_minutos_(duracao_minutos), aulas_semanais_(aulas_semanais) {}

    const std::string& id() const noexcept { return id_; }
    void setId(std::string id) { id_ = std::move(id); }

    const std::string& nome() const noexcept { return nome_; }
    void setNome(std::string nome) { nome_ = std::move(nome); }

    const std::string& tipo() const noexcept { return tipo_; }
    void setTipo(std::string tipo) { tipo_ = std::move(tipo); }

    int duracaoMinutos() const noexcept { return duracao_minutos_; }
    void setDuracaoMinutos(int duracao) noexcept { duracao_minutos_ = duracao; }

    int duracaoSlots() const noexcept { return duracao_minutos_ / SLOT_MINUTES; }

    int aulasSemanais() const noexcept { return aulas_semanais_; }
    void setAulasSemanais(int aulas) noexcept { aulas_semanais_ = aulas; }

    const std::vector<std::string>& professores() const noexcept { return professores_; }
    void setProfessores(std::vector<std::string> profs) { professores_ = std::move(profs); }
    void addProfessor(std::string prof_id) { professores_.push_back(std::move(prof_id)); }

    const std::vector<std::string>& turmas() const noexcept { return turmas_; }
    void setTurmas(std::vector<std::string> turmas) { turmas_ = std::move(turmas); }
    void addTurma(std::string turma_id) { turmas_.push_back(std::move(turma_id)); }

    const std::vector<ShiftType>& turnosPermitidos() const noexcept { return turnos_permitidos_; }
    void setTurnosPermitidos(std::vector<ShiftType> turnos) { turnos_permitidos_ = std::move(turnos); }
    void addTurnoPermitido(ShiftType turno) { turnos_permitidos_.push_back(turno); }

    // Conjuntos de salas admissíveis (seção 3.3). Exemplo: [["LAB01"], ["LAB02"]]
    const std::vector<std::vector<std::string>>& salasAlternativas() const noexcept { return salas_alternativas_; }
    void setSalasAlternativas(std::vector<std::vector<std::string>> salas) { salas_alternativas_ = std::move(salas); }
    void addCombinacaoSalas(std::vector<std::string> combo) { salas_alternativas_.push_back(std::move(combo)); }

    bool exigirAulasConsecutivas() const noexcept { return exigir_aulas_consecutivas_; }
    void setExigirAulasConsecutivas(bool exigir) noexcept { exigir_aulas_consecutivas_ = exigir; }

    int maxAulasPorDiaTurma() const noexcept { return max_aulas_por_dia_turma_; }
    void setMaxAulasPorDiaTurma(int max_aulas) noexcept { max_aulas_por_dia_turma_ = max_aulas; }

    bool isTeorica() const noexcept { return tipo_ == "TEORICA" || tipo_ == "Teorica"; }

private:
    std::string id_;
    std::string nome_;
    std::string tipo_{"TEORICA"};
    int duracao_minutos_{50};
    int aulas_semanais_{4};
    std::vector<std::string> professores_;
    std::vector<std::string> turmas_;
    std::vector<ShiftType> turnos_permitidos_{ShiftType::MANHA};
    std::vector<std::vector<std::string>> salas_alternativas_;
    bool exigir_aulas_consecutivas_{false};
    int max_aulas_por_dia_turma_{2};
};

class Lesson {
public:
    Lesson() = default;
    Lesson(std::string id, std::string disciplina_id, std::string disciplina_nome,
           std::string tipo, int duracao_minutos,
           std::vector<std::string> professores, std::vector<std::string> turmas)
        : id_(std::move(id)), disciplina_id_(std::move(disciplina_id)),
          disciplina_nome_(std::move(disciplina_nome)), tipo_(std::move(tipo)),
          duracao_minutos_(duracao_minutos), professores_(std::move(professores)),
          turmas_(std::move(turmas)) {}

    const std::string& id() const noexcept { return id_; }
    void setId(std::string id) { id_ = std::move(id); }

    const std::string& disciplinaId() const noexcept { return disciplina_id_; }
    void setDisciplinaId(std::string id) { disciplina_id_ = std::move(id); }

    const std::string& disciplinaNome() const noexcept { return disciplina_nome_; }
    void setDisciplinaNome(std::string nome) { disciplina_nome_ = std::move(nome); }

    const std::string& tipo() const noexcept { return tipo_; }
    void setTipo(std::string tipo) { tipo_ = std::move(tipo); }

    int duracaoMinutos() const noexcept { return duracao_minutos_; }
    void setDuracaoMinutos(int duracao) noexcept { duracao_minutos_ = duracao; }

    int duracaoSlots() const noexcept { return duracao_minutos_ / SLOT_MINUTES; }

    const std::vector<std::string>& professores() const noexcept { return professores_; }
    void setProfessores(std::vector<std::string> profs) { professores_ = std::move(profs); }

    const std::vector<std::string>& turmas() const noexcept { return turmas_; }
    void setTurmas(std::vector<std::string> turmas) { turmas_ = std::move(turmas); }

    const std::vector<ShiftType>& turnosPermitidos() const noexcept { return turnos_permitidos_; }
    void setTurnosPermitidos(std::vector<ShiftType> turnos) { turnos_permitidos_ = std::move(turnos); }

    const std::vector<std::vector<std::string>>& salasAlternativas() const noexcept { return salas_alternativas_; }
    void setSalasAlternativas(std::vector<std::vector<std::string>> salas) { salas_alternativas_ = std::move(salas); }

    bool isTeorica() const noexcept { return tipo_ == "TEORICA" || tipo_ == "Teorica"; }

    // Bloqueio / Fixação manual (seção 5 & 9)
    bool isPinned() const noexcept { return is_pinned_; }
    void setPinned(bool pinned) noexcept { is_pinned_ = pinned; }

    DayOfWeek pinnedDay() const noexcept { return pinned_day_; }
    void setPinnedDay(DayOfWeek day) noexcept { pinned_day_ = day; }

    int pinnedStartSlot() const noexcept { return pinned_start_slot_; }
    void setPinnedStartSlot(int slot) noexcept { pinned_start_slot_ = slot; }

    const std::vector<std::string>& pinnedRooms() const noexcept { return pinned_rooms_; }
    void setPinnedRooms(std::vector<std::string> rooms) { pinned_rooms_ = std::move(rooms); }

    void unpin() noexcept {
        is_pinned_ = false;
        pinned_rooms_.clear();
    }

private:
    std::string id_;
    std::string disciplina_id_;
    std::string disciplina_nome_;
    std::string tipo_{"TEORICA"};
    int duracao_minutos_{50};
    std::vector<std::string> professores_;
    std::vector<std::string> turmas_;
    std::vector<ShiftType> turnos_permitidos_{ShiftType::MANHA};
    std::vector<std::vector<std::string>> salas_alternativas_;

    bool is_pinned_{false};
    DayOfWeek pinned_day_{DayOfWeek::SEG};
    int pinned_start_slot_{0};
    std::vector<std::string> pinned_rooms_;
};

} // namespace scheduler::model
