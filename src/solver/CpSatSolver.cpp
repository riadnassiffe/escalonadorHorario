/**
 * @file CpSatSolver.cpp
 * @brief Implementação do algoritmo de escalonamento via Google OR-Tools CP-SAT.
 */

#include "solver/ScheduleSolver.h"
#include "solver/SolverCommon.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/sat_parameters.pb.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <string>
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
                                   "_" + std::to_string(c.start_slot) + "_" + std::to_string(var_idx++);
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

    // Vinculação x_{a,d,t,{r}} <= z_{g,r} para aulas teóricas (seção 3.6)
    for (const auto& item : todas_vars) {
        const auto* aula = data.findAula(item.cand->aula_id);
        if (aula && aula->isTeorica() && aula->turmas().size() == 1 && item.cand->salas.size() == 1) {
            std::string g_id = aula->turmas().front();
            std::string r_id = item.cand->salas.front();
            auto it_z = z_turma_sala.find({g_id, r_id});
            if (it_z != z_turma_sala.end()) {
                cp_model.AddLessOrEqual(item.var, it_z->second);
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
                        item.cand->start_slot,
                        item.cand->duration_slots,
                        item.cand->end_slot(),
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
                        item.cand->start_slot,
                        item.cand->duration_slots,
                        item.cand->end_slot(),
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
                        item.cand->start_slot,
                        item.cand->duration_slots,
                        item.cand->end_slot(),
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

    // 5.4b Início obrigatório da primeira aula no início da jornada da turma (seção 3.5 / [H11]):
    // Toda turma que possuir aulas alocadas em um dia letivo deve obrigatoriamente
    // ter sua primeira aula iniciando no slot inicial da jornada (T_ini).
    auto add_turma_inicio_obrigatorio = [&](const std::map<std::pair<std::string, model::DayOfWeek>, std::vector<CandItem>>& map_vars) {
        for (const auto& turma : data.turmas()) {
            for (auto dia : turma.diasLetivos()) {
                auto it = map_vars.find({turma.id(), dia});
                if (it == map_vars.end() || it->second.empty()) continue;

                auto jornada = turma.jornadaParaDia(dia);
                int t_ini = jornada.jornada_total.start_slot;

                LinearExpr sum_start;
                std::vector<BoolVar> later_vars;

                for (const auto& item : it->second) {
                    if (item.cand->start_slot == t_ini) {
                        sum_start += item.var;
                    } else if (item.cand->start_slot > t_ini) {
                        later_vars.push_back(item.var);
                    }
                }

                // Para cada aula que inicia após t_ini, ela só pode ser alocada no dia
                // se houver uma aula iniciando exatamente no horário inicial da jornada (t_ini).
                for (const auto& var_later : later_vars) {
                    cp_model.AddLessOrEqual(var_later, sum_start);
                }
            }
        }
    };

    if (is_multi_sem) {
        add_turma_inicio_obrigatorio(vars_turma_dia_s1);
        add_turma_inicio_obrigatorio(vars_turma_dia_s2);
    } else {
        add_turma_inicio_obrigatorio(vars_turma_dia);
    }

    // 5.5 Carga máxima diária do professor (seção 3.9)
    auto add_prof_carga_max = [&](const std::map<std::pair<std::string, model::DayOfWeek>, std::vector<CandItem>>& map_vars) {
        for (const auto& prof : data.professores()) {
            if (prof.cargaMaximaDiariaMinutos() > 0) {
                for (auto dia : model::allDays()) {
                    auto it = map_vars.find({prof.id(), dia});
                    if (it == map_vars.end()) continue;
                    LinearExpr minutos_dia;
                    for (const auto& item : it->second) {
                        minutos_dia += item.cand->duration_minutes * item.var;
                    }
                    cp_model.AddLessOrEqual(minutos_dia, prof.cargaMaximaDiariaMinutos());
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
                for (auto dia : turma.diasLetivos()) {
                    auto jornada = turma.jornadaParaDia(dia);
                    auto blocos = jornada.blocosLetivos();
                    auto it = map_vars.find({turma.id(), dia});
                    if (it == map_vars.end()) continue;
                    const auto& items = it->second;

                    for (const auto& bloco : blocos) {
                        for (int k = bloco.start_slot; k < bloco.end_slot; ++k) {
                            LinearExpr slot_occupancy;
                            for (const auto& item : items) {
                                if (item.cand->start_slot <= k && k < item.cand->end_slot()) {
                                    slot_occupancy += item.var;
                                }
                            }
                            cp_model.AddEquality(slot_occupancy, 1);
                        }
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

    // 5.7 Distribuição semanal de disciplinas (seção 5):
    // Limitar máximo de aulas da mesma disciplina por dia para cada turma
    if (data.config().distribuir_disciplinas_na_semana) {
        for (const auto& disc : data.disciplinas()) {
            int max_por_dia = disc.maxAulasPorDiaTurma() > 0 ? disc.maxAulasPorDiaTurma() : data.config().max_aulas_disciplina_dia_turma;
            if (max_por_dia > 0) {
                for (const auto& t_id : disc.turmas()) {
                    (void)t_id;
                    for (auto dia : model::allDays()) {
                        LinearExpr aulas_disc_dia;
                        for (int i = 1; i <= disc.aulasSemanais(); ++i) {
                            std::string aula_id = disc.id() + "_" + std::to_string(i);
                            const auto& items = vars_por_aula[aula_id];
                            for (const auto& item : items) {
                                if (item.cand->dia == dia) {
                                    aulas_disc_dia += item.var;
                                }
                            }
                        }
                        cp_model.AddLessOrEqual(aulas_disc_dia, max_por_dia);
                    }
                }
            }
        }
    }

    // 5.8 Disciplinas com aulas consecutivas em bloco (seção 5):
    for (const auto& disc : data.disciplinas()) {
        if (!disc.exigirAulasConsecutivas()) continue;
        int n_aulas = disc.aulasSemanais();

        if (n_aulas == 4) {
            // As 4 aulas devem ocorrer juntas em um único dia (bloco de 4)
            std::vector<BoolVar> dias_ativas;
            for (auto dia : model::allDays()) {
                BoolVar w_d = cp_model.NewBoolVar().WithName("w_d_" + disc.id() + "_" + std::to_string(static_cast<int>(dia)));
                dias_ativas.push_back(w_d);

                LinearExpr aulas_disc_dia;
                for (int i = 1; i <= n_aulas; ++i) {
                    std::string aula_id = disc.id() + "_" + std::to_string(i);
                    const auto& items = vars_por_aula[aula_id];
                    for (const auto& item : items) {
                        if (item.cand->dia == dia) {
                            aulas_disc_dia += item.var;
                        }
                    }
                }
                cp_model.AddEquality(aulas_disc_dia, 4 * w_d);
            }
            LinearExpr sum_dias;
            for (auto w : dias_ativas) sum_dias += w;
            cp_model.AddEquality(sum_dias, 1);
        } else if (n_aulas == 2) {
            // As 2 aulas devem ocorrer juntas em um único dia (bloco de 2)
            std::vector<BoolVar> dias_ativas;
            for (auto dia : model::allDays()) {
                BoolVar w_d = cp_model.NewBoolVar().WithName("w_d_" + disc.id() + "_" + std::to_string(static_cast<int>(dia)));
                dias_ativas.push_back(w_d);

                LinearExpr aulas_disc_dia;
                for (int i = 1; i <= n_aulas; ++i) {
                    std::string aula_id = disc.id() + "_" + std::to_string(i);
                    const auto& items = vars_por_aula[aula_id];
                    for (const auto& item : items) {
                        if (item.cand->dia == dia) {
                            aulas_disc_dia += item.var;
                        }
                    }
                }
                cp_model.AddEquality(aulas_disc_dia, 2 * w_d);
            }
            LinearExpr sum_dias;
            for (auto w : dias_ativas) sum_dias += w;
            cp_model.AddEquality(sum_dias, 1);
        } else if (n_aulas == 8) {
            // Deve ocorrer em 2 dias com 4 aulas cada (2 blocos de 4)
            std::vector<BoolVar> dias_ativas;
            for (auto dia : model::allDays()) {
                BoolVar w_d = cp_model.NewBoolVar().WithName("w_d_" + disc.id() + "_" + std::to_string(static_cast<int>(dia)));
                dias_ativas.push_back(w_d);

                LinearExpr aulas_disc_dia;
                for (int i = 1; i <= n_aulas; ++i) {
                    std::string aula_id = disc.id() + "_" + std::to_string(i);
                    const auto& items = vars_por_aula[aula_id];
                    for (const auto& item : items) {
                        if (item.cand->dia == dia) {
                            aulas_disc_dia += item.var;
                        }
                    }
                }
                cp_model.AddEquality(aulas_disc_dia, 4 * w_d);
            }
            LinearExpr sum_dias;
            for (auto w : dias_ativas) sum_dias += w;
            cp_model.AddEquality(sum_dias, 2);
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
                    if (model::slotToShift(item.cand->start_slot) == shift) {
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
                    for (const auto& item : items_shift) {
                        cp_model.AddLessOrEqual(item.var, y_shift);
                        sum_shift += item.var;
                    }
                    cp_model.AddLessOrEqual(y_shift, sum_shift);

                    expr_T += y_shift;
                }

                // Cálculo de janelas quebrado dentro deste turno (Termo J)
                // Ponderado pela vontade individual do professor (prof.pesoJanela())
                if (prof.pesoJanela() <= 0 || items_shift.size() <= 1) continue;

                int min_s = 288, max_e = 0;
                for (const auto& item : items_shift) {
                    min_s = std::min(min_s, item.cand->start_slot);
                    max_e = std::max(max_e, item.cand->end_slot());
                }

                for (int k = min_s; k < max_e; ++k) {
                    std::vector<BoolVar> cands_before;
                    std::vector<BoolVar> cands_after;
                    LinearExpr w_k;

                    for (const auto& item : items_shift) {
                        if (item.cand->start_slot <= k && k < item.cand->end_slot()) {
                            w_k += item.var;
                        }
                        if (item.cand->end_slot() <= k) {
                            cands_before.push_back(item.var);
                        }
                        if (item.cand->start_slot > k) {
                            cands_after.push_back(item.var);
                        }
                    }

                    if (!cands_before.empty() && !cands_after.empty()) {
                        BoolVar u = cp_model.NewBoolVar().WithName("u_" + std::to_string(gap_var_idx));
                        BoolVar v = cp_model.NewBoolVar().WithName("v_" + std::to_string(gap_var_idx));
                        BoolVar g = cp_model.NewBoolVar().WithName("g_" + std::to_string(gap_var_idx++));

                        for (const auto& b : cands_before) cp_model.AddLessOrEqual(b, u);
                        for (const auto& a : cands_after) cp_model.AddLessOrEqual(a, v);

                        cp_model.AddGreaterOrEqual(g, u + v - 1 - w_k);

                        // Pondera cada slot de janela de acordo com a vontade do professor (pesoJanela: 0 a 10)
                        expr_J += prof.pesoJanela() * g;
                    }
                }
            }
        }
    }

    // 6.4 Termo JG: Minimizar lacunas internas das turmas (seção 4.5)
    if (!data.config().preenchimento_integral_jornada && data.config().lambda_lacunas_turma > 0) {
        for (const auto& turma : data.turmas()) {
            for (auto dia : turma.diasLetivos()) {
                const auto& items = vars_turma_dia[{turma.id(), dia}];
                if (items.size() <= 1) continue;

                auto jornada = turma.jornadaParaDia(dia);
                auto blocos = jornada.blocosLetivos();

                for (const auto& bloco : blocos) {
                    for (int k = bloco.start_slot; k < bloco.end_slot; ++k) {
                        std::vector<BoolVar> cands_before;
                        std::vector<BoolVar> cands_after;
                        LinearExpr w_k;

                        for (const auto& item : items) {
                            if (item.cand->start_slot <= k && k < item.cand->end_slot()) {
                                w_k += item.var;
                            }
                            if (item.cand->end_slot() <= k) {
                                cands_before.push_back(item.var);
                            }
                            if (item.cand->start_slot > k) {
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
                            expr_JG += q;
                        }
                    }
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
    funcao_objetivo += data.config().lambda_lacunas_turma * (5 * expr_JG);

    cp_model.Minimize(funcao_objetivo);

    // 6.5 Inserção de Solução Inicial (Warm Start / Hint) a partir da grade atual em tela
    if (data.config().usar_distribuicao_como_entrada && !data.grade().alocacoes().empty()) {
        if (callback) callback("Aplicando distribuição atual em tela como solução inicial (Warm Start)...", 0.55);
        int hints_aplicados = 0;

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
                if (item.cand->dia == aloc->dia && item.cand->start_slot == aloc->intervalo.start_slot) {
                    if (item.cand->salas == aloc->salas || matching_item == nullptr) {
                        matching_item = &item;
                    }
                }
            }

            if (matching_item) {
                cp_model.AddHint(matching_item->var, true);
                hints_aplicados++;
                for (const auto& item : items) {
                    if (&item != matching_item) {
                        cp_model.AddHint(item.var, false);
                    }
                }
            }
        }

        // Hints para salas teóricas designadas
        for (const auto& [turma_id, sala_id] : data.grade().salasTeoricasDesignadas()) {
            auto it = z_turma_sala.find({turma_id, sala_id});
            if (it != z_turma_sala.end()) {
                cp_model.AddHint(it->second, true);
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

        timetable.addMensagemDiagnostico("[WARM-START] " + std::to_string(hints_aplicados) +
                                         " aulas da distribuição atual em tela foram fornecidas como solução inicial (Hint).");
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

    auto model_proto = cp_model.Build();
    const CpSolverResponse response = SolveWithParameters(model_proto, params);

    auto end_time = std::chrono::steady_clock::now();
    double tempo_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    model::TimetableStatistics stats;
    stats.tempo_execucao_ms = tempo_ms;
    stats.valor_objetivo = response.objective_value();
    stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());

    if (response.status() == CpSolverStatus::OPTIMAL) {
        stats.status = "OPTIMAL";
    } else if (response.status() == CpSolverStatus::FEASIBLE) {
        stats.status = "FEASIBLE";
    } else if (response.status() == CpSolverStatus::INFEASIBLE) {
        stats.status = "INFEASIBLE";
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
        int total_W = 0;
        for (const auto& prof : data.professores()) {
            for (auto dia : model::allDays()) {
                auto it = y_prof_dia.find({prof.id(), dia});
                if (it != y_prof_dia.end() && SolutionBooleanValue(response, it->second)) {
                    total_D++;
                    total_W += prof.penalidadeDia(dia);
                }
            }
        }
        stats.penalidade_dias_D = total_D;
        stats.penalidade_preferencia_dias_W = total_W;

        int total_J_slots = static_cast<int>(SolutionIntegerValue(response, expr_J));
        stats.penalidade_janelas_J = total_J_slots * 5;

        int total_JG_slots = static_cast<int>(SolutionIntegerValue(response, expr_JG));
        stats.penalidade_lacunas_turma_JG = total_JG_slots * 5;

        stats.penalidade_preferencias_P = static_cast<int>(SolutionIntegerValue(response, expr_P));

        timetable.setEstatisticas(stats);
        if (callback) callback("Grade gerada com sucesso!", 1.0);
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
    } else {
        timetable.setEstatisticas(stats);
        if (data.config().sem_limite_tempo) {
            timetable.addMensagemDiagnostico("[BUSCA_INTERROMPIDA] O solver CP-SAT finalizou a busca sem encontrar uma solução viável completa.");
            timetable.addMensagemDiagnostico("Dica: Verifique a disponibilidade dos professores, salas compatíveis ou flexibilize restrições.");
            if (callback) callback("Busca finalizada sem encontrar solução viável.", 1.0);
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
