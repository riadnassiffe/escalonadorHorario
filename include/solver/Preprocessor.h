#pragma once

/**
 * @file Preprocessor.h
 * @brief Pré-processamento, validação de consistência e geração de candidatos viáveis para o escalonamento.
 */

#include "model/InstitutionData.h"
#include "solver/CandidateAllocation.h"
#include <vector>
#include <string>
#include <map>

namespace scheduler::solver {

/**
 * @struct ValidationResult
 * @brief Armazena o resultado da validação prévia dos dados de entrada da instituição.
 */
struct ValidationResult {
    bool is_valid{true};                ///< Flag indicando se a base de dados é válida para resolução.
    std::vector<std::string> errors;    ///< Mensagens de erro impeditivas.
    std::vector<std::string> warnings;  ///< Mensagens de advertência não impeditivas.

    void addError(std::string err) {
        errors.push_back(std::move(err));
        is_valid = false;
    }

    void addWarning(std::string warn) {
        warnings.push_back(std::move(warn));
    }
};

/**
 * @class Preprocessor
 * @brief Responsável pela validação dos dados da instituição e geração do domínio de candidatos viáveis.
 *
 * Seleciona slots configurados para as turmas cuja duração coincida exatamente com a da aula,
 * respeitando disponibilidade de professores e compatibilidade de salas físicas.
 */
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

    /**
     * @brief Verifica se uma turma funciona no dia e possui exatamente o slot informado cadastrado.
     * @param turma Turma analisada.
     * @param dia Dia da semana proposto.
     * @param slot Intervalo exato [start_min, end_min) do slot candidato.
     * @return true se a turma possuir exatamente esse slot letivo no dia.
     */
    static bool turmaAcomodaSlot(const model::Turma& turma, model::DayOfWeek dia,
                                 const model::TimeInterval& slot);
};

} // namespace scheduler::solver
