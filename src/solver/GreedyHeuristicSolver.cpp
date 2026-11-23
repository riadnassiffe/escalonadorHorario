/**
 * @file GreedyHeuristicSolver.cpp
 * @brief Implementação da heurística construtiva gulosa para escalonamento de horários.
 */

#include "solver/ScheduleSolver.h"
#include "solver/SolverCommon.h"
#include <algorithm>
#include <chrono>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace scheduler::solver {

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
            int s = c.start_min;
            int e = c.end_min();
            double penalty = 0.0;

            if (prior_aloc && c.dia == prior_aloc->dia && c.start_min == prior_aloc->intervalo.start_min &&
                c.end_min() == prior_aloc->intervalo.end_min) {
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

            // Início prioritário da primeira aula no primeiro slot da jornada da turma
            for (const auto& t_id : aula->turmas()) {
                const auto* t = data.findTurma(t_id);
                if (t) {
                    auto j = t->jornadaParaDia(c.dia);
                    if (!j.slots_letivos.empty()) {
                        int t_ini = j.slots_letivos.front().start_min;
                        const auto& existing = turma_ocup[{t_id, c.dia}];
                        bool existing_in_sem = false;
                        for (const auto& r : existing) {
                            if (semestres_conflitam(r.semester, aula_sem)) {
                                existing_in_sem = true;
                                break;
                            }
                        }
                        if (!existing_in_sem) {
                            if (c.start_min == t_ini) {
                                penalty -= 1000.0; // Forte incentivo para a primeira aula iniciar no início da jornada
                            } else {
                                penalty += 800.0; // Desestimula iniciar o dia fora do horário inicial
                            }
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
                auto c_shift = model::timeToShift(c.start_min);
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
                        if (semestres_conflitam(r.semester, aula_sem) && model::timeToShift(r.start) == c_shift) {
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
                        int gap_min = (max_e - min_s) - occupied_len;
                        if (gap_min > 0) {
                            penalty += gap_min * data.config().lambda_janelas * p->pesoJanela();
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
            int s = best_cand->start_min;
            int e = best_cand->end_min();

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
                            int s1 = aula->pinnedInterval().start_min;
                            int e1 = aula->pinnedInterval().durationMinutes() > 0
                                         ? aula->pinnedInterval().end_min
                                         : (s1 + aula->duracaoMinutos());
                            int s2 = a_outra.intervalo.start_min;
                            int e2 = a_outra.intervalo.end_min;
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
                    int s = c.start_min;
                    int e = c.end_min();

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

    detail::auditarConflitosEntreAulas(data, timetable);
    timetable.setEstatisticas(stats);
    return timetable;
}

} // namespace scheduler::solver
