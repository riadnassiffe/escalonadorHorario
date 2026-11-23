/**
 * @file ScheduleReportExporter.cpp
 * @brief Implementação dos exportadores de relatórios de grade horária (HTML e CSV).
 */

#include "ui/ScheduleReportExporter.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <set>

namespace scheduler::ui {

/**
 * @brief Exporta as alocações da grade em arquivo de texto com formato delimitado por ponto e vírgula (.csv).
 * @param filepath Caminho do arquivo a ser gravado.
 * @param data Dados acadêmicos da instituição.
 * @param err Mensagem de erro caso a escrita falhe.
 * @return true se exportou com sucesso; false caso contrário.
 */
bool ScheduleReportExporter::exportarCsv(const std::string& filepath,
                                         const model::InstitutionData& data,
                                         std::string& err) {
    try {
        std::ofstream ofs(filepath);
        if (!ofs.is_open()) {
            err = "Não foi possível abrir o arquivo CSV para escrita: " + filepath;
            return false;
        }

        ofs << "Aula_ID;Disciplina;Tipo;Regime;Semestre;Dia;Inicio;Fim;Duracao_Min;Professores;Turmas;Salas;Fixada\n";
        for (const auto& a : data.grade().alocacoes()) {
            ofs << a.aula_id << ";"
                << a.disciplina_nome << ";"
                << a.tipo << ";"
                << a.regimeSemestreFormatado() << ";"
                << (a.semestre_alocado == 3 ? "Ano Todo" : (a.semestre_alocado == 2 ? "2º Sem" : "1º Sem")) << ";"
                << a.diaString() << ";"
                << a.intervalo.startString() << ";"
                << a.intervalo.endString() << ";"
                << a.intervalo.durationMinutes() << ";";

            // Professores
            for (size_t i = 0; i < a.professores.size(); ++i) {
                if (i > 0) ofs << ", ";
                const auto* p = data.findProfessor(a.professores[i]);
                ofs << (p ? p->nome() : a.professores[i]);
            }
            ofs << ";";

            // Turmas
            for (size_t i = 0; i < a.turmas.size(); ++i) {
                if (i > 0) ofs << ", ";
                const auto* t = data.findTurma(a.turmas[i]);
                ofs << (t ? t->nome() : a.turmas[i]);
            }
            ofs << ";";

            // Salas
            for (size_t i = 0; i < a.salas.size(); ++i) {
                if (i > 0) ofs << ", ";
                const auto* s = data.findSala(a.salas[i]);
                ofs << (s ? s->nome() : a.salas[i]);
            }
            ofs << ";";

            ofs << (a.is_pinned ? "SIM" : "NAO") << "\n";
        }
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    }
}

/**
 * @brief Gera a tabela semanal formatada em HTML para uma turma, incluindo aulas e recreios.
 * @param data Dados da instituição.
 * @param turma Turma cujos horários serão gerados.
 * @return Fragmento HTML com a tabela.
 */
std::string ScheduleReportExporter::gerarTabelaHtmlTurma(const model::InstitutionData& data,
                                                        const model::Turma& turma) {
    std::ostringstream ss;
    ss << "<div class='section-card'>\n";
    ss << "  <h2>Turma: " << turma.nome() << " (" << turma.id() << ")</h2>\n";
    ss << "  <p class='badge-info'>Turno: " << model::shiftToString(turma.turno())
       << " | Sala Teórica Fixa: " << (turma.temSalaTeoricaPredefinida() ? turma.salaTeoricaId() : data.grade().salaTeoricaTurma(turma.id()))
       << "</p>\n";

    ss << "  <table class='timetable'>\n";
    ss << "    <thead><tr><th>Horário</th>";
    for (auto d : model::allDays()) {
        ss << "<th>" << model::dayToLongString(d) << "</th>";
    }
    ss << "</tr></thead>\n";
    ss << "    <tbody>\n";

    // Obter horários ordenados de aulas e slots da turma
    std::set<std::pair<int, int>> horarios_set;
    for (auto d : model::allDays()) {
        auto aulas = data.grade().aulasPorTurma(turma.id(), d);
        for (const auto& a : aulas) {
            horarios_set.insert({a.intervalo.start_min, a.intervalo.end_min});
        }
    }

    // Incluir slots letivos e intervalos de descanso de todos os dias letivos
    for (auto d : turma.diasLetivos()) {
        auto j_dia = turma.jornadaParaDia(d);
        for (const auto& s : j_dia.slots_letivos) {
            horarios_set.insert({s.start_min, s.end_min});
        }
        for (const auto& inter : j_dia.intervalosAtivos()) {
            horarios_set.insert({inter.start_min, inter.end_min});
        }
    }

    for (const auto& [s, e] : horarios_set) {
        model::TimeInterval row_inter(s, e);

        ss << "    <tr>\n";
        ss << "      <td class='time-col'>" << row_inter.toString() << "</td>\n";

        for (auto d : model::allDays()) {
            auto j_dia = turma.jornadaParaDia(d);
            auto j_total = j_dia.jornadaTotal();
            bool is_recess = false;
            for (const auto& inter : j_dia.intervalosAtivos()) {
                if (s == inter.start_min && e == inter.end_min) {
                    is_recess = true;
                    break;
                }
            }

            if (is_recess) {
                int dur = e - s;
                ss << "      <td class='recess-cell'>INTERVALO (" << dur << " min)</td>\n";
            } else if (!turma.funcionaNoDia(d) || j_dia.slots_letivos.empty() || s < j_total.start_min || e > j_total.end_min) {
                ss << "      <td class='off-cell' style='background:#f1f5f9; color:#94a3b8; text-align:center;'>—</td>\n";
            } else {
                auto aulas = data.grade().aulasPorTurma(turma.id(), d);
                std::string cell_content = "-";
                for (const auto& a : aulas) {
                    if (a.intervalo.start_min == s && a.intervalo.end_min == e) {
                        std::string prof_names;
                        for (size_t i = 0; i < a.professores.size(); ++i) {
                            if (i > 0) prof_names += ", ";
                            const auto* p = data.findProfessor(a.professores[i]);
                            prof_names += (p ? p->nome() : a.professores[i]);
                        }
                        std::string sala_names;
                        for (size_t i = 0; i < a.salas.size(); ++i) {
                            if (i > 0) sala_names += ", ";
                            const auto* sl = data.findSala(a.salas[i]);
                            sala_names += (sl ? sl->nome() : a.salas[i]);
                        }

                        std::string badge_cls = (a.regime == model::RegimeDisciplina::ANUAL) ? "badge-anual" : (a.semestre_alocado == 2 ? "badge-sem2" : "badge-sem1");
                        std::string badge_str = "<span class='badge-regime " + badge_cls + "'>" + a.regimeSemestreFormatado() + "</span>";

                        cell_content = "<div class='lesson-card " + a.tipo + "'>" +
                                       "<b>" + a.disciplina_nome + "</b> " + badge_str + "<br>" +
                                       "<span class='prof-tag'>" + prof_names + "</span><br>" +
                                       "<span class='room-tag'>Sala: " + sala_names + "</span>" +
                                       "</div>";
                        break;
                    }
                }
                ss << "      <td>" << cell_content << "</td>\n";
            }
        }
        ss << "    </tr>\n";
    }

    ss << "    </tbody>\n";
    ss << "  </table>\n";
    ss << "</div>\n";
    return ss.str();
}

/**
 * @brief Gera a tabela semanal formatada em HTML para um professor.
 * @param data Dados da instituição.
 * @param prof Professor cujos horários serão gerados.
 * @return Fragmento HTML com a tabela do docente.
 */
std::string ScheduleReportExporter::gerarTabelaHtmlProfessor(const model::InstitutionData& data,
                                                            const model::Professor& prof) {
    std::ostringstream ss;
    ss << "<div class='section-card'>\n";
    ss << "  <h2>Professor: " << prof.nome() << " (" << prof.id() << ")</h2>\n";
    ss << "  <p class='badge-info'>Pesos por Dia: ";
    for (auto d : model::allDays()) {
        ss << model::dayToString(d) << "=" << prof.pesoDia(d) << " ";
    }
    ss << "</p>\n";

    ss << "  <table class='timetable'>\n";
    ss << "    <thead><tr><th>Horário</th>";
    for (auto d : model::allDays()) {
        ss << "<th>" << model::dayToLongString(d) << "</th>";
    }
    ss << "</tr></thead>\n";
    ss << "    <tbody>\n";

    std::set<std::pair<int, int>> horarios_set;
    for (auto d : model::allDays()) {
        auto aulas = data.grade().aulasPorProfessor(prof.id(), d);
        for (const auto& a : aulas) {
            horarios_set.insert({a.intervalo.start_min, a.intervalo.end_min});
        }
    }

    for (const auto& [s, e] : horarios_set) {
        model::TimeInterval row_inter(s, e);
        ss << "    <tr>\n";
        ss << "      <td class='time-col'>" << row_inter.toString() << "</td>\n";
        for (auto d : model::allDays()) {
            auto aulas = data.grade().aulasPorProfessor(prof.id(), d);
            std::string cell_content = "-";
            for (const auto& a : aulas) {
                if (a.intervalo.start_min == s && a.intervalo.end_min == e) {
                    std::string turma_names;
                    for (size_t i = 0; i < a.turmas.size(); ++i) {
                        if (i > 0) turma_names += ", ";
                        const auto* t = data.findTurma(a.turmas[i]);
                        turma_names += (t ? t->nome() : a.turmas[i]);
                    }
                    std::string sala_names;
                    for (size_t i = 0; i < a.salas.size(); ++i) {
                        if (i > 0) sala_names += ", ";
                        const auto* sl = data.findSala(a.salas[i]);
                        sala_names += (sl ? sl->nome() : a.salas[i]);
                    }

                    std::string badge_cls = (a.regime == model::RegimeDisciplina::ANUAL) ? "badge-anual" : (a.semestre_alocado == 2 ? "badge-sem2" : "badge-sem1");
                    std::string badge_str = "<span class='badge-regime " + badge_cls + "'>" + a.regimeSemestreFormatado() + "</span>";

                    cell_content = "<div class='lesson-card " + a.tipo + "'>" +
                                   "<b>" + a.disciplina_nome + "</b> " + badge_str + "<br>" +
                                   "<span class='turma-tag'>Turma: " + turma_names + "</span><br>" +
                                   "<span class='room-tag'>Sala: " + sala_names + "</span>" +
                                   "</div>";
                    break;
                }
            }
            ss << "      <td>" << cell_content << "</td>\n";
        }
        ss << "    </tr>\n";
    }
    ss << "    </tbody>\n";
    ss << "  </table>\n";
    ss << "</div>\n";
    return ss.str();
}

/**
 * @brief Gera a tabela semanal formatada em HTML para uma sala ou laboratório.
 * @param data Dados da instituição.
 * @param sala Sala cujos horários serão gerados.
 * @return Fragmento HTML com a tabela da sala.
 */
std::string ScheduleReportExporter::gerarTabelaHtmlSala(const model::InstitutionData& data,
                                                       const model::Room& sala) {
    std::ostringstream ss;
    ss << "<div class='section-card'>\n";
    ss << "  <h2>Sala: " << sala.nome() << " (" << sala.id() << ")</h2>\n";
    ss << "  <p class='badge-info'>Tipo: " << sala.tipo() << " | Capacidade: " << sala.capacidade() << " alunos</p>\n";

    ss << "  <table class='timetable'>\n";
    ss << "    <thead><tr><th>Horário</th>";
    for (auto d : model::allDays()) {
        ss << "<th>" << model::dayToLongString(d) << "</th>";
    }
    ss << "</tr></thead>\n";
    ss << "    <tbody>\n";

    std::set<std::pair<int, int>> horarios_set;
    for (auto d : model::allDays()) {
        auto aulas = data.grade().aulasPorSala(sala.id(), d);
        for (const auto& a : aulas) {
            horarios_set.insert({a.intervalo.start_min, a.intervalo.end_min});
        }
    }

    for (const auto& [s, e] : horarios_set) {
        model::TimeInterval row_inter(s, e);
        ss << "    <tr>\n";
        ss << "      <td class='time-col'>" << row_inter.toString() << "</td>\n";
        for (auto d : model::allDays()) {
            auto aulas = data.grade().aulasPorSala(sala.id(), d);
            std::string cell_content = "-";
            for (const auto& a : aulas) {
                if (a.intervalo.start_min == s && a.intervalo.end_min == e) {
                    std::string turma_names;
                    for (size_t i = 0; i < a.turmas.size(); ++i) {
                        if (i > 0) turma_names += ", ";
                        const auto* t = data.findTurma(a.turmas[i]);
                        turma_names += (t ? t->nome() : a.turmas[i]);
                    }
                    std::string prof_names;
                    for (size_t i = 0; i < a.professores.size(); ++i) {
                        if (i > 0) prof_names += ", ";
                        const auto* p = data.findProfessor(a.professores[i]);
                        prof_names += (p ? p->nome() : a.professores[i]);
                    }

                    std::string badge_cls = (a.regime == model::RegimeDisciplina::ANUAL) ? "badge-anual" : (a.semestre_alocado == 2 ? "badge-sem2" : "badge-sem1");
                    std::string badge_str = "<span class='badge-regime " + badge_cls + "'>" + a.regimeSemestreFormatado() + "</span>";

                    cell_content = "<div class='lesson-card " + a.tipo + "'>" +
                                   "<b>" + a.disciplina_nome + "</b> " + badge_str + "<br>" +
                                   "<span class='turma-tag'>Turma: " + turma_names + "</span><br>" +
                                   "<span class='prof-tag'>" + prof_names + "</span>" +
                                   "</div>";
                    break;
                }
            }
            ss << "      <td>" << cell_content << "</td>\n";
        }
        ss << "    </tr>\n";
    }
    ss << "    </tbody>\n";
    ss << "  </table>\n";
    ss << "</div>\n";
    return ss.str();
}

/**
 * @brief Exporta um relatório completo e responsivo da grade horária em HTML com visual moderno.
 * @param filepath Caminho de saída do arquivo HTML.
 * @param data Dados da instituição contendo a grade e estatísticas de resolução.
 * @param err Mensagem de erro caso a criação falhe.
 * @return true se o relatório foi gravado; false em caso de falha.
 */
bool ScheduleReportExporter::exportarHtml(const std::string& filepath,
                                          const model::InstitutionData& data,
                                          std::string& err) {
    try {
        std::ofstream ofs(filepath);
        if (!ofs.is_open()) {
            err = "Não foi possível abrir o arquivo HTML para escrita: " + filepath;
            return false;
        }

        const auto& stats = data.grade().estatisticas();

        ofs << "<!DOCTYPE html>\n"
            << "<html lang='pt-BR'>\n"
            << "<head>\n"
            << "  <meta charset='UTF-8'>\n"
            << "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>\n"
            << "  <title>Relatório da Grade de Horários - CP-SAT</title>\n"
            << "  <style>\n"
            << "    :root {\n"
            << "      --primary: #2563eb;\n"
            << "      --primary-dark: #1d4ed8;\n"
            << "      --bg: #0f172a;\n"
            << "      --surface: #1e293b;\n"
            << "      --surface-light: #334155;\n"
            << "      --text: #f8fafc;\n"
            << "      --text-muted: #94a3b8;\n"
            << "      --border: #475569;\n"
            << "      --accent: #38bdf8;\n"
            << "      --success: #10b981;\n"
            << "      --warning: #f59e0b;\n"
            << "    }\n"
            << "    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 24px; line-height: 1.5; }\n"
            << "    h1, h2, h3 { color: #ffffff; margin-top: 0; }\n"
            << "    .header-panel { background: linear-gradient(135deg, #1e293b 0%, #0f172a 100%); border: 1px solid var(--border); border-radius: 12px; padding: 24px; margin-bottom: 24px; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.1); }\n"
            << "    .kpi-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 16px; margin-top: 16px; }\n"
            << "    .kpi-card { background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 14px; text-align: center; }\n"
            << "    .kpi-val { font-size: 24px; font-weight: 700; color: var(--accent); }\n"
            << "    .kpi-lbl { font-size: 13px; color: var(--text-muted); margin-top: 4px; }\n"
            << "    .section-card { background: var(--surface); border: 1px solid var(--border); border-radius: 12px; padding: 20px; margin-bottom: 28px; }\n"
            << "    .badge-info { display: inline-block; background: var(--surface-light); color: var(--accent); padding: 4px 10px; border-radius: 6px; font-size: 13px; margin-bottom: 16px; }\n"
            << "    .timetable { width: 100%; border-collapse: collapse; margin-top: 10px; }\n"
            << "    .timetable th, .timetable td { border: 1px solid var(--border); padding: 10px 12px; text-align: center; font-size: 13px; }\n"
            << "    .timetable th { background: #1e293b; color: var(--accent); font-weight: 600; text-transform: uppercase; font-size: 12px; letter-spacing: 0.5px; }\n"
            << "    .time-col { font-weight: bold; color: var(--text-muted); width: 120px; background: #162032; }\n"
            << "    .recess-row { background: rgba(245, 158, 11, 0.1); }\n"
            << "    .recess-cell { color: var(--warning); font-weight: 600; font-size: 12px; letter-spacing: 1px; }\n"
            << "    .lesson-card { background: #2a3a52; border-left: 4px solid var(--primary); border-radius: 6px; padding: 8px; text-align: left; position: relative; }\n"
            << "    .lesson-card.LABORATORIO { border-left-color: #ec4899; }\n"
            << "    .lesson-card.ESPECIAL { border-left-color: #8b5cf6; }\n"
            << "    .prof-tag { color: var(--accent); font-size: 11px; }\n"
            << "    .turma-tag { color: #34d399; font-size: 11px; }\n"
            << "    .room-tag { color: var(--text-muted); font-size: 11px; }\n"
            << "    .badge-regime { display: inline-block; font-size: 10px; font-weight: bold; border-radius: 4px; padding: 1px 5px; margin-left: 4px; vertical-align: middle; }\n"
            << "    .badge-anual { background: rgba(56, 189, 248, 0.2); color: #38bdf8; border: 1px solid rgba(56, 189, 248, 0.4); }\n"
            << "    .badge-sem1 { background: rgba(245, 158, 11, 0.2); color: #f59e0b; border: 1px solid rgba(245, 158, 11, 0.4); }\n"
            << "    .badge-sem2 { background: rgba(16, 185, 129, 0.2); color: #10b981; border: 1px solid rgba(16, 185, 129, 0.4); }\n"
            << "  </style>\n"
            << "</head>\n"
            << "<body>\n"
            << "  <div class='header-panel'>\n"
            << "    <h1>Relatório Oficial da Grade de Horários</h1>\n"
            << "    <p style='color:var(--text-muted); margin-bottom:0;'>Otimização com Google OR-Tools CP-SAT | Granularidade de 5 minutos</p>\n"
            << "    <div class='kpi-grid'>\n"
            << "      <div class='kpi-card'><div class='kpi-val'>" << stats.status << "</div><div class='kpi-lbl'>Status do Solver</div></div>\n"
            << "      <div class='kpi-card'><div class='kpi-val'>" << std::fixed << std::setprecision(1) << stats.tempo_execucao_ms << " ms</div><div class='kpi-lbl'>Tempo de Execução</div></div>\n"
            << "      <div class='kpi-card'><div class='kpi-val'>" << stats.total_aulas_alocadas << " / " << stats.total_aulas_esperadas << "</div><div class='kpi-lbl'>Aulas Alocadas</div></div>\n"
            << "      <div class='kpi-card'><div class='kpi-val'>" << stats.penalidade_janelas_J << " min</div><div class='kpi-lbl'>Janelas Professores (J)</div></div>\n"
            << "      <div class='kpi-card'><div class='kpi-val'>" << stats.penalidade_dias_D << "</div><div class='kpi-lbl'>Dias Trabalhados (D)</div></div>\n"
            << "      <div class='kpi-card'><div class='kpi-val'>" << stats.penalidade_preferencia_dias_W << "</div><div class='kpi-lbl'>Penalidade Preferência Dias (W)</div></div>\n"
            << "    </div>\n"
            << "  </div>\n";

        // Tabelas por Turma
        for (const auto& turma : data.turmas()) {
            ofs << gerarTabelaHtmlTurma(data, turma);
        }

        // Tabelas por Professor
        for (const auto& prof : data.professores()) {
            ofs << gerarTabelaHtmlProfessor(data, prof);
        }

        // Tabelas por Sala
        for (const auto& sala : data.salas()) {
            ofs << gerarTabelaHtmlSala(data, sala);
        }

        ofs << "</body>\n</html>\n";
        return true;
    } catch (const std::exception& e) {
        err = e.what();
        return false;
    }
}

} // namespace scheduler::ui
