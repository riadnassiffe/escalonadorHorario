#pragma once

#include "model/Professor.h"
#include "model/Room.h"
#include "model/Turma.h"
#include "model/Lesson.h"
#include "model/OptimizationConfig.h"
#include "model/Timetable.h"
#include <vector>
#include <string>
#include <memory>

namespace scheduler::model {

class InstitutionData {
public:
    InstitutionData();

    // Professores
    const std::vector<Professor>& professores() const noexcept { return professores_; }
    std::vector<Professor>& professores() noexcept { return professores_; }
    void addProfessor(Professor prof);
    bool removeProfessor(const std::string& id);
    const Professor* findProfessor(const std::string& id) const;
    Professor* findProfessor(const std::string& id);

    // Salas
    const std::vector<Room>& salas() const noexcept { return salas_; }
    std::vector<Room>& salas() noexcept { return salas_; }
    void addSala(Room sala);
    bool removeSala(const std::string& id);
    const Room* findSala(const std::string& id) const;
    Room* findSala(const std::string& id);

    // Turmas
    const std::vector<Turma>& turmas() const noexcept { return turmas_; }
    std::vector<Turma>& turmas() noexcept { return turmas_; }
    void addTurma(Turma turma);
    bool removeTurma(const std::string& id);
    const Turma* findTurma(const std::string& id) const;
    Turma* findTurma(const std::string& id);

    // Disciplinas
    const std::vector<Disciplina>& disciplinas() const noexcept { return disciplinas_; }
    std::vector<Disciplina>& disciplinas() noexcept { return disciplinas_; }
    void addDisciplina(Disciplina disc);
    bool removeDisciplina(const std::string& id);
    const Disciplina* findDisciplina(const std::string& id) const;
    Disciplina* findDisciplina(const std::string& id);

    // Ocorrências de Aulas (A)
    const std::vector<Lesson>& aulas() const noexcept { return aulas_; }
    std::vector<Lesson>& aulas() noexcept { return aulas_; }
    void setAulas(std::vector<Lesson> aulas) { aulas_ = std::move(aulas); }
    void gerarOcorrenciasAulas(bool preservar_fixadas = true,
                               EscopoSemestreOtimizacao escopo = EscopoSemestreOtimizacao::TODAS_SEM_FILTRO);
    const Lesson* findAula(const std::string& id) const;
    Lesson* findAula(const std::string& id);

    // Configuração da Otimização
    const OptimizationConfig& config() const noexcept { return config_; }
    OptimizationConfig& config() noexcept { return config_; }
    void setConfig(OptimizationConfig cfg) { config_ = std::move(cfg); }

    // Grade gerada
    const Timetable& grade() const noexcept { return grade_; }
    Timetable& grade() noexcept { return grade_; }
    void setGrade(Timetable grade) { grade_ = std::move(grade); }

    void clear();

    // Geração dinâmica de IDs para novas entidades
    std::string nextProfessorId() const;
    std::string nextTurmaId() const;
    std::string nextSalaId() const;
    std::string nextDisciplinaId() const;

    // Carrega o cenário acadêmico completo descrito nas instruções
    static InstitutionData criarExemploPadrao();

private:
    std::vector<Professor> professores_;
    std::vector<Room> salas_;
    std::vector<Turma> turmas_;
    std::vector<Disciplina> disciplinas_;
    std::vector<Lesson> aulas_;
    OptimizationConfig config_;
    Timetable grade_;
};

} // namespace scheduler::model
