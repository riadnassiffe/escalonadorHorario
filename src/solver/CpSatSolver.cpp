/**
 * @file CpSatSolver.cpp
 * @brief Implementação do algoritmo de escalonamento via Google OR-Tools CP-SAT.
 */

#include "solver/ScheduleSolver.h"
#include "solver/SolverCommon.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/time_limit.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace scheduler::solver {

using namespace operations_research;
using namespace operations_research::sat;

/**
 * @brief Resolve o escalonamento utilizando o solver CP-SAT do Google OR-Tools.
 *
 * Constrói modelo booleano com variáveis de alocação de candidatos e aplica
 * restrições rígidas (hard constraints) e ponderações de otimização (soft constraints).
 *
 * @param data Dados acadêmicos e parâmetros de configuração.
 * @param callback Callback opcional para progresso e cancelamento.
 * @return Grade horária gerada.
 */
model::Timetable ScheduleSolver::solveCpSat(const model::InstitutionData& data,
                                            ProgressCallback callback) {
    model::Timetable timetable;
    auto start_time = std::chrono::steady_clock::now();

    if (callback) callback("Validando dados acadêmicos...", 0.05);

    // 1. Validação prévia
    auto val_res = Preprocessor::validarDados(data);
    if (!val_res.is_valid) {
        model::TimetableStatistics stats;
        stats.status = "DADOS_INVALIDOS";
        timetable.setEstatisticas(stats);
        for (const auto& err : val_res.errors) {
            timetable.addMensagemDiagnostico("[ERRO] " + err);
        }
        for (const auto& w : val_res.warnings) {
            timetable.addMensagemDiagnostico("[AVISO] " + w);
        }
        return timetable;
    }

    if (callback) callback("Gerando candidatos de alocação (C_a)...", 0.15);

    // 2. Gerar candidatos para cada aula
    auto todos_candidatos = Preprocessor::gerarTodosCandidatos(data, val_res);
    if (!val_res.is_valid) {
        model::TimetableStatistics stats;
        stats.status = "INFEASIBLE";
        timetable.setEstatisticas(stats);
        for (const auto& err : val_res.errors) {
            timetable.addMensagemDiagnostico("[INVIABILIDADE] " + err);
        }
        return timetable;
    }

    if (callback) callback("Construindo modelo matemático CP-SAT...", 0.25);

    CpModelBuilder cp_model;

    // Estrutura para vincular candidato com sua variável BoolVar
    struct CandItem {
        const CandidateAllocation* cand;
        BoolVar var;
    };

    std::vector<CandItem> todas_vars;
    std::map<std::string, std::vector<CandItem>> vars_por_aula;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<CandItem>> vars_prof_dia;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<CandItem>> vars_turma_dia;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<CandItem>> vars_sala_dia;

    // Particionamento semestral para resolução multi-semestre integrada (seção 6)
    bool has_semestrais = false;
    for (const auto& a : data.aulas()) {
        if (a.isSemestral() || a.semestre() != model::SemestreLetivo::AMBOS) {
            has_semestrais = true;
            break;
        }
    }
    bool is_multi_sem = (data.config().escopo_semestre == model::EscopoSemestreOtimizacao::AMBOS_INTEGRADO ||
                        (data.config().escopo_semestre == model::EscopoSemestreOtimizacao::TODAS_SEM_FILTRO && has_semestrais));
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<CandItem>> vars_prof_dia_s1;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<CandItem>> vars_prof_dia_s2;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<CandItem>> vars_turma_dia_s1;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<CandItem>> vars_turma_dia_s2;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<CandItem>> vars_sala_dia_s1;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<CandItem>> vars_sala_dia_s2;

    // 3. Criar variáveis de decisão x_{a,d,t,b} (seção 2.6)
    int var_idx = 0;
    for (const auto& aula : data.aulas()) {
        const auto& cands = todos_candidatos[aula.id()];
        bool in_s1 = true;
        bool in_s2 = false;
        if (is_multi_sem) {
            if (aula.isAnual() || aula.semestre() == model::SemestreLetivo::AMBOS) {
                in_s1 = true;
                in_s2 = true; // Disciplina anual sincronizada nos dois semestres
            } else if (aula.semestre() == model::SemestreLetivo::SEMESTRE_2 || aula.semestreAtivo() == 2) {
                in_s1 = false;
                in_s2 = true;
            } else {
                in_s1 = true;
                in_s2 = false;
            }
        }

        for (const auto& c : cands) {
            std::string var_name = "x_" + aula.id() + "_" + std::to_string(static_cast<int>(c.dia)) +
                                   "_" + std::to_string(c.start_min) + "_" + std::to_string(var_idx++);
            BoolVar x = cp_model.NewBoolVar().WithName(var_name);
            CandItem item{&c, x};

            todas_vars.push_back(item);
            vars_por_aula[aula.id()].push_back(item);

            // Mapeamento por professores
            for (const auto& p_id : aula.professores()) {
                vars_prof_dia[{p_id, c.dia}].push_back(item);
                if (in_s1) vars_prof_dia_s1[{p_id, c.dia}].push_back(item);
                if (in_s2) vars_prof_dia_s2[{p_id, c.dia}].push_back(item);
            }
            // Mapeamento por turmas
            for (const auto& t_id : aula.turmas()) {
                vars_turma_dia[{t_id, c.dia}].push_back(item);
                if (in_s1) vars_turma_dia_s1[{t_id, c.dia}].push_back(item);
                if (in_s2) vars_turma_dia_s2[{t_id, c.dia}].push_back(item);
            }
            // Mapeamento por salas
            for (const auto& s_id : c.salas) {
                vars_sala_dia[{s_id, c.dia}].push_back(item);
                if (in_s1) vars_sala_dia_s1[{s_id, c.dia}].push_back(item);
                if (in_s2) vars_sala_dia_s2[{s_id, c.dia}].push_back(item);
            }
        }
    }

    // 4. Variáveis de sala teórica fixa por turma: z_{g,r} (seção 2.6 & 3.6)
    std::map<std::pair<std::string, std::string>, BoolVar> z_turma_sala;
    for (const auto& turma : data.turmas()) {
        if (turma.temSalaTeoricaPredefinida()) {
            std::string r = turma.salaTeoricaId();
            BoolVar z = cp_model.NewBoolVar().WithName("z_" + turma.id() + "_" + r);
            cp_model.AddEquality(z, 1);
            z_turma_sala[{turma.id(), r}] = z;
        } else if (!turma.salasTeoricasPermitidas().empty()) {
            std::vector<BoolVar> z_list;
            for (const auto& r : turma.salasTeoricasPermitidas()) {
                BoolVar z = cp_model.NewBoolVar().WithName("z_" + turma.id() + "_" + r);
                z_turma_sala[{turma.id(), r}] = z;
                z_list.push_back(z);
            }
            // Exatamente uma sala teórica fixa durante toda a semana (seção 3.6)
            LinearExpr sum_z;
            for (const auto& z : z_list) sum_z += z;
            cp_model.AddEquality(sum_z, 1);
        }
    }

    // Vinculação sum_{c in C_a : b_c = {r}} x_{a,c} <= z_{g,r} para aulas teóricas (seção 7 / R5)
    for (const auto& aula : data.aulas()) {
        if (aula.isPinned() || !aula.isTeorica() || aula.temSalasAlternativasExplicitas() || aula.turmas().size() != 1) continue;
        std::string g_id = aula.turmas().front();
        std::map<std::string, LinearExpr> expr_por_sala;
        for (const auto& item : vars_por_aula[aula.id()]) {
            if (item.cand->salas.size() == 1) {
                expr_por_sala[item.cand->salas.front()] += item.var;
            }
        }
        for (const auto& [r_id, sum_x] : expr_por_sala) {
            auto it_z = z_turma_sala.find({g_id, r_id});
            if (it_z != z_turma_sala.end()) {
                cp_model.AddLessOrEqual(sum_x, it_z->second);
            }
        }
    }

    // 5. Restrições Obrigatórias (Hard Constraints)

    // 5.1 Cada aula deve ser alocada exatamente uma vez (seção 3.1)
    for (const auto& aula : data.aulas()) {
        const auto& items = vars_por_aula[aula.id()];
        LinearExpr sum_alloc;
        for (const auto& item : items) {
            sum_alloc += item.var;
        }
        cp_model.AddEquality(sum_alloc, 1);
    }

    // 5.2 Não sobreposição de professores (seção 3.2)
    // Utiliza AddNoOverlap do CP-SAT com intervalos opcionais por semestre letivo
    int int_id = 0;
    auto add_prof_no_overlap = [&](const std::map<std::pair<std::string, model::DayOfWeek>, std::vector<CandItem>>& map_vars, const std::string& prefix) {
        for (const auto& prof : data.professores()) {
            for (auto dia : model::allDays()) {
                auto it = map_vars.find({prof.id(), dia});
                if (it == map_vars.end() || it->second.size() <= 1) continue;

                std::vector<IntervalVar> intervals;
                for (const auto& item : it->second) {
                    std::string name = prefix + "_p_" + prof.id() + "_" + std::to_string(int_id++);
                    IntervalVar interval = cp_model.NewOptionalIntervalVar(
                        item.cand->start_min,
                        item.cand->duration_minutes,
                        item.cand->end_min(),
                        item.var).WithName(name);
                    intervals.push_back(interval);
                }
                cp_model.AddNoOverlap(intervals);
            }
        }
    };

    if (is_multi_sem) {
        add_prof_no_overlap(vars_prof_dia_s1, "int_s1");
        add_prof_no_overlap(vars_prof_dia_s2, "int_s2");
    } else {
        add_prof_no_overlap(vars_prof_dia, "int");
    }

    // 5.3 Não sobreposição de salas (seção 3.3)
    auto add_sala_no_overlap = [&](const std::map<std::pair<std::string, model::DayOfWeek>, std::vector<CandItem>>& map_vars, const std::string& prefix) {
        for (const auto& sala : data.salas()) {
            for (auto dia : model::allDays()) {
                auto it = map_vars.find({sala.id(), dia});
                if (it == map_vars.end() || it->second.size() <= 1) continue;

                std::vector<IntervalVar> intervals;
                for (const auto& item : it->second) {
                    std::string name = prefix + "_r_" + sala.id() + "_" + std::to_string(int_id++);
                    IntervalVar interval = cp_model.NewOptionalIntervalVar(
                        item.cand->start_min,
                        item.cand->duration_minutes,
                        item.cand->end_min(),
                        item.var).WithName(name);
                    intervals.push_back(interval);
                }
                cp_model.AddNoOverlap(intervals);
            }
        }
    };

    if (is_multi_sem) {
        add_sala_no_overlap(vars_sala_dia_s1, "int_s1");
        add_sala_no_overlap(vars_sala_dia_s2, "int_s2");
    } else {
        add_sala_no_overlap(vars_sala_dia, "int");
    }

    // 5.4 Não sobreposição de turmas (seção 3.4)
    auto add_turma_no_overlap = [&](const std::map<std::pair<std::string, model::DayOfWeek>, std::vector<CandItem>>& map_vars, const std::string& prefix) {
        for (const auto& turma : data.turmas()) {
            for (auto dia : model::allDays()) {
                auto it = map_vars.find({turma.id(), dia});
                if (it == map_vars.end() || it->second.size() <= 1) continue;

                std::vector<IntervalVar> intervals;
                for (const auto& item : it->second) {
                    std::string name = prefix + "_t_" + turma.id() + "_" + std::to_string(int_id++);
                    IntervalVar interval = cp_model.NewOptionalIntervalVar(
                        item.cand->start_min,
                        item.cand->duration_minutes,
                        item.cand->end_min(),
                        item.var).WithName(name);
                    intervals.push_back(interval);
                }
                cp_model.AddNoOverlap(intervals);
            }
        }
    };

    if (is_multi_sem) {
        add_turma_no_overlap(vars_turma_dia_s1, "int_s1");
        add_turma_no_overlap(vars_turma_dia_s2, "int_s2");
    } else {
        add_turma_no_overlap(vars_turma_dia, "int");
    }

    // 5.5 Carga máxima diária do professor (seção 3.9)
    auto add_prof_carga_max = [&](const std::map<std::pair<std::string, model::DayOfWeek>, std::vector<CandItem>>& map_vars) {
        for (const auto& prof : data.professores()) {
            if (prof.cargaMaximaDiariaMinutos() > 0) {
                for (auto dia : model::allDays()) {
                    auto it = map_vars.find({prof.id(), dia});
                    if (it == map_vars.end()) continue;
                    LinearExpr minutos_dia;
                    int pinned_min = 0;
                    for (const auto& item : it->second) {
                        minutos_dia += item.cand->duration_minutes * item.var;
                        const auto* a_ptr = data.findAula(item.cand->aula_id);
                        if (a_ptr && a_ptr->isPinned()) {
                            pinned_min += item.cand->duration_minutes;
                        }
                    }
                    int limite_dia = std::max(prof.cargaMaximaDiariaMinutos(), pinned_min);
                    cp_model.AddLessOrEqual(minutos_dia, limite_dia);
                }
            }
        }
    };
    if (is_multi_sem) {
        add_prof_carga_max(vars_prof_dia_s1);
        add_prof_carga_max(vars_prof_dia_s2);
    } else {
        add_prof_carga_max(vars_prof_dia);
    }

    // 5.6 Preenchimento integral opcional da jornada (seção 3.10)
    if (data.config().preenchimento_integral_jornada) {
        auto add_preenchimento_integral = [&](const std::map<std::pair<std::string, model::DayOfWeek>, std::vector<CandItem>>& map_vars) {
            for (const auto& turma : data.turmas()) {
                bool has_unpinned = false;
                for (auto dia : turma.diasLetivos()) {
                    auto it_chk = map_vars.find({turma.id(), dia});
                    if (it_chk == map_vars.end()) continue;
                    for (const auto& item : it_chk->second) {
                        const auto* a_ptr = data.findAula(item.cand->aula_id);
                        if (!a_ptr || !a_ptr->isPinned()) {
                            has_unpinned = true;
                            break;
                        }
                    }
                    if (has_unpinned) break;
                }
                if (!has_unpinned) continue;

                for (auto dia : turma.diasLetivos()) {
                    auto jornada = turma.jornadaParaDia(dia);
                    auto it = map_vars.find({turma.id(), dia});
                    if (it == map_vars.end()) continue;
                    const auto& items = it->second;

                    for (const auto& slot : jornada.slots_letivos) {
                        LinearExpr slot_occupancy;
                        for (const auto& item : items) {
                            if (item.cand->start_min < slot.end_min && slot.start_min < item.cand->end_min()) {
                                slot_occupancy += item.var;
                            }
                        }
                        cp_model.AddEquality(slot_occupancy, 1);
                    }
                }
            }
        };
        if (is_multi_sem) {
            add_preenchimento_integral(vars_turma_dia_s1);
            add_preenchimento_integral(vars_turma_dia_s2);
        } else {
            add_preenchimento_integral(vars_turma_dia);
        }
    }

    // 5.7 Distribuição semanal de disciplinas (seção 7 / R11):
    // Limitar máximo de aulas da mesma disciplina por dia para cada turma
    if (data.config().distribuir_disciplinas_na_semana) {
        for (const auto& disc : data.disciplinas()) {
            if (vars_por_aula.find(disc.id() + "_1") == vars_por_aula.end()) continue;
            int max_por_dia = disc.maxAulasPorDiaTurma();
            if (max_por_dia > 0) {
                for (const auto& t_id : disc.turmas()) {
                    (void)t_id;
                    for (auto dia : model::allDays()) {
                        LinearExpr aulas_disc_dia;
                        bool has_day_vars = false;
                        int pinned_on_day = 0;
                        for (int i = 1; i <= disc.aulasSemanais(); ++i) {
                            std::string aula_id = disc.id() + "_" + std::to_string(i);
                            auto it_v = vars_por_aula.find(aula_id);
                            if (it_v == vars_por_aula.end()) continue;
                            const auto* a_ptr = data.findAula(aula_id);
                            for (const auto& item : it_v->second) {
                                if (item.cand->dia == dia) {
                                    aulas_disc_dia += item.var;
                                    has_day_vars = true;
                                    if (a_ptr && a_ptr->isPinned()) {
                                        pinned_on_day++;
                                    }
                                }
                            }
                        }
                        if (has_day_vars) {
                            int limite_dia = std::max(max_por_dia, pinned_on_day);
                            cp_model.AddLessOrEqual(aulas_disc_dia, limite_dia);
                        }
                    }
                }
            }
        }
    }

    // 5.8 Disciplinas com aulas consecutivas em bloco (seção 7 / R11):
    struct ConsecDayHint {
        std::vector<BoolVar> day_vars;
        BoolVar w_d;
    };
    std::vector<ConsecDayHint> consec_day_hints;

    if (data.config().exigir_aulas_consecutivas_rigido) {
    for (const auto& disc : data.disciplinas()) {
        if (!disc.exigirAulasConsecutivas()) continue;
        if (vars_por_aula.find(disc.id() + "_1") == vars_por_aula.end()) continue;
        int n_aulas = disc.aulasSemanais();
        int max_por_dia = disc.maxAulasPorDiaTurma();
        int teto_bloco = (data.config().distribuir_disciplinas_na_semana && max_por_dia > 0)
                             ? std::min(4, max_por_dia)
                             : 4;
        int block_size = 0;
        for (int b = teto_bloco; b >= 2; --b) {
            if (n_aulas % b == 0 && (n_aulas / b) <= static_cast<int>(model::allDays().size())) {
                block_size = b;
                break;
            }
        }
        if (block_size < 2) continue;
        int num_days = n_aulas / block_size;

        int pinned_total = 0;
        std::map<model::DayOfWeek, int> pinned_per_day;
        for (int i = 1; i <= n_aulas; ++i) {
            std::string aula_id = disc.id() + "_" + std::to_string(i);
            const auto* a_ptr = data.findAula(aula_id);
            if (a_ptr && a_ptr->isPinned()) {
                pinned_total++;
                pinned_per_day[a_ptr->pinnedDay()]++;
            }
        }
        bool pin_incompatible = (pinned_total == n_aulas) || (static_cast<int>(pinned_per_day.size()) > num_days);
        if (!pin_incompatible) {
            for (const auto& [d_pin, cnt_pin] : pinned_per_day) {
                (void)d_pin;
                if (cnt_pin > block_size) {
                    pin_incompatible = true;
                    break;
                }
            }
        }
        if (pin_incompatible) continue;

        std::vector<BoolVar> dias_ativas;
        for (auto dia : model::allDays()) {
            BoolVar w_d = cp_model.NewBoolVar().WithName("w_d_" + disc.id() + "_" + std::to_string(static_cast<int>(dia)));
            dias_ativas.push_back(w_d);

            LinearExpr aulas_disc_dia;
            std::vector<BoolVar> day_vars;
            for (int i = 1; i <= n_aulas; ++i) {
                std::string aula_id = disc.id() + "_" + std::to_string(i);
                auto it_v = vars_por_aula.find(aula_id);
                if (it_v == vars_por_aula.end()) continue;
                for (const auto& item : it_v->second) {
                    if (item.cand->dia == dia) {
                        aulas_disc_dia += item.var;
                        day_vars.push_back(item.var);
                    }
                }
            }
            cp_model.AddEquality(aulas_disc_dia, block_size * w_d);
            consec_day_hints.push_back(ConsecDayHint{std::move(day_vars), w_d});
        }
        LinearExpr sum_dias;
        for (auto w : dias_ativas) sum_dias += w;
        cp_model.AddEquality(sum_dias, num_days);
    }
    }

    if (callback) callback("Configurando função objetivo e preferências...", 0.40);

    // 6. Função Objetivo (seção 4 & 8):
    // min Z = lambda_1 * J + lambda_2 * D + lambda_3 * P + lambda_4 * J_G + lambda_5 * W

    LinearExpr expr_D;   // Dias de trabalho
    LinearExpr expr_W;   // Penalidades de dias menos preferidos
    LinearExpr expr_P;   // Preferências de horários
    LinearExpr expr_J;   // Janelas de professores (calculadas por turno e ponderadas pelo peso do professor)
    LinearExpr expr_T;   // Penalidades por turnos não pretendidos pelos professores
    LinearExpr expr_JG;  // Lacunas de turmas
    LinearExpr expr_C;   // Benefício de pares de aulas consecutivas

    struct ConsecPairHint {
        std::string disc_id;
        model::DayOfWeek dia;
        model::TimeInterval slot_a;
        model::TimeInterval slot_b;
        BoolVar var;
    };
    struct ShiftHint {
        std::vector<BoolVar> shift_vars;
        BoolVar y_shift;
    };
    struct GapHint {
        std::vector<BoolVar> before_vars;
        std::vector<BoolVar> after_vars;
        std::vector<BoolVar> cover_vars;
        BoolVar u;
        BoolVar v;
        BoolVar g;
    };
    std::vector<ConsecPairHint> consec_pair_hints;
    std::vector<ShiftHint> shift_hints;
    std::vector<GapHint> gap_hints;

    // 6.1 Variáveis y_{p,d}: indica se professor p trabalha no dia d (seção 4.2 & 4.4)
    std::map<std::pair<std::string, model::DayOfWeek>, BoolVar> y_prof_dia;
    for (const auto& prof : data.professores()) {
        for (auto dia : model::allDays()) {
            const auto& items = vars_prof_dia[{prof.id(), dia}];
            BoolVar y = cp_model.NewBoolVar().WithName("y_" + prof.id() + "_" + std::to_string(static_cast<int>(dia)));
            y_prof_dia[{prof.id(), dia}] = y;

            if (items.empty()) {
                cp_model.AddEquality(y, 0);
            } else {
                LinearExpr sum_items;
                for (const auto& item : items) {
                    cp_model.AddLessOrEqual(item.var, y); // y >= x
                    sum_items += item.var;
                }
                cp_model.AddLessOrEqual(y, sum_items); // y <= sum(x)

                // Termo D (seção 4.2): D = sum y_p,d
                expr_D += y;

                // Termo W (seção 4.4): priorizar os dias preferidos
                int pen_dia = prof.penalidadeDia(dia); // 10 - omega_{p,d}
                if (data.config().modelo_preferencia_dias == model::ModeloPreferenciaDias::POR_DIA_TRABALHADO) {
                    expr_W += pen_dia * y;
                } else {
                    // POR_MINUTO_AULA
                    LinearExpr minutos_p_d;
                    for (const auto& item : items) {
                        minutos_p_d += item.cand->duration_minutes * item.var;
                    }
                    expr_W += pen_dia * minutos_p_d;
                }
            }
        }
    }

    // 6.2 Termo P: Preferências de horários (seção 4.3)
    for (const auto& item : todas_vars) {
        if (item.cand->custo_preferencia > 0) {
            expr_P += item.cand->custo_preferencia * item.var;
        }
    }

    // 6.3 Termo J: Minimizar janelas entre aulas dos professores (quebrado por turno e ponderado pelo peso do professor)
    //     e Termo T: Penalizar alocações em turnos não pretendidos
    int gap_var_idx = 0;
    int shift_var_idx = 0;

    for (const auto& prof : data.professores()) {
        for (auto dia : model::allDays()) {
            const auto& items_dia = vars_prof_dia[{prof.id(), dia}];
            if (items_dia.empty()) continue;

            for (auto shift : model::allWorkShifts()) {
                std::vector<CandItem> items_shift;
                for (const auto& item : items_dia) {
                    if (model::timeToShift(item.cand->start_min) == shift) {
                        items_shift.push_back(item);
                    }
                }

                if (items_shift.empty()) continue;

                // Penalidade para turno não pretendido no dia (Termo T)
                if (!prof.pretendeTurno(dia, shift)) {
                    BoolVar y_shift = cp_model.NewBoolVar().WithName(
                        "y_shift_" + prof.id() + "_" + std::to_string(static_cast<int>(dia)) + "_" +
                        std::to_string(static_cast<int>(shift)) + "_" + std::to_string(shift_var_idx++));

                    LinearExpr sum_shift;
                    std::vector<BoolVar> shift_vars;
                    for (const auto& item : items_shift) {
                        cp_model.AddLessOrEqual(item.var, y_shift);
                        sum_shift += item.var;
                        shift_vars.push_back(item.var);
                    }
                    cp_model.AddLessOrEqual(y_shift, sum_shift);

                    expr_T += y_shift;
                    shift_hints.push_back(ShiftHint{std::move(shift_vars), y_shift});
                }

                // Cálculo de janelas em minutos dentro deste turno via segmentos elementares (Termo J)
                if (prof.pesoJanela() <= 0 || items_shift.size() <= 1 || data.config().lambda_janelas <= 0) continue;

                std::vector<int> pts;
                pts.reserve(items_shift.size() * 2);
                for (const auto& item : items_shift) {
                    pts.push_back(item.cand->start_min);
                    pts.push_back(item.cand->end_min());
                }
                std::sort(pts.begin(), pts.end());
                pts.erase(std::unique(pts.begin(), pts.end()), pts.end());

                for (size_t m = 0; m + 1 < pts.size(); ++m) {
                    int seg_s = pts[m];
                    int seg_e = pts[m + 1];
                    int dur = seg_e - seg_s;
                    if (dur <= 0) continue;

                    std::vector<BoolVar> cands_before;
                    std::vector<BoolVar> cands_after;
                    std::vector<BoolVar> cands_cover;
                    LinearExpr w_m;

                    for (const auto& item : items_shift) {
                        if (item.cand->start_min <= seg_s && seg_e <= item.cand->end_min()) {
                            w_m += item.var;
                            cands_cover.push_back(item.var);
                        }
                        if (item.cand->end_min() <= seg_s) {
                            cands_before.push_back(item.var);
                        }
                        if (item.cand->start_min >= seg_e) {
                            cands_after.push_back(item.var);
                        }
                    }

                    if (!cands_before.empty() && !cands_after.empty()) {
                        BoolVar u = cp_model.NewBoolVar().WithName("u_" + std::to_string(gap_var_idx));
                        BoolVar v = cp_model.NewBoolVar().WithName("v_" + std::to_string(gap_var_idx));
                        BoolVar g = cp_model.NewBoolVar().WithName("g_" + std::to_string(gap_var_idx++));

                        for (const auto& b : cands_before) cp_model.AddLessOrEqual(b, u);
                        for (const auto& a : cands_after) cp_model.AddLessOrEqual(a, v);

                        cp_model.AddGreaterOrEqual(g, u + v - 1 - w_m);

                        expr_J += (prof.pesoJanela() * dur) * g;
                        gap_hints.push_back(GapHint{
                            std::move(cands_before), std::move(cands_after), std::move(cands_cover), u, v, g});
                    }
                }
            }
        }
    }

    // 6.4 Termo JG: Minimizar lacunas internas nos slots letivos das turmas (em minutos)
    if (!data.config().preenchimento_integral_jornada && data.config().lambda_lacunas_turma > 0) {
        for (const auto& turma : data.turmas()) {
            for (auto dia : turma.diasLetivos()) {
                const auto& items = vars_turma_dia[{turma.id(), dia}];
                if (items.size() <= 1) continue;

                auto jornada = turma.jornadaParaDia(dia);
                for (const auto& slot : jornada.slots_letivos) {
                    int dur = slot.durationMinutes();
                    if (dur <= 0) continue;

                    std::vector<BoolVar> cands_before;
                    std::vector<BoolVar> cands_after;
                    std::vector<BoolVar> cands_cover;
                    LinearExpr w_k;

                    for (const auto& item : items) {
                        if (item.cand->start_min < slot.end_min && slot.start_min < item.cand->end_min()) {
                            w_k += item.var;
                            cands_cover.push_back(item.var);
                        }
                        if (item.cand->end_min() <= slot.start_min) {
                            cands_before.push_back(item.var);
                        }
                        if (item.cand->start_min >= slot.end_min) {
                            cands_after.push_back(item.var);
                        }
                    }

                    if (!cands_before.empty() && !cands_after.empty()) {
                        BoolVar u = cp_model.NewBoolVar();
                        BoolVar v = cp_model.NewBoolVar();
                        BoolVar q = cp_model.NewBoolVar();

                        for (const auto& b : cands_before) cp_model.AddLessOrEqual(b, u);
                        for (const auto& a : cands_after) cp_model.AddLessOrEqual(a, v);

                        cp_model.AddGreaterOrEqual(q, u + v - 1 - w_k);
                        expr_JG += dur * q;
                        gap_hints.push_back(GapHint{
                            std::move(cands_before), std::move(cands_after), std::move(cands_cover), u, v, q});
                    }
                }
            }
        }
    }

    // 6.4b Termo C: Benefício para pares de aulas consecutivas no mesmo turno
    if (data.config().lambda_aulas_consecutivas > 0) {
        for (const auto& disc : data.disciplinas()) {
            if (!disc.exigirAulasConsecutivas() || disc.aulasSemanais() < 2 || disc.turmas().empty()) continue;
            if (vars_por_aula.find(disc.id() + "_1") == vars_por_aula.end()) continue;

            const auto* turma = data.findTurma(disc.turmas().front());
            if (!turma) continue;

            std::map<std::tuple<model::DayOfWeek, int, int>, std::vector<BoolVar>> slot_vars;
            for (int i = 1; i <= disc.aulasSemanais(); ++i) {
                auto it_v = vars_por_aula.find(disc.id() + "_" + std::to_string(i));
                if (it_v == vars_por_aula.end()) continue;
                for (const auto& item : it_v->second) {
                    slot_vars[{item.cand->dia, item.cand->start_min, item.cand->end_min()}].push_back(item.var);
                }
            }

            for (auto dia : turma->diasLetivos()) {
                auto slots = turma->jornadaParaDia(dia).slots_letivos;
                std::sort(slots.begin(), slots.end());
                for (size_t k = 0; k + 1 < slots.size(); ++k) {
                    if (model::timeToShift(slots[k].start_min) != model::timeToShift(slots[k + 1].start_min)) {
                        continue;
                    }
                    auto it_a = slot_vars.find({dia, slots[k].start_min, slots[k].end_min});
                    auto it_b = slot_vars.find({dia, slots[k + 1].start_min, slots[k + 1].end_min});
                    if (it_a == slot_vars.end() || it_b == slot_vars.end()) continue;

                    BoolVar p = cp_model.NewBoolVar().WithName(
                        "c_" + disc.id() + "_" + std::to_string(static_cast<int>(dia)) + "_" + std::to_string(k));
                    LinearExpr sum_a, sum_b;
                    for (const auto& v : it_a->second) sum_a += v;
                    for (const auto& v : it_b->second) sum_b += v;
                    cp_model.AddLessOrEqual(p, sum_a);
                    cp_model.AddLessOrEqual(p, sum_b);

                    expr_C += p;
                    consec_pair_hints.push_back(ConsecPairHint{disc.id(), dia, slots[k], slots[k + 1], p});
                }
            }
        }
    }

    // Função Objetivo Global Ponderada
    LinearExpr funcao_objetivo;
    funcao_objetivo += data.config().lambda_janelas * expr_J;
    funcao_objetivo += data.config().lambda_dias * expr_D;
    funcao_objetivo += data.config().lambda_preferencias * expr_P;
    funcao_objetivo += data.config().lambda_preferencia_dias * expr_W;
    funcao_objetivo += data.config().lambda_preferencia_turnos * 10 * expr_T;
    funcao_objetivo += data.config().lambda_lacunas_turma * expr_JG;
    if (data.config().lambda_aulas_consecutivas > 0) {
        funcao_objetivo -= static_cast<int64_t>(data.config().lambda_aulas_consecutivas) * 50 * expr_C;
    }

    cp_model.Minimize(funcao_objetivo);

    // 6.5 Inserção de Solução Inicial (Warm Start / Hint) a partir da grade atual em tela
    if (data.config().usar_distribuicao_como_entrada && !data.grade().alocacoes().empty()) {
        if (callback) callback("Aplicando distribuição atual em tela como solução inicial (Warm Start)...", 0.55);
        int hints_aplicados = 0;
        std::set<int> hinted_true_idx;

        std::map<std::string, const model::AllocatedLesson*> aloc_map;
        for (const auto& a : data.grade().alocacoes()) {
            aloc_map[a.aula_id] = &a;
        }

        for (const auto& aula : data.aulas()) {
            auto it = aloc_map.find(aula.id());
            if (it == aloc_map.end()) continue;
            const auto* aloc = it->second;

            const auto& items = vars_por_aula[aula.id()];
            const CandItem* matching_item = nullptr;

            for (const auto& item : items) {
                if (item.cand->dia == aloc->dia && item.cand->start_min == aloc->intervalo.start_min &&
                    item.cand->end_min() == aloc->intervalo.end_min) {
                    if (item.cand->salas == aloc->salas || matching_item == nullptr) {
                        matching_item = &item;
                    }
                }
            }

            if (matching_item) {
                cp_model.AddHint(matching_item->var, true);
                hinted_true_idx.insert(matching_item->var.index());
                hints_aplicados++;
                for (const auto& item : items) {
                    if (&item != matching_item) {
                        cp_model.AddHint(item.var, false);
                    }
                }
            }
        }

        // Hints completos para salas teóricas designadas (z_{g,r})
        for (const auto& turma : data.turmas()) {
            std::string chosen_r;
            auto it_des = data.grade().salasTeoricasDesignadas().find(turma.id());
            if (it_des != data.grade().salasTeoricasDesignadas().end()) {
                chosen_r = it_des->second;
            } else if (turma.temSalaTeoricaPredefinida()) {
                chosen_r = turma.salaTeoricaId();
            } else if (!turma.salasTeoricasPermitidas().empty()) {
                chosen_r = turma.salasTeoricasPermitidas().front();
            }
            for (const auto& [par, z_var] : z_turma_sala) {
                if (par.first == turma.id()) {
                    cp_model.AddHint(z_var, par.second == chosen_r);
                }
            }
        }

        // Hints para dias de trabalho dos professores
        for (const auto& prof : data.professores()) {
            for (auto dia : model::allDays()) {
                bool trabalha = false;
                for (const auto& aloc : data.grade().alocacoes()) {
                    if (aloc.dia == dia) {
                        for (const auto& p : aloc.professores) {
                            if (p == prof.id()) { trabalha = true; break; }
                        }
                    }
                    if (trabalha) break;
                }
                auto it = y_prof_dia.find({prof.id(), dia});
                if (it != y_prof_dia.end()) {
                    cp_model.AddHint(it->second, trabalha);
                }
            }
        }

        if (!consec_pair_hints.empty()) {
            std::set<std::tuple<std::string, model::DayOfWeek, int, int>> aloc_set;
            for (const auto& aloc : data.grade().alocacoes()) {
                aloc_set.insert({aloc.disciplina_id, aloc.dia, aloc.intervalo.start_min, aloc.intervalo.end_min});
            }
            for (const auto& h : consec_pair_hints) {
                bool has_a = aloc_set.count({h.disc_id, h.dia, h.slot_a.start_min, h.slot_a.end_min}) > 0;
                bool has_b = aloc_set.count({h.disc_id, h.dia, h.slot_b.start_min, h.slot_b.end_min}) > 0;
                cp_model.AddHint(h.var, has_a && has_b);
            }
        }

        for (const auto& h : consec_day_hints) {
            bool active = false;
            for (const auto& v : h.day_vars) {
                if (hinted_true_idx.count(v.index()) > 0) { active = true; break; }
            }
            cp_model.AddHint(h.w_d, active);
        }

        for (const auto& h : shift_hints) {
            bool active = false;
            for (const auto& v : h.shift_vars) {
                if (hinted_true_idx.count(v.index()) > 0) { active = true; break; }
            }
            cp_model.AddHint(h.y_shift, active);
        }

        for (const auto& h : gap_hints) {
            bool u_val = false;
            for (const auto& b : h.before_vars) {
                if (hinted_true_idx.count(b.index()) > 0) { u_val = true; break; }
            }
            bool v_val = false;
            for (const auto& a : h.after_vars) {
                if (hinted_true_idx.count(a.index()) > 0) { v_val = true; break; }
            }
            bool cov_val = false;
            for (const auto& c : h.cover_vars) {
                if (hinted_true_idx.count(c.index()) > 0) { cov_val = true; break; }
            }
            cp_model.AddHint(h.u, u_val);
            cp_model.AddHint(h.v, v_val);
            cp_model.AddHint(h.g, u_val && v_val && !cov_val);
        }

        timetable.addMensagemDiagnostico("[WARM-START] " + std::to_string(hints_aplicados) +
                                         " aulas da distribuição atual em tela foram fornecidas como solução inicial (Hint).");
    }

    if (stop_requested_.load()) {
        model::TimetableStatistics stats;
        stats.tempo_execucao_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
        stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
        stats.status = "CANCELLED";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[CANCELADO] Otimização interrompida pelo usuário antes de iniciar a busca.");
        if (callback) callback("CP-SAT: Otimização cancelada pelo usuário.", 1.0);
        return timetable;
    }

    if (callback) callback("Executando solver CP-SAT...", 0.60);

    // 7. Parâmetros e Resolução
    SatParameters params;
    if (!data.config().sem_limite_tempo && data.config().tempo_limite_segundos > 0.0) {
        params.set_max_time_in_seconds(data.config().tempo_limite_segundos);
    }
    if (!data.config().sem_limite_memoria && data.config().max_memoria_mb > 0) {
        params.set_max_memory_in_mb(data.config().max_memoria_mb);
    }
    params.set_num_workers(data.config().num_threads);
    params.set_log_search_progress(data.config().log_search_progress);
    params.set_catch_sigint_signal(false);

    auto model_proto = cp_model.Build();
    Model sat_model;
    sat_model.Add(NewSatParameters(params));

    // O OR-Tools SharedTimeLimit::Stop() grava `true` no booleano externo registrado
    // quando a busca termina internamente (INFEASIBLE, OPTIMAL, limite de memória etc.).
    // Mantemos uma flag local isolada para que apenas requestStop() marque stop_requested_.
    std::atomic<bool> cp_sat_stop{false};
    sat_model.GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(&cp_sat_stop);
    if (callback) {
        sat_model.Add(NewFeasibleSolutionObserver([&callback](const CpSolverResponse& r) {
            callback("CP-SAT: Nova solução viável (objetivo = " +
                     std::to_string(static_cast<long long>(std::llround(r.objective_value()))) + ")...", 0.75);
        }));
    }

    CpSolverResponse response;
    bool skipped_before_solve = false;
    {
        struct CpSatInterruptGuard {
            ScheduleSolver* owner;
            CpSatInterruptGuard(ScheduleSolver* s, std::atomic<bool>* flag) : owner(s) {
                owner->setInterruptHandler([flag]() {
                    flag->store(true);
                });
            }
            ~CpSatInterruptGuard() {
                owner->clearInterruptHandler();
            }
        } guard(this, &cp_sat_stop);

        if (stop_requested_.load()) {
            cp_sat_stop.store(true);
            skipped_before_solve = true;
        } else {
            response = SolveCpModel(model_proto, &sat_model);
        }
    }

    auto end_time = std::chrono::steady_clock::now();
    double tempo_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    model::TimetableStatistics stats;
    stats.tempo_execucao_ms = tempo_ms;
    stats.valor_objetivo = skipped_before_solve ? 0.0 : response.objective_value();
    stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());

    if (skipped_before_solve) {
        stats.status = "CANCELLED";
    } else if (response.status() == CpSolverStatus::OPTIMAL) {
        stats.status = "OPTIMAL";
    } else if (response.status() == CpSolverStatus::FEASIBLE) {
        stats.status = "FEASIBLE";
    } else if (response.status() == CpSolverStatus::INFEASIBLE) {
        stats.status = "INFEASIBLE";
    } else if (response.status() == CpSolverStatus::MODEL_INVALID) {
        stats.status = "MODEL_INVALID";
    } else {
        stats.status = "UNKNOWN";
    }

    if (stats.status == "OPTIMAL" || stats.status == "FEASIBLE") {
        if (callback) callback("Extraindo solução ótima...", 0.90);

        // Extrair aulas selecionadas
        for (const auto& item : todas_vars) {
            if (SolutionBooleanValue(response, item.var)) {
                const auto* aula = data.findAula(item.cand->aula_id);
                model::AllocatedLesson aloc;
                aloc.aula_id = item.cand->aula_id;
                aloc.disciplina_id = aula ? aula->disciplinaId() : "";
                aloc.disciplina_nome = aula ? aula->disciplinaNome() : "";
                aloc.tipo = aula ? aula->tipo() : "TEORICA";
                aloc.dia = item.cand->dia;
                aloc.intervalo = item.cand->intervalo();
                aloc.professores = aula ? aula->professores() : std::vector<std::string>{};
                aloc.turmas = aula ? aula->turmas() : std::vector<std::string>{};
                aloc.salas = item.cand->salas;
                aloc.is_pinned = aula ? aula->isPinned() : false;
                if (aula) {
                    aloc.regime = aula->regime();
                    aloc.semestre = aula->semestre();
                    if (aula->isAnual()) {
                        aloc.semestre_alocado = 3; // Anual / Ambos
                    } else {
                        aloc.semestre_alocado = (aula->semestre() == model::SemestreLetivo::SEMESTRE_2 || aula->semestreAtivo() == 2) ? 2 : 1;
                    }
                }

                timetable.addAllocatedLesson(std::move(aloc));
            }
        }
        stats.total_aulas_alocadas = static_cast<int>(timetable.alocacoes().size());

        // Extrair designação de salas teóricas por turma (z_{g,r})
        for (const auto& [par, z_var] : z_turma_sala) {
            if (SolutionBooleanValue(response, z_var)) {
                timetable.setSalaTeoricaDesignada(par.first, par.second);
            }
        }

        // Calcular penalidades exatas para o relatório
        int total_D = 0;
        for (const auto& prof : data.professores()) {
            for (auto dia : model::allDays()) {
                auto it = y_prof_dia.find({prof.id(), dia});
                if (it != y_prof_dia.end() && SolutionBooleanValue(response, it->second)) {
                    total_D++;
                }
            }
        }
        stats.penalidade_dias_D = total_D;
        stats.penalidade_preferencia_dias_W = static_cast<int>(SolutionIntegerValue(response, expr_W));

        stats.penalidade_janelas_J = static_cast<int>(SolutionIntegerValue(response, expr_J));

        stats.penalidade_lacunas_turma_JG = static_cast<int>(SolutionIntegerValue(response, expr_JG));

        stats.penalidade_preferencias_P = static_cast<int>(SolutionIntegerValue(response, expr_P));

        stats.beneficio_consecutivas_C = detail::calcularBeneficioConsecutivas(data, timetable);

        timetable.setEstatisticas(stats);
        if (stop_requested_.load() && stats.status == "FEASIBLE") {
            timetable.addMensagemDiagnostico("[INTERROMPIDO] Otimização interrompida pelo usuário. Aplicada a melhor solução viável encontrada até o momento.");
            if (callback) callback("CP-SAT: Interrompido pelo usuário (melhor solução viável aplicada).", 1.0);
        } else {
            if (callback) callback("Grade gerada com sucesso!", 1.0);
        }
    } else if (stats.status == "INFEASIBLE") {
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[INVIABILIDADE] O solver CP-SAT determinou que o conjunto de restrições não possui solução viável.");
        timetable.addMensagemDiagnostico("Dica: Verifique a disponibilidade dos professores, salas compatíveis, ou flexibilize a jornada e limites diários.");

        // Simulação heurística para apontar exatamente quais aulas encontram restrições conflitantes
        auto diag_tt = solveGreedyHeuristic(data, nullptr);
        bool header_added = false;
        for (const auto& msg : diag_tt.mensagensDiagnostico()) {
            if (msg.find("Não foi possível alocar a aula") != std::string::npos) {
                if (!header_added) {
                    timetable.addMensagemDiagnostico("[DIAGNÓSTICO] Aulas com restrições conflitantes na simulação de alocação:");
                    header_added = true;
                }
                timetable.addMensagemDiagnostico(msg);
            }
        }
        if (callback) callback("Inviável: Sem solução para as restrições informadas.", 1.0);
    } else if (stats.status == "MODEL_INVALID") {
        timetable.setEstatisticas(stats);
        std::string info = response.solution_info();
        timetable.addMensagemDiagnostico(
            "[ERRO] Modelo CP-SAT inválido" + (info.empty() ? std::string(".") : ": " + info));
        if (callback) callback("CP-SAT: Modelo matemático inválido.", 1.0);
    } else if (stop_requested_.load()) {
        stats.status = "CANCELLED";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[CANCELADO] Otimização interrompida pelo usuário antes de encontrar uma solução viável.");
        if (callback) callback("CP-SAT: Otimização cancelada pelo usuário.", 1.0);
    } else {
        timetable.setEstatisticas(stats);
        const std::string sol_info = response.solution_info();
        if (!sol_info.empty()) {
            timetable.addMensagemDiagnostico("[CP-SAT] Detalhes do encerramento: " + sol_info);
        }
        const bool has_mem_limit = !data.config().sem_limite_memoria && data.config().max_memoria_mb > 0;
        if (data.config().sem_limite_tempo) {
            std::string msg = "[BUSCA_INTERROMPIDA] O solver CP-SAT finalizou a busca sem encontrar uma solução viável completa.";
            if (has_mem_limit) {
                msg += " (Limite de memória configurado: " + std::to_string(data.config().max_memoria_mb) + " MB.)";
            }
            timetable.addMensagemDiagnostico(msg);
            timetable.addMensagemDiagnostico("Dica: Verifique a disponibilidade dos professores, salas compatíveis, limite de memória ou flexibilize restrições.");
            if (callback) callback("Busca finalizada sem encontrar solução viável.", 1.0);
        } else if (data.config().tempo_limite_segundos > 0.0 &&
                   tempo_ms < data.config().tempo_limite_segundos * 1000.0 * 0.85) {
            std::string msg = "[LIMITE_MEMORIA_OU_BUSCA] O solver CP-SAT encerrou antes do tempo limite configurado (" +
                              model::formatDurationMs(tempo_ms) + " de " +
                              model::formatDurationSeconds(data.config().tempo_limite_segundos) + ") sem solução viável.";
            if (has_mem_limit) {
                msg += " Limite de memória configurado: " + std::to_string(data.config().max_memoria_mb) + " MB.";
            }
            timetable.addMensagemDiagnostico(msg);
            timetable.addMensagemDiagnostico("Dica: Aumente ou remova o limite de memória RAM, ou verifique restrições conflitantes nos logs.");
            if (callback) callback("CP-SAT encerrou antes do tempo limite sem solução viável.", 1.0);
        } else {
            timetable.addMensagemDiagnostico("[TEMPO_LIMITE] O tempo limite de resolução foi atingido antes de encontrar uma solução viável completa.");
            timetable.addMensagemDiagnostico("Dica: Aumente o tempo limite, selecione 'Sem restrição de tempo' ou utilize a Heurística Gulosa para gerar uma solução inicial rápida.");
            if (callback) callback("Tempo limite atingido sem encontrar solução viável.", 1.0);
        }
    }

    detail::auditarConflitosEntreAulas(data, timetable);
    return timetable;
}

} // namespace scheduler::solver
