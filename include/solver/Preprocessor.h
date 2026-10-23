#pragma once

#include "model/InstitutionData.h"
#include "solver/CandidateAllocation.h"
#include <vector>
#include <string>
#include <map>

namespace scheduler::solver {

struct ValidationResult {
    bool is_valid{true};
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    void addError(std::string err) {
        errors.push_back(std::move(err));
        is_valid = false;
    }
    void addWarning(std::string warn) {
        warnings.push_back(std::move(warn));
    }
};

class Preprocessor {
public:
    static ValidationResult validarDados(const model::InstitutionData& data);

    static std::vector<CandidateAllocation> gerarCandidatosParaAula(
        const model::InstitutionData& data,
        const model::Lesson& aula,
        std::vector<std::string>& diagnostico_aula);

    static std::map<std::string, std::vector<CandidateAllocation>> gerarTodosCandidatos(
        const model::InstitutionData& data,
        ValidationResult& resultado);

    static bool turmaAcomodaAula(const model::Turma& turma, model::DayOfWeek dia,
                                 int start_slot, int duration_slots);
    static bool shiftAcomodaAula(model::ShiftType shift, int start_slot, int duration_slots);
};

} // namespace scheduler::solver
