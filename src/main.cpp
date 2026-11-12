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
#include "ui/ScheduleReportExporter.h"
#include "serialization/JsonSerialization.h"
#include "solver/ScheduleSolver.h"
#include <QApplication>
#include <QPixmap>
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
                  << "  --greedy                     Utiliza Heurística Gulosa\n"
                  << "  --cpsat                      Utiliza CP-SAT (Google OR-Tools)\n"
                  << "  --scip                       Utiliza SCIP (Solving Constraint Integer Programs)\n"
                  << "  --highs                      Utiliza HiGHS (High Performance Optimization Software)\n"
                  << "  --milp                       Utiliza MILP (HiGHS/SCIP)\n"
                  << "  --max-memory <MB>            Define teto de consumo de memória RAM em MB (ex: 2048)\n"
                  << "  --unlimited-memory           Executa sem limite imposto de memória\n"
                  << "  --time-limit <SEG>           Define tempo limite de resolução em segundos\n"
                  << "  --warmstart-chain            Testa encadeamento guloso -> CP-SAT\n"
                  << "  --render-screenshots         Gera capturas de tela dos diálogos da interface\n"
                  << "  --help, -h                   Exibe esta mensagem de ajuda\n";
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

        scheduler::model::InstitutionData data;
        std::string filename;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--max-memory" || arg == "--time-limit") {
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
            } else if (arg.rfind("--time-limit=", 0) == 0) {
                data.config().tempo_limite_segundos = std::stod(arg.substr(13));
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

        data.setGrade(tt);
        const auto& stats = tt.estatisticas();
        std::cout << "\n------------------ RESULTADOS DA OTIMIZAÇÃO ------------------" << std::endl;
        std::cout << "Status: " << stats.status << std::endl;
        std::cout << "Tempo de resolução: " << stats.tempo_execucao_ms << " ms" << std::endl;
        std::cout << "Valor da função objetivo (Z): " << stats.valor_objetivo << std::endl;
        std::cout << "Aulas alocadas: " << stats.total_aulas_alocadas << " de " << stats.total_aulas_esperadas << std::endl;
        std::cout << "Penalidade Janelas Professores (J): " << stats.penalidade_janelas_J << " min" << std::endl;
        std::cout << "Penalidade Dias Trabalhados (D): " << stats.penalidade_dias_D << std::endl;
        std::cout << "Penalidade Preferência Semanal Dias (W): " << stats.penalidade_preferencia_dias_W << std::endl;
        std::cout << "Penalidade Lacunas Turmas (JG): " << stats.penalidade_lacunas_turma_JG << " min" << std::endl;
        for (const auto& msg : tt.mensagensDiagnostico()) {
            std::cout << "  " << msg << std::endl;
        }

        std::string err;
        if (scheduler::ui::ScheduleReportExporter::exportarHtml("grade_horarios.html", data, err)) {
            std::cout << "\n[OK] Relatório exportado com sucesso: grade_horarios.html" << std::endl;
        }
        if (scheduler::ui::ScheduleReportExporter::exportarCsv("grade_horarios.csv", data, err)) {
            std::cout << "[OK] Dados exportados com sucesso: grade_horarios.csv" << std::endl;
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
