#include "solver/ScheduleSolver.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/sat_parameters.pb.h"
#include <chrono>
#include <set>
#include <map>
#include <iostream>

namespace scheduler::solver {

using namespace operations_research;
using namespace operations_research::sat;

model::Timetable ScheduleSolver::solve(const model::InstitutionData& data,
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

    // 3. Criar variáveis de decisão x_{a,d,t,b} (seção 2.6)
    int var_idx = 0;
    for (const auto& aula : data.aulas()) {
        const auto& cands = todos_candidatos[aula.id()];
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
            }
            // Mapeamento por turmas
            for (const auto& t_id : aula.turmas()) {
                vars_turma_dia[{t_id, c.dia}].push_back(item);
            }
            // Mapeamento por salas
            for (const auto& s_id : c.salas) {
                vars_sala_dia[{s_id, c.dia}].push_back(item);
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
    // Utiliza AddNoOverlap do CP-SAT com intervalos opcionais
    int int_id = 0;
    for (const auto& prof : data.professores()) {
        for (auto dia : model::allDays()) {
            const auto& items = vars_prof_dia[{prof.id(), dia}];
            if (items.size() <= 1) continue;

            std::vector<IntervalVar> intervals;
            for (const auto& item : items) {
                std::string name = "int_p_" + prof.id() + "_" + std::to_string(int_id++);
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

    // 5.3 Não sobreposição de salas (seção 3.3)
    for (const auto& sala : data.salas()) {
        for (auto dia : model::allDays()) {
            const auto& items = vars_sala_dia[{sala.id(), dia}];
            if (items.size() <= 1) continue;

            std::vector<IntervalVar> intervals;
            for (const auto& item : items) {
                std::string name = "int_r_" + sala.id() + "_" + std::to_string(int_id++);
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

    // 5.4 Não sobreposição de turmas (seção 3.4)
    for (const auto& turma : data.turmas()) {
        for (auto dia : model::allDays()) {
            const auto& items = vars_turma_dia[{turma.id(), dia}];
            if (items.size() <= 1) continue;

            std::vector<IntervalVar> intervals;
            for (const auto& item : items) {
                std::string name = "int_t_" + turma.id() + "_" + std::to_string(int_id++);
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

    // 5.5 Carga máxima diária do professor (seção 3.9)
    for (const auto& prof : data.professores()) {
        if (prof.cargaMaximaDiariaMinutos() > 0) {
            for (auto dia : model::allDays()) {
                const auto& items = vars_prof_dia[{prof.id(), dia}];
                LinearExpr minutos_dia;
                for (const auto& item : items) {
                    minutos_dia += item.cand->duration_minutes * item.var;
                }
                cp_model.AddLessOrEqual(minutos_dia, prof.cargaMaximaDiariaMinutos());
            }
        }
    }

    // 5.6 Preenchimento integral opcional da jornada (seção 3.10)
    if (data.config().preenchimento_integral_jornada) {
        for (const auto& turma : data.turmas()) {
            for (auto dia : turma.diasLetivos()) {
                auto jornada = turma.jornadaParaDia(dia);
                auto blocos = jornada.blocosLetivos();
                const auto& items = vars_turma_dia[{turma.id(), dia}];

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
    }

    // 5.7 Distribuição semanal de disciplinas (seção 5):
    // Limitar máximo de aulas da mesma disciplina por dia para cada turma
    if (data.config().distribuir_disciplinas_na_semana) {
        int max_por_dia = data.config().max_aulas_disciplina_dia_turma;
        if (max_por_dia > 0) {
            for (const auto& disc : data.disciplinas()) {
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

    if (callback) callback("Configurando função objetivo e preferências...", 0.40);

    // 6. Função Objetivo (seção 4 & 8):
    // min Z = lambda_1 * J + lambda_2 * D + lambda_3 * P + lambda_4 * J_G + lambda_5 * W

    LinearExpr expr_D;   // Dias de trabalho
    LinearExpr expr_W;   // Penalidades de dias menos preferidos
    LinearExpr expr_P;   // Preferências de horários
    LinearExpr expr_J;   // Janelas de professores
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

    // 6.3 Termo J: Minimizar janelas entre aulas dos professores (seção 4.1)
    // Modelo linear exato: g_{p,d,k} >= u_{p,d,k} + v_{p,d,k} - 1 - w_{p,d,k}
    int gap_var_idx = 0;
    for (const auto& prof : data.professores()) {
        for (auto dia : model::allDays()) {
            const auto& items = vars_prof_dia[{prof.id(), dia}];
            if (items.size() <= 1) continue;

            // Encontrar menor e maior slot possível
            int min_s = 288, max_e = 0;
            for (const auto& item : items) {
                min_s = std::min(min_s, item.cand->start_slot);
                max_e = std::max(max_e, item.cand->end_slot());
            }

            // Para cada slot k intermediário
            for (int k = min_s; k < max_e; ++k) {
                // Verificar se existem aulas que podem começar antes/em k e aulas depois/em k
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
                    BoolVar u = cp_model.NewBoolVar().WithName("u_" + std::to_string(gap_var_idx));
                    BoolVar v = cp_model.NewBoolVar().WithName("v_" + std::to_string(gap_var_idx));
                    BoolVar g = cp_model.NewBoolVar().WithName("g_" + std::to_string(gap_var_idx++));

                    // u = 1 se alguma aula já terminou antes de k
                    for (const auto& b : cands_before) {
                        cp_model.AddLessOrEqual(b, u);
                    }
                    // v = 1 se alguma aula ainda vai começar depois de k
                    for (const auto& a : cands_after) {
                        cp_model.AddLessOrEqual(a, v);
                    }

                    // g >= u + v - 1 - w_k
                    cp_model.AddGreaterOrEqual(g, u + v - 1 - w_k);

                    expr_J += g; // Cada slot g representa 5 minutos
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
    funcao_objetivo += data.config().lambda_janelas * (5 * expr_J);
    funcao_objetivo += data.config().lambda_dias * expr_D;
    funcao_objetivo += data.config().lambda_preferencias * expr_P;
    funcao_objetivo += data.config().lambda_preferencia_dias * expr_W;
    funcao_objetivo += data.config().lambda_lacunas_turma * (5 * expr_JG);

    cp_model.Minimize(funcao_objetivo);

    if (callback) callback("Executando solver CP-SAT...", 0.60);

    // 7. Parâmetros e Resolução
    SatParameters params;
    params.set_max_time_in_seconds(data.config().tempo_limite_segundos);
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
    } else {
        timetable.setEstatisticas(stats);
        timetable.addMensagemDiagnostico("[INVIABILIDADE] O solver CP-SAT determinou que o conjunto de restrições não possui solução viável.");
        timetable.addMensagemDiagnostico("Dica: Verifique a disponibilidade dos professores, salas compatíveis, ou flexibilize a jornada e limites diários.");
        if (callback) callback("Inviável: Sem solução para as restrições informadas.", 1.0);
    }

    return timetable;
}

} // namespace scheduler::solver
