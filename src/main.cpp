#include "ui/MainWindow.h"
#include "ui/ModernTheme.h"
#include "ui/ScheduleReportExporter.h"
#include "serialization/JsonSerialization.h"
#include "solver/ScheduleSolver.h"
#include <QApplication>
#include <iostream>
#include <string>

#include "ortools/linear_solver/linear_solver.h"

int main(int argc, char* argv[]) {
    // Modo Headless / Teste via Linha de Comando
    if (argc > 1 && (std::string(argv[1]) == "--test" || std::string(argv[1]) == "--benchmark" || std::string(argv[1]) == "--cli")) {
        std::cout << "===========================================================" << std::endl;
        std::cout << "  Escalonador Inteligente de Horários" << std::endl;
        std::cout << "  Modo Linha de Comando / Verificação Automática" << std::endl;
        std::cout << "===========================================================" << std::endl;

        std::unique_ptr<operations_research::MPSolver> highs(operations_research::MPSolver::CreateSolver("HIGHS"));
        std::cout << "  [SOLVER CHECK] HiGHS disponível: " << (highs ? "SIM" : "NAO") << std::endl;
        std::unique_ptr<operations_research::MPSolver> scip(operations_research::MPSolver::CreateSolver("SCIP"));
        std::cout << "  [SOLVER CHECK] SCIP disponível: " << (scip ? "SIM" : "NAO") << std::endl;

        scheduler::model::InstitutionData data;
        std::string filename;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
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

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--greedy") data.config().algoritmo = scheduler::model::SolverAlgorithm::GREEDY_HEURISTIC;
            else if (arg == "--genetic") data.config().algoritmo = scheduler::model::SolverAlgorithm::GENETIC_ALGORITHM;
            else if (arg == "--milp") data.config().algoritmo = scheduler::model::SolverAlgorithm::MILP;
            else if (arg == "--cpsat") data.config().algoritmo = scheduler::model::SolverAlgorithm::CP_SAT;
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

    // Modo Interface Gráfica Qt
    QApplication app(argc, argv);
    app.setApplicationName("EscalonadorGemini");
    app.setApplicationDisplayName("Escalonador Inteligente de Horários - Google OR-Tools CP-SAT");
    app.setOrganizationName("EscalonadorGemini");
    app.setApplicationVersion("1.0.0");

    // Aplica tema visual moderno
    scheduler::ui::ModernTheme::applyTheme(app);

    scheduler::ui::MainWindow main_window;
    main_window.show();

    return app.exec();
}
