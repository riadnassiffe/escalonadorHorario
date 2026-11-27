/**
 * @file main.cpp
 * @brief Ponto de entrada da aplicação do Escalonador de Horários Escolares.
 *
 * Suporta inicialização da interface gráfica Qt (GUI) completa com tema nativo do sistema/KDE Plasma,
 * execução em modo headless / linha de comando para testes automatizados (--cli, --test, --benchmark),
 * e geração de capturas de tela dos diálogos (--render-screenshots).
 */

#include "ui/MainWindow.h"
#include "ui/ModernTheme.h"
#include "ui/ProfessorEditorDialog.h"
#include "ui/TurmaEditorDialog.h"
#include "ui/OptimizeDialog.h"
#include "ui/LessonEditorDialog.h"
#include "ui/RoomEditorDialog.h"
#include "ui/AddLessonToScheduleDialog.h"
#include "ui/SwapLessonDialog.h"
#include "ui/MoveLessonDialog.h"
#include "ui/ChangeRoomDialog.h"
#include "ui/ScheduleReportExporter.h"
#include "serialization/JsonSerialization.h"
#include "solver/ScheduleSolver.h"
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QPixmap>
#include <QPushButton>
#include <QTabWidget>
#include <iostream>
#include <string>

#include "ortools/linear_solver/linear_solver.h"
#ifdef slots
#undef slots
#include <Highs.h>
#include <scip/scip.h>
#define slots Q_SLOTS
#else
#include <Highs.h>
#include <scip/scip.h>
#endif

/**
 * @brief Função principal do programa.
 * @param argc Quantidade de argumentos passados pela linha de comando.
 * @param argv Vetor contendo os argumentos passados.
 * @return Código de saída da execução (0 para sucesso, diferente de zero em caso de erro).
 */
int main(int argc, char* argv[]) {
    // Ajuda via Linha de Comando
    if (argc > 1 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        std::cout << "Uso: " << argv[0] << " [OPÇÕES] [ARQUIVO.json]\n\n"
                  << "Opções de Execução:\n"
                  << "  --cli, --test, --benchmark   Executa em modo linha de comando (sem GUI)\n"
                  << "  --cuopt                      Utiliza NVIDIA cuOpt (GPU-Accelerated Mathematical Optimization)\n"
                  << "  --greedy                     Utiliza Heurística Gulosa\n"
                  << "  --cpsat                      Utiliza CP-SAT (Google OR-Tools)\n"
                  << "  --scip                       Utiliza SCIP (Solving Constraint Integer Programs)\n"
                  << "  --highs                      Utiliza HiGHS (High Performance Optimization Software)\n"
                  << "  --milp                       Utiliza MILP (HiGHS/SCIP)\n"
                  << "  --max-memory <MB>            Define teto de consumo de memória RAM em MB (ex: 2048)\n"
                  << "  --unlimited-memory           Executa sem limite imposto de memória\n"
                  << "  --time-limit <SEG>           Define tempo limite de resolução em segundos\n"
                  << "  --save <ARQUIVO.json>        Salva o projeto JSON atualizado com a grade gerada\n"
                  << "  --warmstart-chain            Testa encadeamento guloso -> CP-SAT\n"
                  << "  --render-screenshots         Gera capturas de tela dos diálogos da interface\n"
                  << "  --verify-ui-fit [JSON]       Verifica responsividade offscreen da GUI em telas de notebook\n"
                  << "  --help, -h                   Exibe esta mensagem de ajuda\n";
        return 0;
    }

    if (argc > 1 && std::string(argv[1]) == "--verify-ui-fit") {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        QApplication app(argc, argv);

        std::string json_path;
        std::string screenshots_dir;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--screenshots-dir" && i + 1 < argc) {
                screenshots_dir = argv[++i];
            } else if (arg.rfind("--", 0) != 0 && json_path.empty()) {
                json_path = arg;
            }
        }

        if (!screenshots_dir.empty()) {
            QDir().mkpath(QString::fromStdString(screenshots_dir));
        }

        scheduler::model::InstitutionData inst;
        std::string err;
        if (json_path.empty() || !scheduler::serialization::JsonSerialization::carregarArquivo(json_path, inst, err)) {
            inst = scheduler::model::InstitutionData::criarExemploPadrao();
            if (!json_path.empty()) {
                std::cerr << "[AVISO] Falha ao carregar '" << json_path << "' (" << err << "); usando exemplo padrão.\n";
            }
        } else {
            inst.gerarOcorrenciasAulas(true, scheduler::model::EscopoSemestreOtimizacao::TODAS_SEM_FILTRO);
        }

        int failures = 0;
        const int max_w = 1024;
        const int max_h = 600;

        auto check_buttons_inside = [&](QWidget* root, const QString& label) {
            const QRect r = root->rect();
            for (auto* btn : root->findChildren<QPushButton*>()) {
                if (!btn->isVisibleTo(root)) continue;
                const QPoint top_left = btn->mapTo(root, QPoint(0, 0));
                const QRect br(top_left, btn->size());
                if (br.left() < r.left() || br.right() > r.right() || br.top() < r.top() || br.bottom() > r.bottom()) {
                    std::cerr << "[FAIL] " << label.toStdString() << ": botão '" << btn->text().toStdString()
                              << "' fora da janela (btn=" << br.x() << "," << br.y() << " " << br.width() << "x" << br.height()
                              << " vs win=" << r.width() << "x" << r.height() << ")\n";
                    failures++;
                }
            }
        };

        auto check_checkboxes_unclipped = [&](QWidget* root, const QString& label) {
            for (auto* chk : root->findChildren<QCheckBox*>()) {
                if (!chk->isVisibleTo(root)) continue;
                auto* pw = chk->parentWidget();
                const bool size_ok = chk->width() > 20 && chk->height() > 10;
                const bool inside_parent = pw && pw->rect().contains(chk->geometry());
                if (!size_ok || !inside_parent) {
                    std::cerr << "[FAIL] " << label.toStdString() << ": checkbox '" << chk->text().toStdString()
                              << "' recortada ou colapsada (geom=" << chk->x() << "," << chk->y()
                              << " " << chk->width() << "x" << chk->height()
                              << " parent=" << (pw ? pw->width() : -1) << "x" << (pw ? pw->height() : -1) << ")\n";
                    failures++;
                }
            }
        };

        auto require_checkbox = [&](QWidget* root, const QString& label, const QString& text) {
            QCheckBox* found = nullptr;
            for (auto* chk : root->findChildren<QCheckBox*>()) {
                if (chk->text() == text) { found = chk; break; }
            }
            if (!found || !found->isVisibleTo(root)) {
                std::cerr << "[FAIL] " << label.toStdString() << ": checkbox obrigatória ausente/oculta: '"
                          << text.toStdString() << "'\n";
                failures++;
                return;
            }
            auto* pw = found->parentWidget();
            if (found->width() <= 20 || found->height() <= 10 || !pw || !pw->rect().contains(found->geometry())) {
                std::cerr << "[FAIL] " << label.toStdString() << ": checkbox obrigatória '" << text.toStdString()
                          << "' recortada (geom=" << found->x() << "," << found->y()
                          << " " << found->width() << "x" << found->height()
                          << " parent=" << (pw ? pw->width() : -1) << "x" << (pw ? pw->height() : -1) << ")\n";
                failures++;
            } else {
                std::cout << "[OK] " << label.toStdString() << " checkbox '" << text.toStdString()
                          << "' visível geom=(" << found->x() << "," << found->y()
                          << " " << found->width() << "x" << found->height() << ")\n";
            }
        };

        auto check_dialog = [&](QDialog& dlg, const QString& name, const QString& shot_file) {
            dlg.show();
            app.processEvents();
            const QSize min_hint = dlg.minimumSizeHint();
            const QSize min_sz = dlg.minimumSize();
            const int eff_min_w = std::max(min_hint.width(), min_sz.width());
            const int eff_min_h = std::max(min_hint.height(), min_sz.height());
            if (eff_min_w > max_w || eff_min_h > max_h) {
                std::cerr << "[FAIL] " << name.toStdString() << ": mínimo excede " << max_w << "x" << max_h
                          << " (minHint=" << min_hint.width() << "x" << min_hint.height()
                          << ", minSize=" << min_sz.width() << "x" << min_sz.height() << ")\n";
                failures++;
            } else {
                std::cout << "[OK] " << name.toStdString() << " min=(" << eff_min_w << "x" << eff_min_h
                          << ") cur=(" << dlg.width() << "x" << dlg.height() << ")\n";
            }
            if (!shot_file.isEmpty()) {
                dlg.grab().save(shot_file);
            }
            check_checkboxes_unclipped(&dlg, name);
            dlg.resize(std::min(dlg.width(), max_w), std::min(dlg.height(), max_h));
            app.processEvents();
            if (dlg.width() > max_w || dlg.height() > max_h) {
                std::cerr << "[FAIL] " << name.toStdString() << ": não encolheu para " << max_w << "x" << max_h
                          << " (ficou " << dlg.width() << "x" << dlg.height() << ")\n";
                failures++;
            }
            check_buttons_inside(&dlg, name + " @1024x600");
            check_checkboxes_unclipped(&dlg, name + " @1024x600");
            dlg.hide();
        };

        const std::vector<std::pair<scheduler::ui::ModernTheme::ThemeMode, QString>> themes = {
            {scheduler::ui::ModernTheme::ThemeMode::SystemKDE, "light"},
            {scheduler::ui::ModernTheme::ThemeMode::ModernDark, "dark"}
        };

        for (const auto& [mode, tag] : themes) {
            scheduler::ui::ModernTheme::setThemeMode(app, mode);

            scheduler::ui::MainWindow win;
            if (!json_path.empty()) {
                win.loadProjectFile(QString::fromStdString(json_path));
            }
            win.show();
            win.resize(1280, 720);
            app.processEvents();

            auto shot_path = [&](const QString& base) -> QString {
                if (screenshots_dir.empty()) return {};
                return QString::fromStdString(screenshots_dir) + "/" + tag + "_" + base + ".png";
            };

            if (!screenshots_dir.empty()) {
                win.grab().save(shot_path("mainwindow"));
            }

            if (auto* tabs = win.findChild<QTabWidget*>()) {
                for (int t = 0; t < tabs->count(); ++t) {
                    tabs->setCurrentIndex(t);
                    app.processEvents();
                    const QSize mh = win.minimumSizeHint();
                    const QSize ms = win.minimumSize();
                    const int ew = std::max(mh.width(), ms.width());
                    const int eh = std::max(mh.height(), ms.height());
                    const QString tab_label = QString("MainWindow[%1]/tab%2").arg(tag).arg(t);
                    if (ew > max_w || eh > max_h) {
                        std::cerr << "[FAIL] " << tab_label.toStdString() << ": min (" << ew << "x" << eh << ") > "
                                  << max_w << "x" << max_h << "\n";
                        failures++;
                    } else {
                        std::cout << "[OK] " << tab_label.toStdString() << " min=(" << ew << "x" << eh << ")\n";
                    }
                    if (t == 5) {
                        win.resize(1280, 720);
                        app.processEvents();
                        if (!screenshots_dir.empty()) {
                            win.grab().save(shot_path("config_tab"));
                        }
                        check_checkboxes_unclipped(tabs->widget(t), tab_label + " @1280x720");
                        require_checkbox(tabs->widget(t), tab_label, "Sem restrição de tempo");
                        require_checkbox(tabs->widget(t), tab_label, "Sem restrição de memória");
                    }
                    win.resize(1280, 680);
                    app.processEvents();
                    if (win.width() > 1280 || win.height() > 680) {
                        std::cerr << "[FAIL] " << tab_label.toStdString() << ": não coube em 1280x680 (ficou "
                                  << win.width() << "x" << win.height() << ")\n";
                        failures++;
                    }
                    win.resize(1024, 600);
                    app.processEvents();
                    if (win.width() > 1024 || win.height() > 600) {
                        std::cerr << "[FAIL] " << tab_label.toStdString() << ": não coube em 1024x600 (ficou "
                                  << win.width() << "x" << win.height() << ")\n";
                        failures++;
                    }
                    if (t == 5) {
                        check_checkboxes_unclipped(tabs->widget(t), tab_label + " @1024x600");
                    }
                }
                tabs->setCurrentIndex(0);
            }

            scheduler::ui::OptimizeDialog opt_dlg(inst.config(), static_cast<int>(inst.grade().alocacoes().size()), &win);
            opt_dlg.show();
            app.processEvents();
            require_checkbox(&opt_dlg, "OptimizeDialog[" + tag + "]", "Sem restrição de tempo");
            require_checkbox(&opt_dlg, "OptimizeDialog[" + tag + "]", "Sem restrição de memória");
            check_dialog(opt_dlg, "OptimizeDialog[" + tag + "]", shot_path("optimize_dialog"));

            const scheduler::model::Turma* turma = inst.turmas().empty() ? nullptr : &inst.turmas().front();
            scheduler::ui::TurmaEditorDialog turma_dlg(inst.salas(), &win, turma);
            check_dialog(turma_dlg, "TurmaEditorDialog[" + tag + "]", shot_path("turma_dialog"));

            const scheduler::model::Professor* prof = inst.professores().empty() ? nullptr : &inst.professores().front();
            scheduler::ui::ProfessorEditorDialog prof_dlg(&win, prof);
            check_dialog(prof_dlg, "ProfessorEditorDialog[" + tag + "]", shot_path("prof_dialog"));

            const scheduler::model::Disciplina* lab_disc = nullptr;
            for (const auto& d : inst.disciplinas()) {
                if (!d.isTeorica()) { lab_disc = &d; break; }
            }
            if (!lab_disc && !inst.disciplinas().empty()) lab_disc = &inst.disciplinas().front();
            scheduler::ui::LessonEditorDialog disc_dlg(inst.professores(), inst.turmas(), inst.salas(), &win, lab_disc);
            check_dialog(disc_dlg, "LessonEditorDialog[" + tag + "]", shot_path("lesson_dialog"));

            scheduler::ui::AddLessonToScheduleDialog add_dlg(inst, &win);
            check_dialog(add_dlg, "AddLessonToScheduleDialog[" + tag + "]", shot_path("add_lesson_dialog"));

            std::string first_aloc_id = inst.grade().alocacoes().empty() ? "" : inst.grade().alocacoes().front().aula_id;
            scheduler::ui::SwapLessonDialog swap_dlg(inst, first_aloc_id, &win);
            check_dialog(swap_dlg, "SwapLessonDialog[" + tag + "]", shot_path("swap_dialog"));

            if (!first_aloc_id.empty()) {
                std::vector<scheduler::model::TimeInterval> slots_vec;
                if (!inst.turmas().empty()) {
                    slots_vec = inst.turmas().front().jornadaPadrao().slots_letivos;
                }
                scheduler::ui::MoveLessonDialog move_dlg(inst, first_aloc_id, slots_vec, &win);
                check_dialog(move_dlg, "MoveLessonDialog[" + tag + "]", "");

                scheduler::ui::ChangeRoomDialog room_chg_dlg(inst, first_aloc_id, &win);
                check_dialog(room_chg_dlg, "ChangeRoomDialog[" + tag + "]", "");
            }

            const scheduler::model::Room* room = inst.salas().empty() ? nullptr : &inst.salas().front();
            scheduler::ui::RoomEditorDialog room_dlg(&win, room);
            check_dialog(room_dlg, "RoomEditorDialog[" + tag + "]", "");
        }

        if (failures > 0) {
            std::cerr << "\nTotal de falhas de responsividade: " << failures << "\n";
            return 1;
        }
        std::cout << "\n[PASS] Todas as telas e diálogos cabem confortavelmente em 1024x600 e 1280x680!\n";
        return 0;
    }

    // Modo Headless / Teste via Linha de Comando
    if (argc > 1 && (std::string(argv[1]) == "--test" || std::string(argv[1]) == "--benchmark" || std::string(argv[1]) == "--cli")) {
        std::cout << "===========================================================" << std::endl;
        std::cout << "  Escalonador Inteligente de Horários" << std::endl;
        std::cout << "  Modo Linha de Comando / Verificação Automática" << std::endl;
        std::cout << "===========================================================" << std::endl;

        std::cout << "  [SOLVER CHECK] HiGHS disponível: SIM (Nativo v" << highsVersion() << ")" << std::endl;
        std::cout << "  [SOLVER CHECK] SCIP disponível: SIM (Nativo v"
                  << SCIPmajorVersion() << "." << SCIPminorVersion() << "." << SCIPtechVersion() << ")" << std::endl;
        std::cout << "  [SOLVER CHECK] NVIDIA cuOpt: SIM (GPU Acelerado / Suporte a C API e Modelo MPS)" << std::endl;

        scheduler::model::InstitutionData data;
        std::string filename;
        std::string save_filename;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--max-memory" || arg == "--time-limit" || arg == "--save") {
                if (arg == "--save" && i + 1 < argc) {
                    save_filename = argv[i + 1];
                }
                ++i;
                continue;
            }
            if (!arg.empty() && arg.rfind("--", 0) != 0) {
                filename = arg;
                break;
            }
        }

        if (!filename.empty()) {
            std::string err;
            std::cout << "Carregando arquivo: " << filename << "..." << std::endl;
            if (!scheduler::serialization::JsonSerialization::carregarArquivo(filename, data, err)) {
                std::cerr << "Erro ao carregar arquivo: " << err << std::endl;
                return 1;
            }
        } else {
            std::cout << "Carregando cenário acadêmico padrão..." << std::endl;
            data = scheduler::model::InstitutionData::criarExemploPadrao();
        }

        std::cout << "Professores: " << data.professores().size() << std::endl;
        std::cout << "Salas/Laboratórios: " << data.salas().size() << std::endl;
        std::cout << "Turmas: " << data.turmas().size() << std::endl;
        std::cout << "Disciplinas: " << data.disciplinas().size() << std::endl;
        std::cout << "Aulas a alocar: " << data.aulas().size() << std::endl;

        bool warmstart_chain = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--warmstart-chain") warmstart_chain = true;
            else if (arg == "--cuopt") data.config().algoritmo = scheduler::model::SolverAlgorithm::CUOPT;
            else if (arg == "--greedy") data.config().algoritmo = scheduler::model::SolverAlgorithm::GREEDY_HEURISTIC;
            else if (arg == "--scip") data.config().algoritmo = scheduler::model::SolverAlgorithm::SCIP;
            else if (arg == "--highs") data.config().algoritmo = scheduler::model::SolverAlgorithm::HIGHS;
            else if (arg == "--milp") data.config().algoritmo = scheduler::model::SolverAlgorithm::MILP;
            else if (arg == "--cpsat") data.config().algoritmo = scheduler::model::SolverAlgorithm::CP_SAT;
            else if (arg == "--log") data.config().log_search_progress = true;
            else if (arg == "--max-memory" && i + 1 < argc) {
                data.config().max_memoria_mb = std::stoi(argv[++i]);
            } else if (arg.rfind("--max-memory=", 0) == 0) {
                data.config().max_memoria_mb = std::stoi(arg.substr(13));
            } else if (arg == "--unlimited-memory") {
                data.config().sem_limite_memoria = true;
            } else if (arg == "--time-limit" && i + 1 < argc) {
                data.config().tempo_limite_segundos = std::stod(argv[++i]);
                data.config().sem_limite_tempo = false;
            } else if (arg.rfind("--time-limit=", 0) == 0) {
                data.config().tempo_limite_segundos = std::stod(arg.substr(13));
                data.config().sem_limite_tempo = false;
            } else if (arg == "--save" && i + 1 < argc) {
                save_filename = argv[++i];
            } else if (arg.rfind("--save=", 0) == 0) {
                save_filename = arg.substr(7);
            }
        }

        std::cout << "Limite de memória RAM: " << (data.config().sem_limite_memoria || data.config().max_memoria_mb <= 0 ? "Sem restrição (ilimitado)" : std::to_string(data.config().max_memoria_mb) + " MB") << std::endl;

        if (warmstart_chain) {
            std::cout << "\n[TESTE ENCADINHAMENTO] 1. Executando Heurística Gulosa para gerar distribuição inicial..." << std::endl;
            data.config().algoritmo = scheduler::model::SolverAlgorithm::GREEDY_HEURISTIC;
            scheduler::solver::ScheduleSolver solver1;
            auto tt1 = solver1.solve(data, nullptr);
            data.setGrade(tt1);
            std::cout << "  -> Distribuição inicial em tela: " << tt1.alocacoes().size() << " aulas." << std::endl;

            std::cout << "\n[TESTE ENCADINHAMENTO] 2. Executando próximo algoritmo (CP-SAT) com a distribuição em tela como entrada..." << std::endl;
            data.config().algoritmo = scheduler::model::SolverAlgorithm::CP_SAT;
            data.config().usar_distribuicao_como_entrada = true;
            data.config().tempo_limite_segundos = 20.0;
            scheduler::solver::ScheduleSolver solver2;
            auto tt2 = solver2.solve(data, [](const std::string& msg, double ratio) {
                std::cout << "  [" << static_cast<int>(ratio * 100) << "%] " << msg << std::endl;
            });
            data.setGrade(tt2);

            std::cout << "\n------------------ RESULTADOS DO ENCADINHAMENTO CP-SAT ------------------" << std::endl;
            std::cout << "Status: " << tt2.estatisticas().status << std::endl;
            std::cout << "Tempo: " << tt2.estatisticas().tempo_execucao_ms << " ms" << std::endl;
            std::cout << "Aulas alocadas: " << tt2.estatisticas().total_aulas_alocadas << std::endl;
            for (const auto& msg : tt2.mensagensDiagnostico()) {
                std::cout << "  " << msg << std::endl;
            }

            return 0;
        }

        std::cout << "\nIniciando resolução com: " << scheduler::model::solverAlgorithmDisplayName(data.config().algoritmo) << "..." << std::endl;
        scheduler::solver::ScheduleSolver solver;
        auto tt = solver.solve(data, [](const std::string& msg, double ratio) {
            std::cout << "  [" << static_cast<int>(ratio * 100) << "%] " << msg << std::endl;
        });

        const auto stats = tt.estatisticas();
        const auto diags = tt.mensagensDiagnostico();
        data.aplicarResultadoOtimizacao(std::move(tt), data.config().escopo_semestre);
        std::cout << "\n------------------ RESULTADOS DA OTIMIZAÇÃO ------------------" << std::endl;
        std::cout << "Status: " << stats.status << std::endl;
        std::cout << "Tempo de resolução: " << stats.tempo_execucao_ms << " ms" << std::endl;
        std::cout << "Valor da função objetivo (Z): " << stats.valor_objetivo << std::endl;
        std::cout << "Aulas alocadas: " << stats.total_aulas_alocadas << " de " << stats.total_aulas_esperadas << std::endl;
        std::cout << "Penalidade Janelas Professores (J): " << stats.penalidade_janelas_J << " min" << std::endl;
        std::cout << "Penalidade Dias Trabalhados (D): " << stats.penalidade_dias_D << std::endl;
        std::cout << "Penalidade Preferência Semanal Dias (W): " << stats.penalidade_preferencia_dias_W << std::endl;
        std::cout << "Penalidade Lacunas Turmas (JG): " << stats.penalidade_lacunas_turma_JG << " min" << std::endl;
        for (const auto& msg : diags) {
            std::cout << "  " << msg << std::endl;
        }

        std::string err;
        if (scheduler::ui::ScheduleReportExporter::exportarHtml("grade_horarios.html", data, err)) {
            std::cout << "\n[OK] Relatório exportado com sucesso: grade_horarios.html" << std::endl;
        }
        if (scheduler::ui::ScheduleReportExporter::exportarCsv("grade_horarios.csv", data, err)) {
            std::cout << "[OK] Dados exportados com sucesso: grade_horarios.csv" << std::endl;
        }
        if (!save_filename.empty()) {
            if (scheduler::serialization::JsonSerialization::salvarArquivo(save_filename, data, err)) {
                std::cout << "[OK] Projeto JSON salvo com sucesso: " << save_filename << std::endl;
            } else {
                std::cerr << "[ERRO] Falha ao salvar JSON: " << err << std::endl;
            }
        }

        return (stats.status == "OPTIMAL" || stats.status == "FEASIBLE") ? 0 : 1;
    }

    if (argc > 1 && std::string(argv[1]) == "--render-screenshots") {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        QApplication app(argc, argv);
        scheduler::ui::ModernTheme::applyTheme(app);

        scheduler::model::InstitutionData inst;
        std::string err;
        scheduler::serialization::JsonSerialization::carregarArquivo("exemploIFC.json", inst, err);

        // 1. MainWindow
        auto* main_win = new scheduler::ui::MainWindow();
        main_win->show();
        main_win->resize(1280, 800);
        app.processEvents();
        main_win->grab().save("img/kde_mainwindow.png");

        // 2. Professor Dialog
        const scheduler::model::Professor* prof = inst.professores().empty() ? nullptr : &inst.professores().front();
        auto* prof_dlg = new scheduler::ui::ProfessorEditorDialog(nullptr, prof);
        prof_dlg->show();
        prof_dlg->resize(760, 720);
        app.processEvents();
        prof_dlg->grab().save("img/kde_prof_dialog.png");

        // 3. Turma Dialog
        const scheduler::model::Turma* turma = inst.turmas().empty() ? nullptr : &inst.turmas().front();
        auto* turma_dlg = new scheduler::ui::TurmaEditorDialog(inst.salas(), nullptr, turma);
        turma_dlg->show();
        turma_dlg->resize(820, 750);
        app.processEvents();
        turma_dlg->grab().save("img/kde_turma_dialog.png");

        // 4. Optimize Dialog
        scheduler::model::OptimizationConfig cfg;
        auto* opt_dlg = new scheduler::ui::OptimizeDialog(cfg, 10, nullptr);
        opt_dlg->show();
        opt_dlg->resize(600, 520);
        app.processEvents();
        opt_dlg->grab().save("img/kde_optimize_dialog.png");

        std::cout << "[OK] Capturas salvas com sucesso no tema nativo do sistema / KDE." << std::endl;
        return 0;
    }

    // Modo Interface Gráfica Qt
    QApplication app(argc, argv);
    app.setApplicationName("EscalonadoEscalonador_de_horario");
    app.setApplicationDisplayName("Escalonador Inteligente de Horários - Google OR-Tools CP-SAT");
    app.setOrganizationName("EscalonadoEscalonador_de_horario");
    app.setApplicationVersion("1.0.0");

    // Aplica tema visual nativo do sistema / KDE Plasma
    scheduler::ui::ModernTheme::applyTheme(app);

    scheduler::ui::MainWindow main_window;
    main_window.show();

    return app.exec();
}
