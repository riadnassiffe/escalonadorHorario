#pragma once

#include <string>

namespace scheduler::model {

enum class ModeloPreferenciaDias {
    POR_DIA_TRABALHADO, // Padrão: W = sum (10 - omega_p,d) * y_p,d
    POR_MINUTO_AULA     // Alternativa: W_min = sum (10 - omega_p,d) * m_p,d
};

inline std::string modeloPreferenciaDiasToString(ModeloPreferenciaDias m) {
    switch (m) {
        case ModeloPreferenciaDias::POR_DIA_TRABALHADO: return "POR_DIA_TRABALHADO";
        case ModeloPreferenciaDias::POR_MINUTO_AULA: return "POR_MINUTO_AULA";
    }
    return "POR_DIA_TRABALHADO";
}

inline ModeloPreferenciaDias stringToModeloPreferenciaDias(const std::string& str) {
    if (str == "POR_MINUTO_AULA") return ModeloPreferenciaDias::POR_MINUTO_AULA;
    return ModeloPreferenciaDias::POR_DIA_TRABALHADO;
}

struct OptimizationConfig {
    // Pesos institucionais da otimização (seção 4 & 8)
    int lambda_janelas{8};            // Reduzir janelas entre aulas dos professores (J)
    int lambda_dias{5};               // Concentrar aulas em menos dias (D)
    int lambda_preferencias{3};       // Respeitar preferências de horários (P)
    int lambda_preferencia_dias{7};   // Priorizar dias preferidos dos professores (W)
    int lambda_lacunas_turma{2};      // Reduzir lacunas na grade das turmas (JG)

    ModeloPreferenciaDias modelo_preferencia_dias{ModeloPreferenciaDias::POR_DIA_TRABALHADO};

    // Restrições adicionais
    bool preenchimento_integral_jornada{false}; // Seção 3.10
    bool distribuir_disciplinas_na_semana{true}; // Seção 5
    int max_aulas_disciplina_dia_turma{2};

    // Parâmetros do Solver CP-SAT
    double tempo_limite_segundos{30.0};
    int num_threads{4};
    bool log_search_progress{false};
};

} // namespace scheduler::model
