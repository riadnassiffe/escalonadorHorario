#pragma once

/**
 * @file MilpModel.h
 * @brief Estruturas e construtores da formulação MILP compartilhada por SCIP, HiGHS, MILP e cuOpt.
 */

#include "model/InstitutionData.h"
#include "model/Timetable.h"
#include "solver/CandidateAllocation.h"
#include "solver/ScheduleSolver.h"
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace scheduler::solver::detail {

inline constexpr double kMilpInf = 1e20;

struct MilpVar {
    std::string name;
    double lb{0.0};
    double ub{1.0};
    double obj{0.0};
};

struct MilpRow {
    std::string name;
    std::vector<int> cols;
    std::vector<double> vals;
    double lhs{-kMilpInf};
    double rhs{kMilpInf};
};

struct MilpCandItem {
    const CandidateAllocation* cand;
    int col;
};

struct GapAuxRecord {
    std::vector<int> before_cols;
    std::vector<int> after_cols;
    std::vector<int> cover_cols;
    int u_col;
    int v_col;
    int g_col;
};

struct ShiftAuxRecord {
    std::vector<int> shift_cols;
    int y_shift_col;
};

struct ConsecAuxRecord {
    std::vector<int> day_cols;
    int w_d_col;
};

struct ConsecPairAuxRecord {
    std::vector<int> slot_a_cols;
    std::vector<int> slot_b_cols;
    int p_col;
};

struct MilpProblem {
    std::map<std::string, std::vector<CandidateAllocation>> todos_candidatos;
    std::vector<MilpVar> vars;
    std::vector<MilpRow> rows;

    std::vector<MilpCandItem> todas_vars;
    std::map<std::string, std::vector<MilpCandItem>> vars_por_aula;
    std::map<std::pair<std::string, std::string>, int> z_turma_sala;
    std::map<std::pair<std::string, model::DayOfWeek>, int> y_prof_dia;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<MilpCandItem>> vars_prof_dia;

    std::vector<std::pair<int, int>> j_terms;   // (g_col, pesoJanela * seg_min)
    std::vector<std::pair<int, int>> jg_terms;  // (q_col, slot_min)
    std::vector<std::pair<int, int>> p_terms;   // (x_col, custo_preferencia)

    std::vector<GapAuxRecord> gap_records;
    std::vector<ShiftAuxRecord> shift_records;
    std::vector<ConsecAuxRecord> consec_records;
    std::vector<ConsecPairAuxRecord> consec_pair_records;

    std::vector<double> warm_start_vals;
    bool has_warm_start{false};
    int hints_aplicados{0};

    int addBinVar(const std::string& name, double obj = 0.0, double lb = 0.0, double ub = 1.0);

    void addRow(const std::string& name,
                std::vector<std::pair<int, double>> terms,
                double lhs,
                double rhs);
};

/**
 * @brief Constrói a formulação matemática MILP completa compartilhada pelos motores MILP.
 */
bool buildMilpProblem(const model::InstitutionData& data,
                      MilpProblem& milp,
                      model::Timetable& timetable,
                      ProgressCallback callback,
                      const std::string& solver_tag,
                      const model::Timetable* heuristic_seed = nullptr);

/**
 * @brief Extrai a grade horária e estatísticas a partir do vetor solução de um solver MILP.
 */
void extractMilpSolution(const model::InstitutionData& data,
                         const MilpProblem& milp,
                         const std::vector<double>& sol_vals,
                         double obj_val,
                         double tempo_ms,
                         const std::string& status_str,
                         model::Timetable& timetable);

} // namespace scheduler::solver::detail
