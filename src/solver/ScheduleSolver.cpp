/**
 * @file ScheduleSolver.cpp
 * @brief Implementação dos algoritmos de resolução e otimização de grade horária.
 */

#include "solver/ScheduleSolver.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/sat_parameters.pb.h"
#include <Highs.h>
#include <scip/scip.h>
#include <scip/scipdefplugins.h>
#include <chrono>
#include <cmath>
#include <set>
#include <map>
#include <algorithm>
#include <random>
#include <iostream>

namespace scheduler::solver {

using namespace operations_research;
using namespace operations_research::sat;

/**
 * @brief Resolve o escalonamento chamando o algoritmo configurado na instituição.
 * @param data Conjunto de dados acadêmicos e configuração de otimização.
 * @param callback Callback opcional para notificação de progresso e status.
 * @return Grade horária gerada (Timetable) com as alocações e estatísticas.
 */
model::Timetable ScheduleSolver::solve(const model::InstitutionData& data,
                                       ProgressCallback callback) {
    switch (data.config().algoritmo) {
        case model::SolverAlgorithm::GREEDY_HEURISTIC:
            return solveGreedyHeuristic(data, callback);
        case model::SolverAlgorithm::SCIP:
            return solveScip(data, callback);
        case model::SolverAlgorithm::HIGHS:
            return solveHighs(data, callback);
        case model::SolverAlgorithm::MILP:
            return solveMilp(data, callback);
        case model::SolverAlgorithm::CP_SAT:
        default:
            return solveCpSat(data, callback);
    }
}

/**
 * @brief Realiza auditoria pós-resolução para identificar choques de horário.
 *
 * Avalia pares de aulas alocadas no mesmo dia e intervalo de tempo sobreposto,
 * verificando choques de professores, turmas ou salas no mesmo semestre letivo.
 * Em caso de choques, adiciona mensagens de aviso detalhadas no diagnóstico da grade.
 *
 * @param data Dados acadêmicos da instituição.
 * @param timetable Grade horária gerada a ser auditada e complementada com mensagens.
 */
static void auditarConflitosEntreAulas(const model::InstitutionData& data, model::Timetable& timetable) {
    const auto& alocs = timetable.alocacoes();
    auto time_overlaps = [](const model::TimeInterval& t1, const model::TimeInterval& t2) {
        return std::max(t1.start_slot, t2.start_slot) < std::min(t1.end_slot, t2.end_slot);
    };
    auto semestres_conflitam = [](const model::AllocatedLesson& a1, const model::AllocatedLesson& a2) {
        if (a1.regime == model::RegimeDisciplina::ANUAL || a2.regime == model::RegimeDisciplina::ANUAL) return true;
        int s1 = (a1.semestre == model::SemestreLetivo::SEMESTRE_2 || a1.semestre_alocado == 2) ? 2 : 1;
        int s2 = (a2.semestre == model::SemestreLetivo::SEMESTRE_2 || a2.semestre_alocado == 2) ? 2 : 1;
        return s1 == s2;
    };

    std::vector<std::string> conflitos_detectados;

    for (size_t i = 0; i < alocs.size(); ++i) {
        for (size_t j = i + 1; j < alocs.size(); ++j) {
            const auto& a1 = alocs[i];
            const auto& a2 = alocs[j];
            if (a1.dia == a2.dia && semestres_conflitam(a1, a2) && time_overlaps(a1.intervalo, a2.intervalo)) {
                // Professor
                for (const auto& p1 : a1.professores) {
                    if (std::find(a2.professores.begin(), a2.professores.end(), p1) != a2.professores.end()) {
                        const auto* p = data.findProfessor(p1);
                        conflitos_detectados.push_back("Choque de Professor: Aula '" + a1.disciplina_nome + "' (" + a1.aula_id + ") conflita com aula '" + a2.disciplina_nome + "' (" + a2.aula_id + ") do Prof. " + (p ? p->nome() : p1) + " na " + model::dayToLongString(a1.dia) + " às " + a1.horarioFormatado() + ".");
                    }
                }
                // Turma
                for (const auto& t1 : a1.turmas) {
                    if (std::find(a2.turmas.begin(), a2.turmas.end(), t1) != a2.turmas.end()) {
                        const auto* t = data.findTurma(t1);
                        conflitos_detectados.push_back("Choque de Turma: Aula '" + a1.disciplina_nome + "' (" + a1.aula_id + ") conflita com aula '" + a2.disciplina_nome + "' (" + a2.aula_id + ") da Turma " + (t ? t->nome() : t1) + " na " + model::dayToLongString(a1.dia) + " às " + a1.horarioFormatado() + ".");
                    }
                }
                // Sala
                for (const auto& r1 : a1.salas) {
                    if (std::find(a2.salas.begin(), a2.salas.end(), r1) != a2.salas.end()) {
                        const auto* r = data.findSala(r1);
                        conflitos_detectados.push_back("Choque de Sala: Aula '" + a1.disciplina_nome + "' (" + a1.aula_id + ") conflita com aula '" + a2.disciplina_nome + "' (" + a2.aula_id + ") na Sala " + (r ? r->nome() : r1) + " na " + model::dayToLongString(a1.dia) + " às " + a1.horarioFormatado() + ".");
                    }
                }
            }
        }
    }

    if (!conflitos_detectados.empty()) {
        timetable.addMensagemDiagnostico("[AUDITORIA DE GRADE] Foram detectados " + std::to_string(conflitos_detectados.size()) + " conflito(s) entre aulas alocadas:");
        for (const auto& c : conflitos_detectados) {
            timetable.addMensagemDiagnostico("  ⚠️ " + c);
        }
    }
}

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

    auditarConflitosEntreAulas(data, timetable);
    return timetable;
}

/**
 * @brief Resolve o escalonamento utilizando heurística gulosa com ordenação de prioridade.
 *
 * Ordena as aulas por restrição (grau de dificuldade de alocação) e as aloca
 * iterativamente nos melhores slots sem gerar choques duros com alocações anteriores.
 *
 * @param data Dados acadêmicos da instituição.
 * @param callback Callback opcional para atualização de progresso.
 * @return Grade horária gerada.
 */
model::Timetable ScheduleSolver::solveGreedyHeuristic(const model::InstitutionData& data,
                                                      ProgressCallback callback) {
    model::Timetable timetable;
    auto start_time = std::chrono::steady_clock::now();

    if (callback) callback("Validando dados acadêmicos...", 0.10);

    auto val_res = Preprocessor::validarDados(data);
    if (!val_res.is_valid) {
        model::TimetableStatistics stats;
        stats.status = "DADOS_INVALIDOS";
        timetable.setEstatisticas(stats);
        for (const auto& err : val_res.errors) timetable.addMensagemDiagnostico("[ERRO] " + err);
        for (const auto& w : val_res.warnings) timetable.addMensagemDiagnostico("[AVISO] " + w);
        return timetable;
    }

    if (callback) callback("Gerando candidatos de alocação (C_a)...", 0.25);
    auto todos_candidatos = Preprocessor::gerarTodosCandidatos(data, val_res);
    if (!val_res.is_valid) {
        model::TimetableStatistics stats;
        stats.status = "INFEASIBLE";
        timetable.setEstatisticas(stats);
        for (const auto& err : val_res.errors) timetable.addMensagemDiagnostico("[ERRO] " + err);
        return timetable;
    }

    if (callback) callback("Executando Heurística Construtiva Gulosa...", 0.40);

    struct SlotRange {
        int start;
        int end;
        int semester{3}; // 1 = 1º Sem, 2 = 2º Sem, 3 = Anual / Ambos
        std::string aula_id;
        std::string disciplina_nome;
        std::string turma_nome;
        std::string prof_nome;
        std::string sala_nome;
    };
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<SlotRange>> prof_ocup;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<SlotRange>> turma_ocup;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<SlotRange>> sala_ocup;
    std::map<std::tuple<std::string, std::string, model::DayOfWeek>, int> turma_disc_dia_count;
    std::map<std::tuple<std::string, std::string, model::DayOfWeek>, std::vector<SlotRange>> turma_disc_dia_aulas;
    std::map<std::string, std::string> turma_base_sala;

    std::vector<const model::Lesson*> sorted_aulas;
    for (const auto& aula : data.aulas()) {
        sorted_aulas.push_back(&aula);
    }
    std::sort(sorted_aulas.begin(), sorted_aulas.end(), [&](const model::Lesson* a, const model::Lesson* b) {
        if (a->isPinned() != b->isPinned()) return a->isPinned() > b->isPinned();
        size_t cands_a = todos_candidatos[a->id()].size();
        size_t cands_b = todos_candidatos[b->id()].size();
        if (cands_a != cands_b) return cands_a < cands_b;
        return a->duracaoMinutos() > b->duracaoMinutos();
    });

    int alocadas_count = 0;
    int total_aulas = static_cast<int>(sorted_aulas.size());

    auto semestres_conflitam = [](int s1, int s2) {
        if (s1 == 3 || s2 == 3) return true;
        return s1 == s2;
    };

    auto overlaps = [&](const std::vector<SlotRange>& ranges, int s, int e, int sem) {
        for (const auto& r : ranges) {
            if (semestres_conflitam(r.semester, sem)) {
                if (std::max(r.start, s) < std::min(r.end, e)) return true;
            }
        }
        return false;
    };

    std::map<std::string, const model::AllocatedLesson*> aloc_em_tela;
    if (data.config().usar_distribuicao_como_entrada) {
        for (const auto& a : data.grade().alocacoes()) {
            aloc_em_tela[a.aula_id] = &a;
        }
    }

    for (int i = 0; i < total_aulas; ++i) {
        const auto* aula = sorted_aulas[i];
        int aula_sem = aula->isAnual() ? 3 : ((aula->semestre() == model::SemestreLetivo::SEMESTRE_2 || aula->semestreAtivo() == 2) ? 2 : 1);
        if (callback && i % 5 == 0) {
            double prog = 0.40 + 0.50 * (static_cast<double>(i) / total_aulas);
            callback("Alocando aula " + std::to_string(i + 1) + " de " + std::to_string(total_aulas) + "...", prog);
        }

        const auto& cands = todos_candidatos[aula->id()];
        const CandidateAllocation* best_cand = nullptr;
        double min_penalty = 1e9;

        const model::AllocatedLesson* prior_aloc = nullptr;
        auto it_aloc = aloc_em_tela.find(aula->id());
        if (it_aloc != aloc_em_tela.end()) {
            prior_aloc = it_aloc->second;
        }

        for (const auto& c : cands) {
            int s = c.start_slot;
            int e = c.end_slot();
            double penalty = 0.0;

            if (prior_aloc && c.dia == prior_aloc->dia && c.start_slot == prior_aloc->intervalo.start_slot) {
                if (c.salas == prior_aloc->salas) {
                    penalty -= 10000.0; // Bônus máximo para manter a alocação da tela
                } else {
                    penalty -= 5000.0;
                }
            }

            bool conflict = false;
            for (const auto& p_id : aula->professores()) {
                if (overlaps(prof_ocup[{p_id, c.dia}], s, e, aula_sem)) {
                    conflict = true;
                    break;
                }
            }
            if (conflict) continue;

            for (const auto& t_id : aula->turmas()) {
                if (overlaps(turma_ocup[{t_id, c.dia}], s, e, aula_sem)) {
                    conflict = true;
                    break;
                }
                if (data.config().distribuir_disciplinas_na_semana) {
                    const auto* disc = data.findDisciplina(aula->disciplinaId());
                    int max_lim = (disc && disc->maxAulasPorDiaTurma() > 0) ? disc->maxAulasPorDiaTurma() : data.config().max_aulas_disciplina_dia_turma;
                    int cnt = turma_disc_dia_count[{t_id, aula->disciplinaId(), c.dia}];
                    if (cnt >= max_lim) {
                        conflict = true;
                        break;
                    }
                }
            }
            if (conflict) continue;

            const auto* disc_ptr = data.findDisciplina(aula->disciplinaId());
            if (disc_ptr && disc_ptr->exigirAulasConsecutivas()) {
                for (const auto& t_id : aula->turmas()) {
                    int cnt = turma_disc_dia_count[{t_id, aula->disciplinaId(), c.dia}];
                    if (cnt > 0) {
                        penalty -= 400.0; // Bônus forte para agrupar as aulas no mesmo dia
                    }
                }
            }

            // Início prioritário da primeira aula no início da jornada da turma (T_ini)
            for (const auto& t_id : aula->turmas()) {
                const auto* t = data.findTurma(t_id);
                if (t) {
                    auto j = t->jornadaParaDia(c.dia);
                    int t_ini = j.jornada_total.start_slot;
                    const auto& existing = turma_ocup[{t_id, c.dia}];
                    bool existing_in_sem = false;
                    for (const auto& r : existing) {
                        if (semestres_conflitam(r.semester, aula_sem)) {
                            existing_in_sem = true;
                            break;
                        }
                    }
                    if (!existing_in_sem) {
                        if (c.start_slot == t_ini) {
                            penalty -= 1000.0; // Forte incentivo para a primeira aula iniciar no início da jornada
                        } else {
                            penalty += 800.0; // Desestimula iniciar o dia fora do horário inicial
                        }
                    }
                }
            }

            for (const auto& sala_id : c.salas) {
                if (overlaps(sala_ocup[{sala_id, c.dia}], s, e, aula_sem)) {
                    conflict = true;
                    break;
                }
            }
            if (conflict) continue;

            for (const auto& p_id : aula->professores()) {
                const auto* p = data.findProfessor(p_id);
                auto c_shift = model::slotToShift(c.start_slot);
                if (p) {
                    penalty += (10 - p->pesoDia(c.dia)) * data.config().lambda_preferencia_dias;
                    if (!p->pretendeTurno(c.dia, c_shift)) {
                        penalty += 10.0 * data.config().lambda_preferencia_turnos;
                    }
                }
                const auto& existing = prof_ocup[{p_id, c.dia}];
                if (!existing.empty() && p && p->pesoJanela() > 0) {
                    std::vector<SlotRange> existing_shift;
                    for (const auto& r : existing) {
                        if (semestres_conflitam(r.semester, aula_sem) && model::slotToShift(r.start) == c_shift) {
                            existing_shift.push_back(r);
                        }
                    }
                    if (!existing_shift.empty()) {
                        int min_s = s, max_e = e;
                        for (const auto& r : existing_shift) {
                            min_s = std::min(min_s, r.start);
                            max_e = std::max(max_e, r.end);
                        }
                        int occupied_len = (e - s);
                        for (const auto& r : existing_shift) occupied_len += (r.end - r.start);
                        int gap_slots = (max_e - min_s) - occupied_len;
                        if (gap_slots > 0) {
                            penalty += gap_slots * data.config().lambda_janelas * (p->pesoJanela() / 5.0);
                        }
                    }
                }
            }

            if (aula->tipo() == "TEORICA" && !c.salas.empty()) {
                for (const auto& t_id : aula->turmas()) {
                    auto it_base = turma_base_sala.find(t_id);
                    if (it_base != turma_base_sala.end()) {
                        if (it_base->second == c.salas[0]) {
                            penalty -= 50.0;
                        } else {
                            penalty += 50.0;
                        }
                    }
                }
            }

            if (penalty < min_penalty) {
                min_penalty = penalty;
                best_cand = &c;
            }
        }

        if (best_cand) {
            int s = best_cand->start_slot;
            int e = best_cand->end_slot();

            std::string prof_label;
            for (size_t pi = 0; pi < aula->professores().size(); ++pi) {
                if (pi > 0) prof_label += ", ";
                const auto* p = data.findProfessor(aula->professores()[pi]);
                prof_label += (p && !p->nome().empty()) ? p->nome() : aula->professores()[pi];
            }
            std::string turma_label;
            for (size_t ti = 0; ti < aula->turmas().size(); ++ti) {
                if (ti > 0) turma_label += ", ";
                const auto* t = data.findTurma(aula->turmas()[ti]);
                turma_label += (t && !t->nome().empty()) ? t->nome() : aula->turmas()[ti];
            }
            std::string sala_label;
            for (size_t si = 0; si < best_cand->salas.size(); ++si) {
                if (si > 0) sala_label += ", ";
                const auto* r = data.findSala(best_cand->salas[si]);
                sala_label += (r && !r->nome().empty()) ? r->nome() : best_cand->salas[si];
            }

            SlotRange range{s, e, aula_sem, aula->id(), aula->disciplinaNome(), turma_label, prof_label, sala_label};

            for (const auto& p_id : aula->professores()) prof_ocup[{p_id, best_cand->dia}].push_back(range);
            for (const auto& t_id : aula->turmas()) {
                turma_ocup[{t_id, best_cand->dia}].push_back(range);
                turma_disc_dia_count[{t_id, aula->disciplinaId(), best_cand->dia}]++;
                turma_disc_dia_aulas[{t_id, aula->disciplinaId(), best_cand->dia}].push_back(range);
                if (aula->tipo() == "TEORICA" && !best_cand->salas.empty()) {
                    if (turma_base_sala.find(t_id) == turma_base_sala.end()) {
                        turma_base_sala[t_id] = best_cand->salas[0];
                    }
                }
            }
            for (const auto& sala_id : best_cand->salas) sala_ocup[{sala_id, best_cand->dia}].push_back(range);

            model::AllocatedLesson aloc;
            aloc.aula_id = aula->id();
            aloc.disciplina_id = aula->disciplinaId();
            aloc.disciplina_nome = aula->disciplinaNome();
            aloc.tipo = aula->tipo();
            aloc.dia = best_cand->dia;
            aloc.intervalo = best_cand->intervalo();
            aloc.professores = aula->professores();
            aloc.turmas = aula->turmas();
            aloc.salas = best_cand->salas;
            aloc.is_pinned = aula->isPinned();
            aloc.regime = aula->regime();
            aloc.semestre = aula->semestre();
            if (aula->isAnual()) {
                aloc.semestre_alocado = (data.config().escopo_semestre == model::EscopoSemestreOtimizacao::SEMESTRE_2) ? 2 : (data.config().escopo_semestre == model::EscopoSemestreOtimizacao::SEMESTRE_1 ? 1 : 3);
            } else {
                aloc.semestre_alocado = (aula->semestre() == model::SemestreLetivo::SEMESTRE_2 || aula->semestreAtivo() == 2) ? 2 : 1;
            }
            timetable.addAllocatedLesson(std::move(aloc));
            alocadas_count++;
        } else {
            std::string prof_nomes;
            for (size_t p_i = 0; p_i < aula->professores().size(); ++p_i) {
                if (p_i > 0) prof_nomes += ", ";
                const auto* p = data.findProfessor(aula->professores()[p_i]);
                prof_nomes += (p && !p->nome().empty()) ? p->nome() : aula->professores()[p_i];
            }
            if (prof_nomes.empty()) prof_nomes = "Não informado";

            std::string turma_nomes;
            for (size_t t_i = 0; t_i < aula->turmas().size(); ++t_i) {
                if (t_i > 0) turma_nomes += ", ";
                const auto* t = data.findTurma(aula->turmas()[t_i]);
                turma_nomes += (t && !t->nome().empty()) ? t->nome() : aula->turmas()[t_i];
            }
            if (turma_nomes.empty()) turma_nomes = "Não informada";

            std::vector<std::string> restricoes_quebradas;

            if (cands.empty()) {
                if (aula->professores().empty()) {
                    restricoes_quebradas.push_back("Nenhum professor cadastrado/atribuído à aula");
                }
                if (aula->turmas().empty()) {
                    restricoes_quebradas.push_back("Nenhuma turma cadastrada/atribuída à aula");
                }
                bool tem_sala = false;
                for (const auto& sala : data.salas()) {
                    if (aula->isTeorica() || sala.tipo() == aula->tipo() || !sala.isTeorica()) {
                        tem_sala = true;
                        break;
                    }
                }
                if (!tem_sala) {
                    restricoes_quebradas.push_back("Nenhuma sala cadastrada compatível com o tipo '" + aula->tipo() + "'");
                }
                if (aula->isPinned()) {
                    bool pin_conflito = false;
                    for (const auto& a_outra : data.grade().alocacoes()) {
                        if (a_outra.aula_id != aula->id() && a_outra.dia == aula->pinnedDay()) {
                            int s1 = aula->pinnedStartSlot();
                            int e1 = s1 + aula->duracaoSlots();
                            int s2 = a_outra.intervalo.start_slot;
                            int e2 = a_outra.intervalo.end_slot;
                            if (std::max(s1, s2) < std::min(e1, e2)) {
                                for (const auto& p1 : aula->professores()) {
                                    if (std::find(a_outra.professores.begin(), a_outra.professores.end(), p1) != a_outra.professores.end()) {
                                        const auto* p = data.findProfessor(p1);
                                        restricoes_quebradas.push_back("Aula fixada conflita com a aula '" + a_outra.disciplina_nome + "' (" + a_outra.aula_id + ") do Prof. " + (p ? p->nome() : p1) + " na " + model::dayToLongString(aula->pinnedDay()) + " às " + model::TimeInterval(s2, e2).toString());
                                        pin_conflito = true;
                                    }
                                }
                                for (const auto& t1 : aula->turmas()) {
                                    if (std::find(a_outra.turmas.begin(), a_outra.turmas.end(), t1) != a_outra.turmas.end()) {
                                        const auto* t = data.findTurma(t1);
                                        restricoes_quebradas.push_back("Aula fixada conflita com a aula '" + a_outra.disciplina_nome + "' (" + a_outra.aula_id + ") da Turma " + (t ? t->nome() : t1) + " na " + model::dayToLongString(aula->pinnedDay()) + " às " + model::TimeInterval(s2, e2).toString());
                                        pin_conflito = true;
                                    }
                                }
                            }
                        }
                    }
                    if (!pin_conflito) {
                        restricoes_quebradas.push_back("Aula fixada em horário/dia inválido ou fora da jornada da turma");
                    }
                }
                if (restricoes_quebradas.empty()) {
                    restricoes_quebradas.push_back("Sem candidatos viáveis no pré-processamento (incompatibilidade de jornada, turnos ou disponibilidade docente)");
                }
            } else {
                std::set<std::string> choques_prof;
                std::set<std::string> choques_turma;
                std::set<std::string> choques_limite;
                std::set<std::string> choques_sala;

                for (const auto& c : cands) {
                    int s = c.start_slot;
                    int e = c.end_slot();

                    for (const auto& p_id : aula->professores()) {
                        for (const auto& occ : prof_ocup[{p_id, c.dia}]) {
                            if (std::max(occ.start, s) < std::min(occ.end, e)) {
                                choques_prof.insert("A aula '" + aula->disciplinaNome() + "' (" + aula->id() + ") conflita com a aula '" + occ.disciplina_nome + "' (" + occ.aula_id +
                                    ") do Prof. " + (occ.prof_nome.empty() ? p_id : occ.prof_nome) + " na " + model::dayToLongString(c.dia) +
                                    " às " + model::TimeInterval(occ.start, occ.end).toString());
                            }
                        }
                    }

                    for (const auto& t_id : aula->turmas()) {
                        for (const auto& occ : turma_ocup[{t_id, c.dia}]) {
                            if (std::max(occ.start, s) < std::min(occ.end, e)) {
                                choques_turma.insert("A aula '" + aula->disciplinaNome() + "' (" + aula->id() + ") conflita com a aula '" + occ.disciplina_nome + "' (" + occ.aula_id +
                                    ") da Turma " + (occ.turma_nome.empty() ? t_id : occ.turma_nome) + " na " + model::dayToLongString(c.dia) +
                                    " às " + model::TimeInterval(occ.start, occ.end).toString());
                            }
                        }
                        if (data.config().distribuir_disciplinas_na_semana) {
                            const auto* disc = data.findDisciplina(aula->disciplinaId());
                            int max_lim = (disc && disc->maxAulasPorDiaTurma() > 0) ? disc->maxAulasPorDiaTurma() : data.config().max_aulas_disciplina_dia_turma;
                            int cnt = turma_disc_dia_count[{t_id, aula->disciplinaId(), c.dia}];
                            if (cnt >= max_lim) {
                                const auto* t = data.findTurma(t_id);
                                std::string t_name = (t && !t->nome().empty() ? t->nome() : t_id);
                                std::string aulas_alocadas_str;
                                for (const auto& occ : turma_disc_dia_aulas[{t_id, aula->disciplinaId(), c.dia}]) {
                                    if (!aulas_alocadas_str.empty()) aulas_alocadas_str += ", ";
                                    aulas_alocadas_str += "'" + occ.disciplina_nome + "' (" + occ.aula_id + " às " + model::TimeInterval(occ.start, occ.end).toString() + ")";
                                }
                                choques_limite.insert("A aula '" + aula->disciplinaNome() + "' (" + aula->id() + ") excede o limite diário (" + std::to_string(max_lim) + " aulas/dia) na Turma " + t_name +
                                    " na " + model::dayToLongString(c.dia) + " (aulas já alocadas hoje: " + aulas_alocadas_str + ")");
                            }
                        }
                    }

                    for (const auto& sala_id : c.salas) {
                        for (const auto& occ : sala_ocup[{sala_id, c.dia}]) {
                            if (std::max(occ.start, s) < std::min(occ.end, e)) {
                                choques_sala.insert("A aula '" + aula->disciplinaNome() + "' (" + aula->id() + ") conflita com a aula '" + occ.disciplina_nome + "' (" + occ.aula_id +
                                    ") na Sala " + (occ.sala_nome.empty() ? sala_id : occ.sala_nome) + " na " + model::dayToLongString(c.dia) +
                                    " às " + model::TimeInterval(occ.start, occ.end).toString());
                            }
                        }
                    }
                }

                for (const auto& item : choques_prof) {
                    restricoes_quebradas.push_back("Choque de horário de professor: " + item);
                }
                for (const auto& item : choques_turma) {
                    restricoes_quebradas.push_back("Choque de horário de turma: " + item);
                }
                for (const auto& item : choques_limite) {
                    restricoes_quebradas.push_back("Limite diário de disciplina: " + item);
                }
                for (const auto& item : choques_sala) {
                    restricoes_quebradas.push_back("Choque de sala física: " + item);
                }

                if (restricoes_quebradas.empty()) {
                    restricoes_quebradas.push_back("Conflitos de horário em todos os candidatos admissíveis");
                }
            }

            std::string msg = "❌ Não foi possível alocar a aula: '" + aula->disciplinaNome() + "' (" + aula->id() + ")"
                            + " | Professor: " + prof_nomes
                            + " | Turma: " + turma_nomes;
            if (!restricoes_quebradas.empty()) {
                msg += "\n   ↳ Conflita com as seguintes aulas / restrições:";
                for (const auto& r : restricoes_quebradas) {
                    msg += "\n     • " + r;
                }
            }
            timetable.addMensagemDiagnostico(msg);
        }
    }

    for (const auto& [t_id, sala_id] : turma_base_sala) {
        timetable.setSalaTeoricaDesignada(t_id, sala_id);
    }

    auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start_time).count();
    model::TimetableStatistics stats;
    stats.tempo_execucao_ms = elapsed;
    stats.total_aulas_esperadas = total_aulas;
    stats.total_aulas_alocadas = alocadas_count;

    int total_D = 0;
    int total_W = 0;
    for (const auto& prof : data.professores()) {
        for (auto dia : model::allDays()) {
            if (!prof_ocup[{prof.id(), dia}].empty()) {
                total_D++;
                total_W += prof.penalidadeDia(dia);
            }
        }
    }
    stats.penalidade_dias_D = total_D;
    stats.penalidade_preferencia_dias_W = total_W;

    if (alocadas_count == total_aulas) {
        stats.status = "FEASIBLE";
        timetable.addMensagemDiagnostico("[HEURÍSTICA] Todas as " + std::to_string(total_aulas) + " aulas foram alocadas com sucesso em " + std::to_string(static_cast<int>(elapsed)) + " ms.");
        if (callback) callback("Grade gerada via Heurística Construtiva Gulosa!", 1.0);
    } else {
        stats.status = (alocadas_count > 0 ? "FEASIBLE" : "INFEASIBLE");
        timetable.addMensagemDiagnostico("[HEURÍSTICA] Alocadas " + std::to_string(alocadas_count) + " de " + std::to_string(total_aulas) + " aulas.");
        if (callback) callback("Otimização parcial: algumas restrições impediram alocação de 100% das aulas.", 1.0);
    }

    if (data.config().usar_distribuicao_como_entrada && !data.grade().alocacoes().empty()) {
        timetable.addMensagemDiagnostico("[WARM-START] Distribuição de disciplinas atual da tela utilizada como base de prioridade.");
    }

    auditarConflitosEntreAulas(data, timetable);
    timetable.setEstatisticas(stats);
    return timetable;
}

namespace {

constexpr double kMilpInf = 1e20;

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

struct MilpProblem {
    std::map<std::string, std::vector<CandidateAllocation>> todos_candidatos;
    std::vector<MilpVar> vars;
    std::vector<MilpRow> rows;

    std::vector<MilpCandItem> todas_vars;
    std::map<std::string, std::vector<MilpCandItem>> vars_por_aula;
    std::map<std::pair<std::string, std::string>, int> z_turma_sala;
    std::map<std::pair<std::string, model::DayOfWeek>, int> y_prof_dia;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<MilpCandItem>> vars_prof_dia;

    std::vector<std::pair<int, int>> j_terms;   // (g_col, pesoJanela)
    std::vector<int> jg_cols;                   // q_col
    std::vector<std::pair<int, int>> p_terms;   // (x_col, custo_preferencia)

    std::vector<GapAuxRecord> gap_records;
    std::vector<ShiftAuxRecord> shift_records;
    std::vector<ConsecAuxRecord> consec_records;

    std::vector<double> warm_start_vals;
    bool has_warm_start{false};
    int hints_aplicados{0};

    int addBinVar(const std::string& name, double obj = 0.0, double lb = 0.0, double ub = 1.0) {
        int idx = static_cast<int>(vars.size());
        vars.push_back(MilpVar{name, lb, ub, obj});
        return idx;
    }

    void addRow(const std::string& name,
                std::vector<std::pair<int, double>> terms,
                double lhs,
                double rhs) {
        if (terms.empty()) return;
        std::sort(terms.begin(), terms.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        MilpRow row;
        row.name = name;
        row.lhs = lhs;
        row.rhs = rhs;
        for (const auto& [col, val] : terms) {
            if (!row.cols.empty() && row.cols.back() == col) {
                row.vals.back() += val;
            } else {
                row.cols.push_back(col);
                row.vals.push_back(val);
            }
        }
        std::vector<int> clean_cols;
        std::vector<double> clean_vals;
        clean_cols.reserve(row.cols.size());
        clean_vals.reserve(row.vals.size());
        for (size_t i = 0; i < row.cols.size(); ++i) {
            if (std::abs(row.vals[i]) > 1e-12) {
                clean_cols.push_back(row.cols[i]);
                clean_vals.push_back(row.vals[i]);
            }
        }
        if (clean_cols.empty()) return;
        row.cols = std::move(clean_cols);
        row.vals = std::move(clean_vals);
        rows.push_back(std::move(row));
    }
};

/**
 * @brief Constrói a formulação matemática MILP completa compartilhada pelos motores SCIP e HiGHS.
 */
static bool buildMilpProblem(const model::InstitutionData& data,
                             MilpProblem& milp,
                             model::Timetable& timetable,
                             ProgressCallback callback,
                             const std::string& solver_tag,
                             const model::Timetable* heuristic_seed = nullptr) {
    if (callback) callback(solver_tag + ": Validando dados acadêmicos...", 0.05);

    auto val_res = Preprocessor::validarDados(data);
    if (!val_res.is_valid) {
        model::TimetableStatistics stats;
        stats.status = "DADOS_INVALIDOS";
        timetable.setEstatisticas(stats);
        for (const auto& err : val_res.errors) timetable.addMensagemDiagnostico("[ERRO] " + err);
        for (const auto& w : val_res.warnings) timetable.addMensagemDiagnostico("[AVISO] " + w);
        return false;
    }

    if (callback) callback(solver_tag + ": Gerando candidatos de alocação (C_a)...", 0.15);

    milp.todos_candidatos = Preprocessor::gerarTodosCandidatos(data, val_res);
    if (!val_res.is_valid) {
        model::TimetableStatistics stats;
        stats.status = "INFEASIBLE";
        timetable.setEstatisticas(stats);
        for (const auto& err : val_res.errors) timetable.addMensagemDiagnostico("[INVIABILIDADE] " + err);
        return false;
    }

    if (callback) callback(solver_tag + ": Construindo matriz e restrições lineares inteiras...", 0.25);

    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<MilpCandItem>> vars_turma_dia;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<MilpCandItem>> vars_sala_dia;

    bool has_semestrais = false;
    for (const auto& a : data.aulas()) {
        if (a.isSemestral() || a.semestre() != model::SemestreLetivo::AMBOS) {
            has_semestrais = true;
            break;
        }
    }
    bool is_multi_sem = (data.config().escopo_semestre == model::EscopoSemestreOtimizacao::AMBOS_INTEGRADO ||
                        (data.config().escopo_semestre == model::EscopoSemestreOtimizacao::TODAS_SEM_FILTRO && has_semestrais));

    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<MilpCandItem>> vars_prof_dia_s1;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<MilpCandItem>> vars_prof_dia_s2;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<MilpCandItem>> vars_turma_dia_s1;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<MilpCandItem>> vars_turma_dia_s2;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<MilpCandItem>> vars_sala_dia_s1;
    std::map<std::pair<std::string, model::DayOfWeek>, std::vector<MilpCandItem>> vars_sala_dia_s2;

    // 1. Variáveis de decisão x_{a,d,t,b}
    int var_idx = 0;
    for (const auto& aula : data.aulas()) {
        const auto& cands = milp.todos_candidatos[aula.id()];
        bool in_s1 = true;
        bool in_s2 = false;
        if (is_multi_sem) {
            if (aula.isAnual() || aula.semestre() == model::SemestreLetivo::AMBOS) {
                in_s1 = true;
                in_s2 = true;
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
            double obj_coeff = 0.0;
            if (c.custo_preferencia > 0) {
                obj_coeff += static_cast<double>(data.config().lambda_preferencias * c.custo_preferencia);
            }
            if (data.config().modelo_preferencia_dias == model::ModeloPreferenciaDias::POR_MINUTO_AULA) {
                for (const auto& p_id : aula.professores()) {
                    const auto* prof = data.findProfessor(p_id);
                    if (prof) {
                        obj_coeff += static_cast<double>(data.config().lambda_preferencia_dias *
                                                         prof->penalidadeDia(c.dia) * c.duration_minutes);
                    }
                }
            }

            int col = milp.addBinVar(var_name, obj_coeff);
            MilpCandItem item{&c, col};
            milp.todas_vars.push_back(item);
            milp.vars_por_aula[aula.id()].push_back(item);

            if (c.custo_preferencia > 0) {
                milp.p_terms.push_back({col, c.custo_preferencia});
            }

            for (const auto& p_id : aula.professores()) {
                milp.vars_prof_dia[{p_id, c.dia}].push_back(item);
                if (in_s1) vars_prof_dia_s1[{p_id, c.dia}].push_back(item);
                if (in_s2) vars_prof_dia_s2[{p_id, c.dia}].push_back(item);
            }
            for (const auto& t_id : aula.turmas()) {
                vars_turma_dia[{t_id, c.dia}].push_back(item);
                if (in_s1) vars_turma_dia_s1[{t_id, c.dia}].push_back(item);
                if (in_s2) vars_turma_dia_s2[{t_id, c.dia}].push_back(item);
            }
            for (const auto& s_id : c.salas) {
                vars_sala_dia[{s_id, c.dia}].push_back(item);
                if (in_s1) vars_sala_dia_s1[{s_id, c.dia}].push_back(item);
                if (in_s2) vars_sala_dia_s2[{s_id, c.dia}].push_back(item);
            }
        }
    }

    // 2. Variáveis de sala teórica fixa por turma: z_{g,r}
    int row_id = 0;
    for (const auto& turma : data.turmas()) {
        if (turma.temSalaTeoricaPredefinida()) {
            std::string r = turma.salaTeoricaId();
            int z = milp.addBinVar("z_" + turma.id() + "_" + r, 0.0, 1.0, 1.0);
            milp.z_turma_sala[{turma.id(), r}] = z;
        } else if (!turma.salasTeoricasPermitidas().empty()) {
            std::vector<std::pair<int, double>> terms;
            for (const auto& r : turma.salasTeoricasPermitidas()) {
                int z = milp.addBinVar("z_" + turma.id() + "_" + r, 0.0);
                milp.z_turma_sala[{turma.id(), r}] = z;
                terms.push_back({z, 1.0});
            }
            milp.addRow("z_fixed_" + turma.id(), std::move(terms), 1.0, 1.0);
        }
    }

    // Vinculação sum_{c in C_a : sala(c)={r}} x_{a,c} - z_{g,r} <= 0 (agrupado por aula e sala)
    for (const auto& aula : data.aulas()) {
        if (!aula.isTeorica() || aula.turmas().size() != 1) continue;
        std::string g_id = aula.turmas().front();
        std::map<std::string, std::vector<int>> cols_por_sala;
        for (const auto& item : milp.vars_por_aula[aula.id()]) {
            if (item.cand->salas.size() == 1) {
                cols_por_sala[item.cand->salas.front()].push_back(item.col);
            }
        }
        for (const auto& [r_id, cols] : cols_por_sala) {
            auto it_z = milp.z_turma_sala.find({g_id, r_id});
            if (it_z != milp.z_turma_sala.end()) {
                std::vector<std::pair<int, double>> terms;
                terms.reserve(cols.size() + 1);
                for (int c_col : cols) terms.push_back({c_col, 1.0});
                terms.push_back({it_z->second, -1.0});
                milp.addRow("link_z_" + std::to_string(row_id++), std::move(terms), -kMilpInf, 0.0);
            }
        }
    }

    // 3. Restrição 5.1: Cada aula deve ser alocada exatamente uma vez
    for (const auto& aula : data.aulas()) {
        const auto& items = milp.vars_por_aula[aula.id()];
        std::vector<std::pair<int, double>> terms;
        terms.reserve(items.size());
        for (const auto& item : items) {
            terms.push_back({item.col, 1.0});
        }
        milp.addRow("alloc_" + aula.id(), std::move(terms), 1.0, 1.0);
    }

    // 4. Restrições 5.2, 5.3, 5.4: Não sobreposição linearizada via cliques por slot (sum_{start <= k < end} x <= 1)
    auto add_no_overlap_cliques = [&](const std::map<std::pair<std::string, model::DayOfWeek>, std::vector<MilpCandItem>>& map_vars,
                                      const std::string& prefix) {
        for (const auto& [key, items] : map_vars) {
            if (items.size() <= 1) continue;
            int min_s = 288, max_e = 0;
            for (const auto& item : items) {
                min_s = std::min(min_s, item.cand->start_slot);
                max_e = std::max(max_e, item.cand->end_slot());
            }
            std::set<std::vector<int>> emitted_cliques;
            for (int k = min_s; k < max_e; ++k) {
                std::vector<int> active_cols;
                for (const auto& item : items) {
                    if (item.cand->start_slot <= k && k < item.cand->end_slot()) {
                        active_cols.push_back(item.col);
                    }
                }
                if (active_cols.size() > 1) {
                    std::sort(active_cols.begin(), active_cols.end());
                    active_cols.erase(std::unique(active_cols.begin(), active_cols.end()), active_cols.end());
                    if (active_cols.size() > 1 && emitted_cliques.insert(active_cols).second) {
                        std::vector<std::pair<int, double>> terms;
                        terms.reserve(active_cols.size());
                        for (int c_idx : active_cols) terms.push_back({c_idx, 1.0});
                        milp.addRow(prefix + "_" + key.first + "_" + std::to_string(static_cast<int>(key.second)) + "_" + std::to_string(k),
                                    std::move(terms), -kMilpInf, 1.0);
                    }
                }
            }
        }
    };

    if (is_multi_sem) {
        add_no_overlap_cliques(vars_prof_dia_s1, "noov_p_s1");
        add_no_overlap_cliques(vars_prof_dia_s2, "noov_p_s2");
        add_no_overlap_cliques(vars_sala_dia_s1, "noov_r_s1");
        add_no_overlap_cliques(vars_sala_dia_s2, "noov_r_s2");
        add_no_overlap_cliques(vars_turma_dia_s1, "noov_t_s1");
        add_no_overlap_cliques(vars_turma_dia_s2, "noov_t_s2");
    } else {
        add_no_overlap_cliques(milp.vars_prof_dia, "noov_p");
        add_no_overlap_cliques(vars_sala_dia, "noov_r");
        add_no_overlap_cliques(vars_turma_dia, "noov_t");
    }

    // 5.4b Início obrigatório da primeira aula no início da jornada da turma (T_ini)
    auto add_turma_inicio_obrigatorio = [&](const std::map<std::pair<std::string, model::DayOfWeek>, std::vector<MilpCandItem>>& map_vars) {
        for (const auto& turma : data.turmas()) {
            for (auto dia : turma.diasLetivos()) {
                auto it = map_vars.find({turma.id(), dia});
                if (it == map_vars.end() || it->second.empty()) continue;

                auto jornada = turma.jornadaParaDia(dia);
                int t_ini = jornada.jornada_total.start_slot;

                std::vector<int> start_cols;
                std::map<std::string, std::vector<int>> later_cols_by_aula;
                for (const auto& item : it->second) {
                    if (item.cand->start_slot == t_ini) {
                        start_cols.push_back(item.col);
                    } else if (item.cand->start_slot > t_ini) {
                        later_cols_by_aula[item.cand->aula_id].push_back(item.col);
                    }
                }

                for (const auto& [aula_id, cols_later] : later_cols_by_aula) {
                    (void)aula_id;
                    std::vector<std::pair<int, double>> terms;
                    terms.reserve(cols_later.size() + start_cols.size());
                    for (int col_l : cols_later) {
                        terms.push_back({col_l, 1.0});
                    }
                    for (int col_s : start_cols) {
                        terms.push_back({col_s, -1.0});
                    }
                    milp.addRow("t_ini_" + std::to_string(row_id++), std::move(terms), -kMilpInf, 0.0);
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

    // 5.5 Carga máxima diária do professor
    auto add_prof_carga_max = [&](const std::map<std::pair<std::string, model::DayOfWeek>, std::vector<MilpCandItem>>& map_vars) {
        for (const auto& prof : data.professores()) {
            if (prof.cargaMaximaDiariaMinutos() > 0) {
                for (auto dia : model::allDays()) {
                    auto it = map_vars.find({prof.id(), dia});
                    if (it == map_vars.end() || it->second.empty()) continue;
                    std::vector<std::pair<int, double>> terms;
                    terms.reserve(it->second.size());
                    for (const auto& item : it->second) {
                        terms.push_back({item.col, static_cast<double>(item.cand->duration_minutes)});
                    }
                    milp.addRow("p_maxmin_" + std::to_string(row_id++),
                                std::move(terms), -kMilpInf, static_cast<double>(prof.cargaMaximaDiariaMinutos()));
                }
            }
        }
    };

    if (is_multi_sem) {
        add_prof_carga_max(vars_prof_dia_s1);
        add_prof_carga_max(vars_prof_dia_s2);
    } else {
        add_prof_carga_max(milp.vars_prof_dia);
    }

    // 5.6 Preenchimento integral opcional da jornada
    if (data.config().preenchimento_integral_jornada) {
        auto add_preenchimento_integral = [&](const std::map<std::pair<std::string, model::DayOfWeek>, std::vector<MilpCandItem>>& map_vars) {
            for (const auto& turma : data.turmas()) {
                for (auto dia : turma.diasLetivos()) {
                    auto jornada = turma.jornadaParaDia(dia);
                    auto blocos = jornada.blocosLetivos();
                    auto it = map_vars.find({turma.id(), dia});
                    if (it == map_vars.end()) continue;
                    const auto& items = it->second;

                    for (const auto& bloco : blocos) {
                        for (int k = bloco.start_slot; k < bloco.end_slot; ++k) {
                            std::vector<std::pair<int, double>> terms;
                            for (const auto& item : items) {
                                if (item.cand->start_slot <= k && k < item.cand->end_slot()) {
                                    terms.push_back({item.col, 1.0});
                                }
                            }
                            milp.addRow("full_" + std::to_string(row_id++), std::move(terms), 1.0, 1.0);
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

    // 5.7 Distribuição semanal de disciplinas
    if (data.config().distribuir_disciplinas_na_semana) {
        for (const auto& disc : data.disciplinas()) {
            int max_por_dia = disc.maxAulasPorDiaTurma() > 0 ? disc.maxAulasPorDiaTurma() : data.config().max_aulas_disciplina_dia_turma;
            if (max_por_dia > 0) {
                for (const auto& t_id : disc.turmas()) {
                    (void)t_id;
                    for (auto dia : model::allDays()) {
                        std::vector<std::pair<int, double>> terms;
                        for (int i = 1; i <= disc.aulasSemanais(); ++i) {
                            std::string aula_id = disc.id() + "_" + std::to_string(i);
                            const auto& items = milp.vars_por_aula[aula_id];
                            for (const auto& item : items) {
                                if (item.cand->dia == dia) {
                                    terms.push_back({item.col, 1.0});
                                }
                            }
                        }
                        if (!terms.empty()) {
                            milp.addRow("dist_" + std::to_string(row_id++),
                                        std::move(terms), -kMilpInf, static_cast<double>(max_por_dia));
                        }
                    }
                }
            }
        }
    }

    // 5.8 Disciplinas com aulas consecutivas em bloco
    for (const auto& disc : data.disciplinas()) {
        if (!disc.exigirAulasConsecutivas()) continue;
        int n_aulas = disc.aulasSemanais();
        if (n_aulas != 2 && n_aulas != 4 && n_aulas != 8) continue;

        int block_size = (n_aulas == 2) ? 2 : 4;
        int num_days = (n_aulas == 8) ? 2 : 1;

        std::vector<std::pair<int, double>> sum_dias_terms;
        for (auto dia : model::allDays()) {
            int w_d = milp.addBinVar("w_d_" + disc.id() + "_" + std::to_string(static_cast<int>(dia)), 0.0);
            sum_dias_terms.push_back({w_d, 1.0});

            std::vector<std::pair<int, double>> terms;
            std::vector<int> day_cols;
            for (int i = 1; i <= n_aulas; ++i) {
                std::string aula_id = disc.id() + "_" + std::to_string(i);
                const auto& items = milp.vars_por_aula[aula_id];
                for (const auto& item : items) {
                    if (item.cand->dia == dia) {
                        terms.push_back({item.col, 1.0});
                        day_cols.push_back(item.col);
                    }
                }
            }
            terms.push_back({w_d, -static_cast<double>(block_size)});
            milp.addRow("consec_" + std::to_string(row_id++), std::move(terms), 0.0, 0.0);
            milp.consec_records.push_back(ConsecAuxRecord{std::move(day_cols), w_d});
        }
        milp.addRow("consec_days_" + disc.id(), std::move(sum_dias_terms),
                    static_cast<double>(num_days), static_cast<double>(num_days));
    }

    if (callback) callback(solver_tag + ": Configurando função objetivo e preferências...", 0.40);

    // 6.1 Variáveis y_{p,d}: indica se professor p trabalha no dia d
    for (const auto& prof : data.professores()) {
        for (auto dia : model::allDays()) {
            const auto& items = milp.vars_prof_dia[{prof.id(), dia}];
            std::string y_name = "y_" + prof.id() + "_" + std::to_string(static_cast<int>(dia));
            if (items.empty()) {
                int y = milp.addBinVar(y_name, 0.0, 0.0, 0.0);
                milp.y_prof_dia[{prof.id(), dia}] = y;
            } else {
                double y_obj = static_cast<double>(data.config().lambda_dias);
                if (data.config().modelo_preferencia_dias == model::ModeloPreferenciaDias::POR_DIA_TRABALHADO) {
                    y_obj += static_cast<double>(data.config().lambda_preferencia_dias * prof.penalidadeDia(dia));
                }
                int y = milp.addBinVar(y_name, y_obj);
                milp.y_prof_dia[{prof.id(), dia}] = y;

                std::vector<std::pair<int, double>> sum_terms;
                sum_terms.reserve(items.size() + 1);
                sum_terms.push_back({y, 1.0});
                std::map<std::string, std::vector<int>> cols_by_aula;
                for (const auto& item : items) {
                    cols_by_aula[item.cand->aula_id].push_back(item.col);
                    sum_terms.push_back({item.col, -1.0});
                }
                for (const auto& [aula_id, a_cols] : cols_by_aula) {
                    (void)aula_id;
                    std::vector<std::pair<int, double>> lb_terms;
                    lb_terms.reserve(a_cols.size() + 1);
                    for (int c_col : a_cols) lb_terms.push_back({c_col, 1.0});
                    lb_terms.push_back({y, -1.0});
                    milp.addRow("y_lb_" + std::to_string(row_id++), std::move(lb_terms), -kMilpInf, 0.0);
                }
                // y <= sum(x)  <=>  y - sum(x) <= 0
                milp.addRow("y_ub_" + std::to_string(row_id++), std::move(sum_terms), -kMilpInf, 0.0);
            }
        }
    }

    // 6.3 Termo J (Janelas de professores) e Termo T (Turnos não pretendidos)
    int gap_var_idx = 0;
    int shift_var_idx = 0;

    for (const auto& prof : data.professores()) {
        for (auto dia : model::allDays()) {
            const auto& items_dia = milp.vars_prof_dia[{prof.id(), dia}];
            if (items_dia.empty()) continue;

            for (auto shift : model::allWorkShifts()) {
                std::vector<MilpCandItem> items_shift;
                for (const auto& item : items_dia) {
                    if (model::slotToShift(item.cand->start_slot) == shift) {
                        items_shift.push_back(item);
                    }
                }
                if (items_shift.empty()) continue;

                // Termo T
                if (!prof.pretendeTurno(dia, shift)) {
                    double t_obj = static_cast<double>(data.config().lambda_preferencia_turnos * 10);
                    int y_shift = milp.addBinVar(
                        "y_shift_" + prof.id() + "_" + std::to_string(static_cast<int>(dia)) + "_" +
                        std::to_string(static_cast<int>(shift)) + "_" + std::to_string(shift_var_idx++),
                        t_obj);

                    std::vector<std::pair<int, double>> sum_shift;
                    std::vector<int> shift_cols;
                    sum_shift.push_back({y_shift, 1.0});
                    for (const auto& item : items_shift) {
                        milp.addRow("y_sh_lb_" + std::to_string(row_id++),
                                    {{item.col, 1.0}, {y_shift, -1.0}}, -kMilpInf, 0.0);
                        sum_shift.push_back({item.col, -1.0});
                        shift_cols.push_back(item.col);
                    }
                    milp.addRow("y_sh_ub_" + std::to_string(row_id++), std::move(sum_shift), -kMilpInf, 0.0);
                    milp.shift_records.push_back(ShiftAuxRecord{std::move(shift_cols), y_shift});
                }

                // Termo J (encadeamento linear O(|I| + |K|) de u_k e v_k)
                if (prof.pesoJanela() <= 0 || items_shift.size() <= 1 || data.config().lambda_janelas <= 0) continue;

                int min_s = 288, max_e = 0;
                int min_end = 288, max_start = 0;
                for (const auto& item : items_shift) {
                    min_s = std::min(min_s, item.cand->start_slot);
                    max_e = std::max(max_e, item.cand->end_slot());
                    min_end = std::min(min_end, item.cand->end_slot());
                    max_start = std::max(max_start, item.cand->start_slot);
                }

                if (min_end >= max_start) continue;

                int span = max_e - min_s;
                std::vector<int> u_cols(span, -1);
                std::vector<int> v_cols(span, -1);
                for (int k = min_s; k < max_e; ++k) {
                    u_cols[k - min_s] = milp.addBinVar("u_" + std::to_string(gap_var_idx) + "_" + std::to_string(k), 0.0);
                    v_cols[k - min_s] = milp.addBinVar("v_" + std::to_string(gap_var_idx) + "_" + std::to_string(k), 0.0);
                    if (k > min_s) {
                        // u_{k-1} <= u_k
                        milp.addRow("u_chain_" + std::to_string(row_id++),
                                    {{u_cols[k - 1 - min_s], 1.0}, {u_cols[k - min_s], -1.0}}, -kMilpInf, 0.0);
                    }
                }
                for (int k = min_s; k + 1 < max_e; ++k) {
                    // v_{k+1} <= v_k
                    milp.addRow("v_chain_" + std::to_string(row_id++),
                                {{v_cols[k + 1 - min_s], 1.0}, {v_cols[k - min_s], -1.0}}, -kMilpInf, 0.0);
                }
                gap_var_idx++;

                for (const auto& item : items_shift) {
                    int e_slot = item.cand->end_slot();
                    if (e_slot >= min_s && e_slot < max_e) {
                        milp.addRow("u_lb_" + std::to_string(row_id++),
                                    {{item.col, 1.0}, {u_cols[e_slot - min_s], -1.0}}, -kMilpInf, 0.0);
                    }
                    int s_slot = item.cand->start_slot;
                    if (s_slot > min_s && s_slot - 1 < max_e) {
                        milp.addRow("v_lb_" + std::to_string(row_id++),
                                    {{item.col, 1.0}, {v_cols[s_slot - 1 - min_s], -1.0}}, -kMilpInf, 0.0);
                    }
                }

                for (int k = min_s; k < max_e; ++k) {
                    std::vector<int> cands_before;
                    std::vector<int> cands_after;
                    std::vector<int> cands_cover;

                    for (const auto& item : items_shift) {
                        if (item.cand->start_slot <= k && k < item.cand->end_slot()) {
                            cands_cover.push_back(item.col);
                        }
                        if (item.cand->end_slot() <= k) {
                            cands_before.push_back(item.col);
                        }
                        if (item.cand->start_slot > k) {
                            cands_after.push_back(item.col);
                        }
                    }

                    int u = u_cols[k - min_s];
                    int v = v_cols[k - min_s];
                    int g = -1;

                    if (!cands_before.empty() && !cands_after.empty()) {
                        double g_obj = static_cast<double>(data.config().lambda_janelas * prof.pesoJanela());
                        g = milp.addBinVar("g_" + std::to_string(gap_var_idx) + "_" + std::to_string(k), g_obj);

                        // g >= u + v - 1 - w_k  <=>  g - u - v + w_k >= -1
                        std::vector<std::pair<int, double>> g_terms;
                        g_terms.push_back({g, 1.0});
                        g_terms.push_back({u, -1.0});
                        g_terms.push_back({v, -1.0});
                        for (int cov : cands_cover) {
                            g_terms.push_back({cov, 1.0});
                        }
                        milp.addRow("gap_p_" + std::to_string(row_id++), std::move(g_terms), -1.0, kMilpInf);

                        milp.j_terms.push_back({g, prof.pesoJanela()});
                    }
                    milp.gap_records.push_back(GapAuxRecord{
                        std::move(cands_before), std::move(cands_after), std::move(cands_cover), u, v, g});
                }
            }
        }
    }

    // 6.4 Termo JG: Minimizar lacunas internas das turmas
    if (!data.config().preenchimento_integral_jornada && data.config().lambda_lacunas_turma > 0) {
        int q_idx = 0;
        for (const auto& turma : data.turmas()) {
            for (auto dia : turma.diasLetivos()) {
                const auto& items = vars_turma_dia[{turma.id(), dia}];
                if (items.size() <= 1) continue;

                auto jornada = turma.jornadaParaDia(dia);
                auto blocos = jornada.blocosLetivos();

                for (const auto& bloco : blocos) {
                    int b_start = bloco.start_slot;
                    int b_end = bloco.end_slot;
                    if (b_end <= b_start) continue;

                    int span = b_end - b_start;
                    std::vector<int> u_cols(span, -1);
                    std::vector<int> v_cols(span, -1);
                    for (int k = b_start; k < b_end; ++k) {
                        u_cols[k - b_start] = milp.addBinVar("uq_" + std::to_string(q_idx) + "_" + std::to_string(k), 0.0);
                        v_cols[k - b_start] = milp.addBinVar("vq_" + std::to_string(q_idx) + "_" + std::to_string(k), 0.0);
                        if (k > b_start) {
                            milp.addRow("uq_chain_" + std::to_string(row_id++),
                                        {{u_cols[k - 1 - b_start], 1.0}, {u_cols[k - b_start], -1.0}}, -kMilpInf, 0.0);
                        }
                    }
                    for (int k = b_start; k + 1 < b_end; ++k) {
                        milp.addRow("vq_chain_" + std::to_string(row_id++),
                                    {{v_cols[k + 1 - b_start], 1.0}, {v_cols[k - b_start], -1.0}}, -kMilpInf, 0.0);
                    }
                    q_idx++;

                    for (const auto& item : items) {
                        int e_slot = item.cand->end_slot();
                        if (e_slot >= b_start && e_slot < b_end) {
                            milp.addRow("uq_lb_" + std::to_string(row_id++),
                                        {{item.col, 1.0}, {u_cols[e_slot - b_start], -1.0}}, -kMilpInf, 0.0);
                        } else if (e_slot <= b_start) {
                            milp.addRow("uq_lb0_" + std::to_string(row_id++),
                                        {{item.col, 1.0}, {u_cols[0], -1.0}}, -kMilpInf, 0.0);
                        }
                        int s_slot = item.cand->start_slot;
                        if (s_slot > b_start && s_slot - 1 < b_end) {
                            milp.addRow("vq_lb_" + std::to_string(row_id++),
                                        {{item.col, 1.0}, {v_cols[s_slot - 1 - b_start], -1.0}}, -kMilpInf, 0.0);
                        } else if (s_slot >= b_end) {
                            milp.addRow("vq_lbn_" + std::to_string(row_id++),
                                        {{item.col, 1.0}, {v_cols[span - 1], -1.0}}, -kMilpInf, 0.0);
                        }
                    }

                    for (int k = b_start; k < b_end; ++k) {
                        std::vector<int> cands_before;
                        std::vector<int> cands_after;
                        std::vector<int> cands_cover;

                        for (const auto& item : items) {
                            if (item.cand->start_slot <= k && k < item.cand->end_slot()) {
                                cands_cover.push_back(item.col);
                            }
                            if (item.cand->end_slot() <= k) {
                                cands_before.push_back(item.col);
                            }
                            if (item.cand->start_slot > k) {
                                cands_after.push_back(item.col);
                            }
                        }

                        int u = u_cols[k - b_start];
                        int v = v_cols[k - b_start];
                        int q = -1;

                        if (!cands_before.empty() && !cands_after.empty()) {
                            double q_obj = static_cast<double>(data.config().lambda_lacunas_turma * 5);
                            q = milp.addBinVar("q_" + std::to_string(q_idx) + "_" + std::to_string(k), q_obj);

                            std::vector<std::pair<int, double>> q_terms;
                            q_terms.push_back({q, 1.0});
                            q_terms.push_back({u, -1.0});
                            q_terms.push_back({v, -1.0});
                            for (int cov : cands_cover) {
                                q_terms.push_back({cov, 1.0});
                            }
                            milp.addRow("gap_t_" + std::to_string(row_id++), std::move(q_terms), -1.0, kMilpInf);

                            milp.jg_cols.push_back(q);
                        }
                        milp.gap_records.push_back(GapAuxRecord{
                            std::move(cands_before), std::move(cands_after), std::move(cands_cover), u, v, q});
                    }
                }
            }
        }
    }

    // 6.5 Solução Inicial (Warm Start da tela ou semente primal inicial)
    const model::Timetable* active_seed = nullptr;
    bool from_screen = false;
    if (data.config().usar_distribuicao_como_entrada && !data.grade().alocacoes().empty()) {
        active_seed = &data.grade();
        from_screen = true;
    } else if (heuristic_seed && !heuristic_seed->alocacoes().empty()) {
        active_seed = heuristic_seed;
        from_screen = false;
    }

    if (active_seed) {
        if (callback && from_screen) callback(solver_tag + ": Preparando solução inicial (Warm Start)...", 0.50);
        milp.warm_start_vals.assign(milp.vars.size(), 0.0);

        std::map<std::string, const model::AllocatedLesson*> aloc_map;
        for (const auto& a : active_seed->alocacoes()) {
            aloc_map[a.aula_id] = &a;
        }

        for (const auto& aula : data.aulas()) {
            auto it = aloc_map.find(aula.id());
            if (it == aloc_map.end()) continue;
            const auto* aloc = it->second;

            const auto& items = milp.vars_por_aula[aula.id()];
            const MilpCandItem* matching_item = nullptr;
            for (const auto& item : items) {
                if (item.cand->dia == aloc->dia && item.cand->start_slot == aloc->intervalo.start_slot) {
                    if (item.cand->salas == aloc->salas || matching_item == nullptr) {
                        matching_item = &item;
                    }
                }
            }
            if (matching_item) {
                milp.warm_start_vals[matching_item->col] = 1.0;
                milp.hints_aplicados++;
            }
        }

        if (milp.hints_aplicados > 0) {
            // Preencher z_{g,r}
            for (const auto& turma : data.turmas()) {
                std::string chosen_r;
                auto it_des = active_seed->salasTeoricasDesignadas().find(turma.id());
                if (it_des != active_seed->salasTeoricasDesignadas().end()) {
                    chosen_r = it_des->second;
                } else if (turma.temSalaTeoricaPredefinida()) {
                    chosen_r = turma.salaTeoricaId();
                } else if (!turma.salasTeoricasPermitidas().empty()) {
                    chosen_r = turma.salasTeoricasPermitidas().front();
                }
                if (!chosen_r.empty()) {
                    auto it_z = milp.z_turma_sala.find({turma.id(), chosen_r});
                    if (it_z != milp.z_turma_sala.end()) {
                        milp.warm_start_vals[it_z->second] = 1.0;
                    }
                }
            }

            // Preencher y_{p,d}
            for (const auto& [key, y_col] : milp.y_prof_dia) {
                double active = 0.0;
                for (const auto& item : milp.vars_prof_dia[key]) {
                    if (milp.warm_start_vals[item.col] > 0.5) {
                        active = 1.0;
                        break;
                    }
                }
                milp.warm_start_vals[y_col] = active;
            }

            // Preencher w_d de aulas consecutivas
            for (const auto& rec : milp.consec_records) {
                double active = 0.0;
                for (int c_col : rec.day_cols) {
                    if (milp.warm_start_vals[c_col] > 0.5) {
                        active = 1.0;
                        break;
                    }
                }
                milp.warm_start_vals[rec.w_d_col] = active;
            }

            // Preencher y_shift
            for (const auto& rec : milp.shift_records) {
                double active = 0.0;
                for (int c_col : rec.shift_cols) {
                    if (milp.warm_start_vals[c_col] > 0.5) {
                        active = 1.0;
                        break;
                    }
                }
                milp.warm_start_vals[rec.y_shift_col] = active;
            }

            // Preencher u, v, g / q de janelas e lacunas
            for (const auto& rec : milp.gap_records) {
                double u_val = 0.0;
                for (int b : rec.before_cols) {
                    if (milp.warm_start_vals[b] > 0.5) { u_val = 1.0; break; }
                }
                double v_val = 0.0;
                for (int a : rec.after_cols) {
                    if (milp.warm_start_vals[a] > 0.5) { v_val = 1.0; break; }
                }
                double w_val = 0.0;
                for (int c_col : rec.cover_cols) {
                    w_val += milp.warm_start_vals[c_col];
                }
                double g_val = std::max(0.0, u_val + v_val - 1.0 - w_val);
                milp.warm_start_vals[rec.u_col] = u_val;
                milp.warm_start_vals[rec.v_col] = v_val;
                if (rec.g_col >= 0) {
                    milp.warm_start_vals[rec.g_col] = (g_val > 0.5) ? 1.0 : 0.0;
                }
            }

            milp.has_warm_start = true;
            if (from_screen) {
                timetable.addMensagemDiagnostico("[WARM-START] " + std::to_string(milp.hints_aplicados) +
                                                 " aulas da distribuição atual em tela foram fornecidas como solução inicial.");
            }
        }
    }

    return true;
}

/**
 * @brief Extrai a grade horária e estatísticas a partir do vetor solução de um solver MILP (SCIP ou HiGHS).
 */
static void extractMilpSolution(const model::InstitutionData& data,
                                const MilpProblem& milp,
                                const std::vector<double>& sol_vals,
                                double obj_val,
                                double tempo_ms,
                                const std::string& status_str,
                                model::Timetable& timetable) {
    model::TimetableStatistics stats;
    stats.tempo_execucao_ms = tempo_ms;
    stats.valor_objetivo = obj_val;
    stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
    stats.status = status_str;

    for (const auto& item : milp.todas_vars) {
        if (item.col < static_cast<int>(sol_vals.size()) && sol_vals[item.col] > 0.5) {
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
                    aloc.semestre_alocado = 3;
                } else {
                    aloc.semestre_alocado = (aula->semestre() == model::SemestreLetivo::SEMESTRE_2 || aula->semestreAtivo() == 2) ? 2 : 1;
                }
            }
            timetable.addAllocatedLesson(std::move(aloc));
        }
    }
    stats.total_aulas_alocadas = static_cast<int>(timetable.alocacoes().size());

    for (const auto& [par, z_col] : milp.z_turma_sala) {
        if (z_col < static_cast<int>(sol_vals.size()) && sol_vals[z_col] > 0.5) {
            timetable.setSalaTeoricaDesignada(par.first, par.second);
        }
    }

    int total_D = 0;
    int total_W = 0;
    for (const auto& prof : data.professores()) {
        for (auto dia : model::allDays()) {
            auto it = milp.y_prof_dia.find({prof.id(), dia});
            if (it != milp.y_prof_dia.end() && it->second < static_cast<int>(sol_vals.size()) && sol_vals[it->second] > 0.5) {
                total_D++;
                total_W += prof.penalidadeDia(dia);
            }
        }
    }
    stats.penalidade_dias_D = total_D;
    stats.penalidade_preferencia_dias_W = total_W;

    int total_J_slots = 0;
    for (const auto& [g_col, peso] : milp.j_terms) {
        if (g_col < static_cast<int>(sol_vals.size()) && sol_vals[g_col] > 0.5) {
            total_J_slots += peso;
        }
    }
    stats.penalidade_janelas_J = total_J_slots * 5;

    int total_JG_slots = 0;
    for (int q_col : milp.jg_cols) {
        if (q_col < static_cast<int>(sol_vals.size()) && sol_vals[q_col] > 0.5) {
            total_JG_slots++;
        }
    }
    stats.penalidade_lacunas_turma_JG = total_JG_slots * 5;

    int total_P = 0;
    for (const auto& [x_col, custo] : milp.p_terms) {
        if (x_col < static_cast<int>(sol_vals.size()) && sol_vals[x_col] > 0.5) {
            total_P += custo;
        }
    }
    stats.penalidade_preferencias_P = total_P;

    timetable.setEstatisticas(stats);
}

} // namespace

/**
 * @brief Resolve o escalonamento utilizando o solver nativo SCIP (Solving Constraint Integer Programs).
 *
 * Constrói um programa linear inteiro misto / por restrições completo na API nativa do SCIP,
 * com suporte a cortes de clique por slot, salas teóricas fixas, janelas e Warm Start.
 *
 * @param data Dados acadêmicos da instituição.
 * @param callback Callback opcional para notificação de progresso.
 * @return Grade horária gerada pelo SCIP.
 */
model::Timetable ScheduleSolver::solveScip(const model::InstitutionData& data,
                                           ProgressCallback callback) {
    model::Timetable timetable;
    auto start_time = std::chrono::steady_clock::now();

    model::Timetable seed_tt;
    const model::Timetable* seed_ptr = nullptr;
    if (data.grade().alocacoes().empty() || !data.config().usar_distribuicao_como_entrada) {
        seed_tt = solveGreedyHeuristic(data, nullptr);
        if (seed_tt.estatisticas().total_aulas_alocadas == static_cast<int>(data.aulas().size())) {
            seed_ptr = &seed_tt;
        }
    }

    MilpProblem milp;
    if (!buildMilpProblem(data, milp, timetable, callback, "SCIP", seed_ptr)) {
        return timetable;
    }

    if (callback) callback("SCIP: Inicializando motor SCIP (Branch-Cut-and-Price)...", 0.55);

    SCIP* scip = nullptr;
    if (SCIPcreate(&scip) != SCIP_OKAY || scip == nullptr) {
        model::TimetableStatistics stats;
        stats.status = "UNKNOWN";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[ERRO] Não foi possível inicializar a instância nativa do SCIP.");
        return timetable;
    }

    SCIPincludeDefaultPlugins(scip);
    SCIPcreateProbBasic(scip, "EscalonadorHorarios_SCIP");
    SCIPsetObjsense(scip, SCIP_OBJSENSE_MINIMIZE);

    // Parâmetros de execução do SCIP
    SCIPsetIntParam(scip, "display/verblevel", data.config().log_search_progress ? 4 : 0);
    if (!data.config().sem_limite_tempo && data.config().tempo_limite_segundos > 0.0) {
        SCIPsetRealParam(scip, "limits/time", data.config().tempo_limite_segundos);
    }
    if (!data.config().sem_limite_memoria && data.config().max_memoria_mb > 0) {
        SCIPsetRealParam(scip, "limits/memory", static_cast<SCIP_Real>(data.config().max_memoria_mb));
    }
    if (data.config().num_threads > 0) {
        SCIPsetIntParam(scip, "parallel/maxnthreads", data.config().num_threads);
    }

    // Criação das variáveis binárias no SCIP
    std::vector<SCIP_VAR*> scip_vars(milp.vars.size(), nullptr);
    for (size_t i = 0; i < milp.vars.size(); ++i) {
        const auto& v = milp.vars[i];
        SCIPcreateVarBasic(scip, &scip_vars[i], v.name.c_str(), v.lb, v.ub, v.obj, SCIP_VARTYPE_BINARY);
        SCIPaddVar(scip, scip_vars[i]);
    }

    // Adição das restrições lineares no SCIP
    for (const auto& r : milp.rows) {
        if (r.cols.empty()) continue;
        std::vector<SCIP_VAR*> row_vars(r.cols.size());
        std::vector<SCIP_Real> row_vals(r.vals.begin(), r.vals.end());
        for (size_t j = 0; j < r.cols.size(); ++j) {
            row_vars[j] = scip_vars[r.cols[j]];
        }
        SCIP_Real lhs = (r.lhs <= -1e19) ? -SCIPinfinity(scip) : r.lhs;
        SCIP_Real rhs = (r.rhs >= 1e19) ? SCIPinfinity(scip) : r.rhs;

        SCIP_CONS* cons = nullptr;
        SCIPcreateConsBasicLinear(scip, &cons, r.name.c_str(),
                                  static_cast<int>(row_vars.size()), row_vars.data(), row_vals.data(),
                                  lhs, rhs);
        SCIPaddCons(scip, cons);
        SCIPreleaseCons(scip, &cons);
    }

    // Injeção de Solução Inicial (Warm Start) no SCIP (estágio PROBLEM -> SCIPaddSolFree)
    if (milp.has_warm_start && milp.warm_start_vals.size() == scip_vars.size()) {
        SCIP_SOL* init_sol = nullptr;
        if (SCIPcreateSol(scip, &init_sol, nullptr) == SCIP_OKAY && init_sol != nullptr) {
            for (size_t i = 0; i < scip_vars.size(); ++i) {
                SCIPsetSolVal(scip, init_sol, scip_vars[i], milp.warm_start_vals[i]);
            }
            SCIP_Bool stored = FALSE;
            SCIPaddSolFree(scip, &init_sol, &stored);
        }
    }

    if (callback) callback("Executando solver SCIP...", 0.65);

    SCIPsolve(scip);

    auto end_time = std::chrono::steady_clock::now();
    double tempo_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    SCIP_STATUS scip_status = SCIPgetStatus(scip);
    SCIP_SOL* best_sol = SCIPgetBestSol(scip);
    long long n_nodes = SCIPgetNNodes(scip);

    std::string status_str = "UNKNOWN";
    if (scip_status == SCIP_STATUS_OPTIMAL) {
        status_str = "OPTIMAL";
    } else if (best_sol != nullptr) {
        status_str = "FEASIBLE";
    } else if (scip_status == SCIP_STATUS_INFEASIBLE) {
        status_str = "INFEASIBLE";
    }

    if (status_str == "OPTIMAL" || status_str == "FEASIBLE") {
        if (callback) callback("SCIP: Extraindo solução encontrada...", 0.90);
        std::vector<double> sol_vals(scip_vars.size(), 0.0);
        for (size_t i = 0; i < scip_vars.size(); ++i) {
            sol_vals[i] = SCIPgetSolVal(scip, best_sol, scip_vars[i]);
        }
        double obj_val = SCIPgetSolOrigObj(scip, best_sol);
        extractMilpSolution(data, milp, sol_vals, obj_val, tempo_ms, status_str, timetable);
        timetable.addMensagemDiagnostico(
            "[SCIP] Solução obtida via SCIP v" + std::to_string(SCIPmajorVersion()) + "." +
            std::to_string(SCIPminorVersion()) + "." + std::to_string(SCIPtechVersion()) +
            " (" + std::to_string(milp.vars.size()) + " variáveis, " +
            std::to_string(milp.rows.size()) + " restrições, " +
            std::to_string(n_nodes) + " nós B&B).");
        if (callback) callback("Grade gerada com sucesso pelo SCIP!", 1.0);
    } else if (status_str == "INFEASIBLE") {
        model::TimetableStatistics stats;
        stats.tempo_execucao_ms = tempo_ms;
        stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
        stats.status = "INFEASIBLE";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[INVIABILIDADE] O solver SCIP comprovou que o conjunto de restrições não possui solução viável.");
        auto diag_tt = solveGreedyHeuristic(data, nullptr);
        for (const auto& msg : diag_tt.mensagensDiagnostico()) {
            if (msg.find("Não foi possível alocar a aula") != std::string::npos) {
                timetable.addMensagemDiagnostico(msg);
            }
        }
        if (callback) callback("SCIP: Problema inviável para as restrições informadas.", 1.0);
    } else {
        model::TimetableStatistics stats;
        stats.tempo_execucao_ms = tempo_ms;
        stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
        stats.status = "UNKNOWN";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[TEMPO_LIMITE] O solver SCIP atingiu o limite de tempo/memória antes de encontrar uma solução viável completa.");
        if (callback) callback("SCIP: Limite atingido sem solução viável.", 1.0);
    }

    for (size_t i = 0; i < scip_vars.size(); ++i) {
        if (scip_vars[i] != nullptr) {
            SCIPreleaseVar(scip, &scip_vars[i]);
        }
    }
    SCIPfree(&scip);

    auditarConflitosEntreAulas(data, timetable);
    return timetable;
}

/**
 * @brief Resolve o escalonamento utilizando o solver nativo HiGHS (High Performance Optimization Software).
 *
 * Constrói a matriz esparsa comprimida (CSR) do problema linear inteiro misto na API C++ do HiGHS,
 * aplicando pré-processamento algébrico, planos de corte, Dual Simplex e Branch-and-Cut paralelo.
 *
 * @param data Dados acadêmicos da instituição.
 * @param callback Callback opcional para notificação de progresso.
 * @return Grade horária gerada pelo HiGHS.
 */
model::Timetable ScheduleSolver::solveHighs(const model::InstitutionData& data,
                                            ProgressCallback callback) {
    model::Timetable timetable;
    auto start_time = std::chrono::steady_clock::now();

    model::Timetable seed_tt;
    const model::Timetable* seed_ptr = nullptr;
    if (data.grade().alocacoes().empty() || !data.config().usar_distribuicao_como_entrada) {
        seed_tt = solveGreedyHeuristic(data, nullptr);
        if (seed_tt.estatisticas().total_aulas_alocadas == static_cast<int>(data.aulas().size())) {
            seed_ptr = &seed_tt;
        }
    }

    MilpProblem milp;
    if (!buildMilpProblem(data, milp, timetable, callback, "HiGHS", seed_ptr)) {
        return timetable;
    }

    if (callback) callback("HiGHS: Montando matriz esparsa e inicializando MILP...", 0.55);

    Highs highs;
    highs.setOptionValue("output_flag", data.config().log_search_progress);
    if (!data.config().sem_limite_tempo && data.config().tempo_limite_segundos > 0.0) {
        highs.setOptionValue("time_limit", data.config().tempo_limite_segundos);
    }
    if (data.config().num_threads > 0) {
        highs.setOptionValue("threads", data.config().num_threads);
    }

    HighsModel model;
    HighsLp& lp = model.lp_;
    lp.model_name_ = "EscalonadorHorarios_HiGHS";
    lp.num_col_ = static_cast<HighsInt>(milp.vars.size());
    lp.num_row_ = static_cast<HighsInt>(milp.rows.size());
    lp.sense_ = ObjSense::kMinimize;
    lp.offset_ = 0.0;

    lp.col_cost_.resize(lp.num_col_);
    lp.col_lower_.resize(lp.num_col_);
    lp.col_upper_.resize(lp.num_col_);
    lp.integrality_.resize(lp.num_col_, HighsVarType::kInteger);

    for (size_t i = 0; i < milp.vars.size(); ++i) {
        lp.col_cost_[i] = milp.vars[i].obj;
        lp.col_lower_[i] = milp.vars[i].lb;
        lp.col_upper_[i] = milp.vars[i].ub;
    }

    lp.row_lower_.resize(lp.num_row_);
    lp.row_upper_.resize(lp.num_row_);
    lp.a_matrix_.clear();
    lp.a_matrix_.format_ = MatrixFormat::kRowwise;
    lp.a_matrix_.num_col_ = lp.num_col_;
    lp.a_matrix_.num_row_ = lp.num_row_;
    lp.a_matrix_.start_.clear();
    lp.a_matrix_.index_.clear();
    lp.a_matrix_.value_.clear();
    lp.a_matrix_.start_.reserve(lp.num_row_ + 1);
    lp.a_matrix_.start_.push_back(0);

    for (size_t r_idx = 0; r_idx < milp.rows.size(); ++r_idx) {
        const auto& r = milp.rows[r_idx];
        lp.row_lower_[r_idx] = (r.lhs <= -1e19) ? -kHighsInf : r.lhs;
        lp.row_upper_[r_idx] = (r.rhs >= 1e19) ? kHighsInf : r.rhs;
        for (size_t k = 0; k < r.cols.size(); ++k) {
            lp.a_matrix_.index_.push_back(static_cast<HighsInt>(r.cols[k]));
            lp.a_matrix_.value_.push_back(r.vals[k]);
        }
        lp.a_matrix_.start_.push_back(static_cast<HighsInt>(lp.a_matrix_.index_.size()));
    }
    lp.ensureColwise();

    HighsStatus pass_status = highs.passModel(model);
    if (pass_status == HighsStatus::kError) {
        model::TimetableStatistics stats;
        stats.status = "UNKNOWN";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[ERRO] Falha ao transferir o modelo linear para o motor HiGHS.");
        return timetable;
    }

    // Injeção de Solução Inicial (Warm Start) no HiGHS
    if (milp.has_warm_start && milp.warm_start_vals.size() == milp.vars.size()) {
        HighsSolution init_sol;
        init_sol.col_value = milp.warm_start_vals;
        highs.setSolution(init_sol);
    }

    if (callback) callback("Executando solver HiGHS (Branch-and-Cut)...", 0.65);

    highs.run();

    auto end_time = std::chrono::steady_clock::now();
    double tempo_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    HighsModelStatus model_status = highs.getModelStatus();
    const HighsInfo& info = highs.getInfo();
    const HighsSolution& sol = highs.getSolution();
    bool has_feasible_sol = (info.primal_solution_status == kSolutionStatusFeasible) &&
                            (sol.col_value.size() == milp.vars.size());

    std::string status_str = "UNKNOWN";
    if (model_status == HighsModelStatus::kOptimal && has_feasible_sol) {
        status_str = "OPTIMAL";
    } else if (has_feasible_sol) {
        status_str = "FEASIBLE";
    } else if (model_status == HighsModelStatus::kInfeasible) {
        status_str = "INFEASIBLE";
    }

    if (status_str == "OPTIMAL" || status_str == "FEASIBLE") {
        if (callback) callback("HiGHS: Extraindo solução encontrada...", 0.90);
        extractMilpSolution(data, milp, sol.col_value, info.objective_function_value, tempo_ms, status_str, timetable);
        timetable.addMensagemDiagnostico(
            "[HiGHS] Solução obtida via HiGHS v" + std::string(highsVersion()) +
            " (" + std::to_string(milp.vars.size()) + " variáveis, " +
            std::to_string(milp.rows.size()) + " restrições, " +
            std::to_string(info.mip_node_count) + " nós B&C).");
        if (callback) callback("Grade gerada com sucesso pelo HiGHS!", 1.0);
    } else if (status_str == "INFEASIBLE") {
        model::TimetableStatistics stats;
        stats.tempo_execucao_ms = tempo_ms;
        stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
        stats.status = "INFEASIBLE";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[INVIABILIDADE] O solver HiGHS comprovou que o conjunto de restrições não possui solução viável.");
        auto diag_tt = solveGreedyHeuristic(data, nullptr);
        for (const auto& msg : diag_tt.mensagensDiagnostico()) {
            if (msg.find("Não foi possível alocar a aula") != std::string::npos) {
                timetable.addMensagemDiagnostico(msg);
            }
        }
        if (callback) callback("HiGHS: Problema inviável para as restrições informadas.", 1.0);
    } else {
        model::TimetableStatistics stats;
        stats.tempo_execucao_ms = tempo_ms;
        stats.total_aulas_esperadas = static_cast<int>(data.aulas().size());
        stats.status = "UNKNOWN";
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[TEMPO_LIMITE] O solver HiGHS atingiu o tempo limite antes de encontrar uma solução viável completa.");
        if (callback) callback("HiGHS: Tempo limite atingido sem solução viável.", 1.0);
    }

    auditarConflitosEntreAulas(data, timetable);
    return timetable;
}

/**
 * @brief Resolve o problema via formulação linear inteira mista (MILP - alias para HiGHS/SCIP).
 *
 * @param data Dados acadêmicos da instituição.
 * @param callback Callback opcional para notificação de progresso.
 * @return Grade horária gerada.
 */
model::Timetable ScheduleSolver::solveMilp(const model::InstitutionData& data,
                                           ProgressCallback callback) {
    return solveHighs(data, callback);
}

} // namespace scheduler::solver
