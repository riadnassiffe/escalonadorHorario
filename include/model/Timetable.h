#pragma once

#include "model/TimeSlot.h"
#include <string>
#include <vector>
#include <map>

namespace scheduler::model {

struct AllocatedLesson {
    std::string aula_id;
    std::string disciplina_id;
    std::string disciplina_nome;
    std::string tipo{"TEORICA"};
    DayOfWeek dia{DayOfWeek::SEG};
    TimeInterval intervalo;
    std::vector<std::string> professores;
    std::vector<std::string> turmas;
    std::vector<std::string> salas;
    bool is_pinned{false};

    std::string horarioFormatado() const {
        return intervalo.toString();
    }
    std::string diaString() const {
        return dayToString(dia);
    }
};

struct TimetableStatistics {
    std::string status{"NAO_EXECUTADO"}; // OPTIMAL, FEASIBLE, INFEASIBLE, UNKNOWN
    double tempo_execucao_ms{0.0};
    double valor_objetivo{0.0};
    int penalidade_janelas_J{0};
    int penalidade_dias_D{0};
    int penalidade_preferencias_P{0};
    int penalidade_preferencia_dias_W{0};
    int penalidade_lacunas_turma_JG{0};
    int total_aulas_alocadas{0};
    int total_aulas_esperadas{0};
    std::string solver_info;
};

class Timetable {
public:
    Timetable() = default;

    void addAllocatedLesson(AllocatedLesson lesson) {
        alocacoes_.push_back(std::move(lesson));
    }

    const std::vector<AllocatedLesson>& alocacoes() const noexcept { return alocacoes_; }
    std::vector<AllocatedLesson>& alocacoes() noexcept { return alocacoes_; }

    void clear() {
        alocacoes_.clear();
        salas_teoricas_designadas_.clear();
        estatisticas_ = TimetableStatistics();
        mensagens_diagnostico_.clear();
    }

    const std::map<std::string, std::string>& salasTeoricasDesignadas() const noexcept {
        return salas_teoricas_designadas_;
    }
    void setSalaTeoricaDesignada(std::string turma_id, std::string sala_id) {
        salas_teoricas_designadas_[std::move(turma_id)] = std::move(sala_id);
    }
    std::string salaTeoricaTurma(const std::string& turma_id) const {
        auto it = salas_teoricas_designadas_.find(turma_id);
        if (it != salas_teoricas_designadas_.end()) return it->second;
        return "";
    }

    const TimetableStatistics& estatisticas() const noexcept { return estatisticas_; }
    void setEstatisticas(TimetableStatistics stats) { estatisticas_ = std::move(stats); }

    const std::vector<std::string>& mensagensDiagnostico() const noexcept { return mensagens_diagnostico_; }
    void addMensagemDiagnostico(std::string msg) { mensagens_diagnostico_.push_back(std::move(msg)); }
    void setMensagensDiagnostico(std::vector<std::string> msgs) { mensagens_diagnostico_ = std::move(msgs); }

    std::vector<AllocatedLesson> aulasPorTurma(const std::string& turma_id, DayOfWeek d) const;
    std::vector<AllocatedLesson> aulasPorProfessor(const std::string& prof_id, DayOfWeek d) const;
    std::vector<AllocatedLesson> aulasPorSala(const std::string& sala_id, DayOfWeek d) const;

    AllocatedLesson* findAula(const std::string& aula_id);

private:
    std::vector<AllocatedLesson> alocacoes_;
    std::map<std::string, std::string> salas_teoricas_designadas_;
    TimetableStatistics estatisticas_;
    std::vector<std::string> mensagens_diagnostico_;
};

} // namespace scheduler::model
