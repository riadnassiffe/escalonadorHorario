#pragma once

/**
 * @file OptimizationConfig.h
 * @brief Parâmetros, pesos e algoritmos de configuração para o motor de otimização de horários.
 */

#include <string>

namespace scheduler::model {

/**
 * @enum ModeloPreferenciaDias
 * @brief Estratégia de penalização para alocação de aulas em dias de menor preferência do professor.
 */
enum class ModeloPreferenciaDias {
    POR_DIA_TRABALHADO, ///< Penalidade fixa por dia alocado: W = sum (10 - omega_p,d) * y_p,d
    POR_MINUTO_AULA     ///< Penalidade proporcional aos minutos letivos: W_min = sum (10 - omega_p,d) * m_p,d
};

/**
 * @brief Converte o enum ModeloPreferenciaDias para string.
 * @param m Modelo a ser convertido.
 * @return String representativa do modelo.
 */
inline std::string modeloPreferenciaDiasToString(ModeloPreferenciaDias m) {
    switch (m) {
        case ModeloPreferenciaDias::POR_DIA_TRABALHADO: return "POR_DIA_TRABALHADO";
        case ModeloPreferenciaDias::POR_MINUTO_AULA: return "POR_MINUTO_AULA";
    }
    return "POR_DIA_TRABALHADO";
}

/**
 * @brief Converte uma string no enum ModeloPreferenciaDias correspondente.
 * @param str Identificação textual do modelo.
 * @return ModeloPreferenciaDias correspondente.
 */
inline ModeloPreferenciaDias stringToModeloPreferenciaDias(const std::string& str) {
    if (str == "POR_MINUTO_AULA") return ModeloPreferenciaDias::POR_MINUTO_AULA;
    return ModeloPreferenciaDias::POR_DIA_TRABALHADO;
}

/**
 * @enum SolverAlgorithm
 * @brief Algoritmos disponíveis para a resolução do escalonamento de horários.
 */
enum class SolverAlgorithm {
    CP_SAT,             ///< Google OR-Tools CP-SAT (Constraint Programming) - Algoritmo exato recomendado.
    GREEDY_HEURISTIC,   ///< Heurística Construtiva Gulosa (Greedy Search) - Resolução rápida por prioridades.
    MILP,               ///< Programação Linear Inteira Mista (Mixed Integer Linear Programming).
    GENETIC_ALGORITHM   ///< Algoritmo Genético / Meta-heurística populacional.
};

/**
 * @brief Converte o enum SolverAlgorithm em identificador textual ("CP_SAT", "GREEDY_HEURISTIC", etc.).
 * @param algo Algoritmo de resolução.
 * @return String com a chave do algoritmo.
 */
inline std::string solverAlgorithmToString(SolverAlgorithm algo) {
    switch (algo) {
        case SolverAlgorithm::CP_SAT: return "CP_SAT";
        case SolverAlgorithm::GREEDY_HEURISTIC: return "GREEDY_HEURISTIC";
        case SolverAlgorithm::MILP: return "MILP";
        case SolverAlgorithm::GENETIC_ALGORITHM: return "GENETIC_ALGORITHM";
    }
    return "CP_SAT";
}

/**
 * @brief Converte uma string na enumeração SolverAlgorithm correspondente.
 * @param str Chave do algoritmo.
 * @return Valor de SolverAlgorithm correspondente.
 */
inline SolverAlgorithm stringToSolverAlgorithm(const std::string& str) {
    if (str == "GREEDY_HEURISTIC") return SolverAlgorithm::GREEDY_HEURISTIC;
    if (str == "MILP") return SolverAlgorithm::MILP;
    if (str == "GENETIC_ALGORITHM") return SolverAlgorithm::GENETIC_ALGORITHM;
    return SolverAlgorithm::CP_SAT;
}

/**
 * @brief Retorna uma descrição amigável para exibição em menus do algoritmo de resolução.
 * @param algo Algoritmo selecionado.
 * @return String com o nome formal do algoritmo.
 */
inline std::string solverAlgorithmDisplayName(SolverAlgorithm algo) {
    switch (algo) {
        case SolverAlgorithm::CP_SAT: return "Google OR-Tools CP-SAT (Restrições / Exato)";
        case SolverAlgorithm::GREEDY_HEURISTIC: return "Heurística Construtiva Gulosa (Rápido / Prioridade)";
        case SolverAlgorithm::MILP: return "Programação Linear Inteira Mista (MILP)";
        case SolverAlgorithm::GENETIC_ALGORITHM: return "Algoritmo Genético (Meta-heurística)";
    }
    return "Google OR-Tools CP-SAT";
}

/**
 * @enum EscopoSemestreOtimizacao
 * @brief Define quais disciplinas/semestres serão abrangidos pelo escalonador.
 */
enum class EscopoSemestreOtimizacao {
    TODAS_SEM_FILTRO,   ///< Todas as disciplinas cadastradas (modo geral unificado).
    SEMESTRE_1,         ///< Apenas 1º Semestre (disciplinas anuais + semestrais do 1º semestre).
    SEMESTRE_2,         ///< Apenas 2º Semestre (disciplinas anuais + semestrais do 2º semestre).
    AMBOS_INTEGRADO     ///< Ambos os Semestres (otimização multi-semestre integrada com sincronização).
};

/**
 * @brief Converte o enum EscopoSemestreOtimizacao em string representativa.
 * @param e Escopo de semestre a ser convertido.
 * @return String com a chave do escopo.
 */
inline std::string escopoSemestreToString(EscopoSemestreOtimizacao e) {
    switch (e) {
        case EscopoSemestreOtimizacao::TODAS_SEM_FILTRO: return "TODAS_SEM_FILTRO";
        case EscopoSemestreOtimizacao::SEMESTRE_1: return "SEMESTRE_1";
        case EscopoSemestreOtimizacao::SEMESTRE_2: return "SEMESTRE_2";
        case EscopoSemestreOtimizacao::AMBOS_INTEGRADO: return "AMBOS_INTEGRADO";
    }
    return "TODAS_SEM_FILTRO";
}

/**
 * @brief Converte uma representação textual no enum EscopoSemestreOtimizacao correspondente.
 * @param str Chave ou nome do escopo semestral.
 * @return EscopoSemestreOtimizacao correspondente.
 */
inline EscopoSemestreOtimizacao stringToEscopoSemestre(const std::string& str) {
    if (str == "SEMESTRE_1" || str == "1" || str == "1º Semestre") return EscopoSemestreOtimizacao::SEMESTRE_1;
    if (str == "SEMESTRE_2" || str == "2" || str == "2º Semestre") return EscopoSemestreOtimizacao::SEMESTRE_2;
    if (str == "AMBOS_INTEGRADO" || str == "AMBOS" || str == "INTEGRADO") return EscopoSemestreOtimizacao::AMBOS_INTEGRADO;
    return EscopoSemestreOtimizacao::TODAS_SEM_FILTRO;
}

/**
 * @brief Retorna uma descrição amigável para interface do escopo semestral configurado.
 * @param e Escopo semestral avaliado.
 * @return String descritiva do escopo.
 */
inline std::string escopoSemestreDisplayName(EscopoSemestreOtimizacao e) {
    switch (e) {
        case EscopoSemestreOtimizacao::TODAS_SEM_FILTRO: return "Todas as Disciplinas (Geral)";
        case EscopoSemestreOtimizacao::SEMESTRE_1: return "1º Semestre (Anuais + 1º Sem)";
        case EscopoSemestreOtimizacao::SEMESTRE_2: return "2º Semestre (Anuais + 2º Sem)";
        case EscopoSemestreOtimizacao::AMBOS_INTEGRADO: return "Ambos os Semestres (Multi-Semestre Integrado)";
    }
    return "Todas as Disciplinas (Geral)";
}

/**
 * @struct OptimizationConfig
 * @brief Configurações globais de otimização matemática e parâmetros do solver.
 */
struct OptimizationConfig {
    SolverAlgorithm algoritmo{SolverAlgorithm::CP_SAT}; ///< Algoritmo de resolução selecionado.

    EscopoSemestreOtimizacao escopo_semestre{EscopoSemestreOtimizacao::TODAS_SEM_FILTRO}; ///< Escopo semestral a otimizar.
    bool sincronizar_anuais_entre_semestres{true}; ///< Garante horários idênticos para anuais no 1º e 2º sem.

    // Pesos institucionais da função objetivo
    int lambda_janelas{8};            ///< Peso para reduzir janelas entre aulas dos professores (J).
    int lambda_dias{5};               ///< Peso para concentrar aulas em menos dias na semana (D).
    int lambda_preferencias{3};       ///< Peso para respeitar preferências horárias dos professores (P).
    int lambda_preferencia_dias{7};   ///< Peso para priorizar dias preferidos dos professores (W).
    int lambda_preferencia_turnos{8}; ///< Peso para priorizar turnos pretendidos dos professores (T).
    int lambda_lacunas_turma{2};      ///< Peso para reduzir lacunas ou buracos na grade das turmas (JG).

    ModeloPreferenciaDias modelo_preferencia_dias{ModeloPreferenciaDias::POR_DIA_TRABALHADO}; ///< Modelo de cálculo do peso W.

    // Restrições adicionais
    bool preenchimento_integral_jornada{false}; ///< Exige que todo horário da jornada da turma tenha aula alocada.
    bool distribuir_disciplinas_na_semana{true}; ///< Tenta espalhar aulas da mesma matéria em dias distintos.
    int max_aulas_disciplina_dia_turma{2};      ///< Teto diário de aulas da mesma matéria para uma turma.

    // Parâmetros de execução do solver
    double tempo_limite_segundos{30.0}; ///< Tempo limite máximo de execução da busca em segundos.
    int num_threads{4};                  ///< Quantidade de threads de CPU utilizadas em paralelo.
    bool log_search_progress{false};    ///< Se true, imprime o log interno de progresso do solver.

    bool usar_distribuicao_como_entrada{true}; ///< Usa a grade horária atual como solução inicial (Warm Start).
};

} // namespace scheduler::model
