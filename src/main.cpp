#include "ui/MainWindow.h"
#include "ui/ModernTheme.h"
#include "ui/ScheduleReportExporter.h"
#include "serialization/JsonSerialization.h"
#include "solver/ScheduleSolver.h"
#include <QApplication>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    // Modo Headless / Teste via Linha de Comando
    if (argc > 1 && (std::string(argv[1]) == "--test" || std::string(argv[1]) == "--benchmark" || std::string(argv[1]) == "--cli")) {
        std::cout << "===========================================================" << std::endl;
        std::cout << "  Escalonador Inteligente de Horários - OR-Tools CP-SAT" << std::endl;
        std::cout << "  Modo Linha de Comando / Verificação Automática" << std::endl;
        std::cout << "===========================================================" << std::endl;

        scheduler::model::InstitutionData data;
        if (argc > 2) {
            std::string err;
            std::cout << "Carregando arquivo: " << argv[2] << "..." << std::endl;
            if (!scheduler::serialization::JsonSerialization::carregarArquivo(argv[2], data, err)) {
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

        std::cout << "\nIniciando resolução com Google OR-Tools CP-SAT..." << std::endl;
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
        std::cout << "Penalidade Preferências Horárias (P): " << stats.penalidade_preferencias_P << std::endl;

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
