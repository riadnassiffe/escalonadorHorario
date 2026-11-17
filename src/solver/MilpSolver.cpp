/**
 * @file MilpSolver.cpp
 * @brief Implementação da formulação MILP compartilhada e do método solveMilp.
 */

#include "solver/ScheduleSolver.h"
#include "solver/MilpModel.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace scheduler::solver {

namespace detail {

int MilpProblem::addBinVar(const std::string& name, double obj, double lb, double ub) {
    int idx = static_cast<int>(vars.size());
    vars.push_back(MilpVar{name, lb, ub, obj});
    return idx;
}

void MilpProblem::addRow(const std::string& name,
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

bool buildMilpProblem(const model::InstitutionData& data,
                      MilpProblem& milp,
                      model::Timetable& timetable,
                      ProgressCallback callback,
                      const std::string& solver_tag,
                      const model::Timetable* heuristic_seed) {
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

void extractMilpSolution(const model::InstitutionData& data,
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

} // namespace detail

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
