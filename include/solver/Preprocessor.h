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
 *
 * Registra erros impeditivos para o escalonamento e advertências sobre possíveis inconsistências.
 */
struct ValidationResult {
    bool is_valid{true};                ///< Flag indicando se a base de dados é válida para resolução.
    std::vector<std::string> errors;    ///< Mensagens de erro impeditivas.
    std::vector<std::string> warnings;  ///< Mensagens de advertência não impeditivas.

    /**
     * @brief Registra um erro impeditivo e altera a flag is_valid para false.
     * @param err Descrição detalhada do erro detectado.
     */
    void addError(std::string err) {
        errors.push_back(std::move(err));
        is_valid = false;
    }

    /**
     * @brief Registra uma advertência informativa.
     * @param warn Descrição do aviso detectado.
     */
    void addWarning(std::string warn) {
        warnings.push_back(std::move(warn));
    }
};

/**
 * @class Preprocessor
 * @brief Responsável pela validação dos dados da instituição e geração do domínio de candidatos viáveis.
 *
 * Filtra horários e dias que respeitam os blocos contínuos da jornada das turmas, a disponibilidade
 * de professores e a compatibilidade de salas físicas antes de entregar o modelo ao solver.
 */
class Preprocessor {
public:
    /**
     * @brief Executa validação ampla da integridade cadastral e consistência de jornadas da instituição.
     * @param data Instância de InstitutionData contendo todas as entidades.
     * @return Objeto ValidationResult contendo eventuais erros e avisos.
     */
    static ValidationResult validarDados(const model::InstitutionData& data);

    /**
     * @brief Gera o conjunto de alocações candidatas viáveis para uma aula específica.
     * @param data Dados cadastrais da instituição.
     * @param aula Ocorrência de aula a ser pré-processada.
     * @param diagnostico_aula Vetor preenchido com mensagens de diagnóstico caso não haja candidatos viáveis.
     * @return Vetor de CandidateAllocation admissíveis para a aula.
     */
    static std::vector<CandidateAllocation> gerarCandidatosParaAula(
        const model::InstitutionData& data,
        const model::Lesson& aula,
        std::vector<std::string>& diagnostico_aula);

    /**
     * @brief Gera o mapa de candidatos para todas as aulas ativas da instituição.
     * @param data Dados cadastrais da instituição.
     * @param resultado Estrutura ValidationResult para acumular erros de aulas sem candidatos.
     * @return Mapa associando cada aula_id à sua lista de CandidateAllocations viáveis.
     */
    static std::map<std::string, std::vector<CandidateAllocation>> gerarTodosCandidatos(
        const model::InstitutionData& data,
        ValidationResult& resultado);

    /**
     * @brief Verifica se a jornada de uma turma acomoda integralmente uma aula em determinado slot.
     * @param turma Turma analisada.
     * @param dia Dia da semana proposto.
     * @param start_slot Slot inicial da aula.
     * @param duration_slots Duração da aula em slots.
     * @return true se a aula couber inteiramente em um único bloco contínuo da turma.
     */
    static bool turmaAcomodaAula(const model::Turma& turma, model::DayOfWeek dia,
                                 int start_slot, int duration_slots);

    /**
     * @brief Verifica se um determinado horário está contido nos limites do turno de trabalho especificado.
     * @param shift Turno avaliado (MANHA, TARDE, NOITE, INTEGRAL).
     * @param start_slot Slot inicial da aula.
     * @param duration_slots Duração da aula em slots.
     * @return true se o horário estiver compreendido no turno.
     */
    static bool shiftAcomodaAula(model::ShiftType shift, int start_slot, int duration_slots);
};

} // namespace scheduler::solver
