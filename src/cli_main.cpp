/**
 * @file cli_main.cpp
 * @brief Ponto de entrada headless (sem Qt) para execucao do solver via workers web (Celery).
 */

#include "model/InstitutionData.h"
#include "model/OptimizationConfig.h"
#include "serialization/JsonSerialization.h"
#include "solver/Preprocessor.h"
#include "solver/ScheduleSolver.h"

#include <atomic>
#include <csignal>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

namespace {

std::atomic<bool> g_cancelled{false};
scheduler::solver::ScheduleSolver* g_active_solver{nullptr};

void handleSignal(int /*sig*/) {
    g_cancelled.store(true);
    if (g_active_solver != nullptr) {
        g_active_solver->requestStop();
    }
}

void printHelp(const char* prog) {
    std::cout << "Uso: " << prog << " --input <in.json> [--output <out.json>] [OPCOES]\n\n"
              << "Opcoes:\n"
              << "  --input <arquivo.json>       Arquivo JSON de entrada (obrigatorio)\n"
              << "  --output <arquivo.json>      Arquivo JSON de saida com a grade gerada\n"
              << "  --progress-jsonl             Emite eventos JSON Lines de progresso em stdout\n"
              << "  --validate-only              Apenas executa Preprocessor::validarDados e sai\n"
              << "  --algorithm <ALGO>           Sobrescreve algoritmo (CP_SAT, HIGHS, SCIP, CUOPT, GREEDY_HEURISTIC, MILP)\n"
              << "  --time-limit <SEG>           Sobrescreve tempo limite em segundos\n"
              << "  --max-memory <MB>            Sobrescreve limite de memoria RAM em MB\n"
              << "  --help, -h                   Exibe esta ajuda\n";
}

} // namespace

int main(int argc, char* argv[]) {
    std::string input_path;
    std::string output_path;
    bool progress_jsonl = false;
    bool validate_only = false;
    std::string algorithm_override;
    double time_limit_override = -1.0;
    int max_memory_override = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printHelp(argv[0]);
            return 0;
        } else if (arg == "--input" && i + 1 < argc) {
            input_path = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--progress-jsonl") {
            progress_jsonl = true;
        } else if (arg == "--validate-only") {
            validate_only = true;
        } else if (arg == "--algorithm" && i + 1 < argc) {
            algorithm_override = argv[++i];
        } else if (arg == "--time-limit" && i + 1 < argc) {
            time_limit_override = std::stod(argv[++i]);
        } else if (arg == "--max-memory" && i + 1 < argc) {
            max_memory_override = std::stoi(argv[++i]);
        }
    }

    if (input_path.empty()) {
        std::cerr << "{\"type\":\"error\",\"message\":\"Parametro --input obrigatorio\"}\n";
        return 3;
    }

    scheduler::model::InstitutionData data;
    std::string err;
    if (!scheduler::serialization::JsonSerialization::carregarArquivo(input_path, data, err)) {
        std::cerr << nlohmann::json({{"type", "error"}, {"message", err}}).dump() << "\n";
        return 3;
    }

    if (!algorithm_override.empty()) {
        data.config().algoritmo = scheduler::model::stringToSolverAlgorithm(algorithm_override);
    }
    if (time_limit_override > 0.0) {
        data.config().tempo_limite_segundos = time_limit_override;
        data.config().sem_limite_tempo = false;
    }
    if (max_memory_override > 0) {
        data.config().max_memoria_mb = max_memory_override;
        data.config().sem_limite_memoria = false;
    }

    if (validate_only) {
        auto val = scheduler::solver::Preprocessor::validarDados(data);
        nlohmann::json out = {
            {"is_valid", val.is_valid},
            {"errors", val.errors},
            {"warnings", val.warnings}
        };
        std::cout << out.dump() << std::endl;
        return val.is_valid ? 0 : 3;
    }

    scheduler::solver::ScheduleSolver solver;
    g_active_solver = &solver;
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    auto progress_cb = [&](const std::string& msg, double ratio) {
        if (progress_jsonl) {
            nlohmann::json ev = {
                {"type", "progress"},
                {"ratio", ratio},
                {"message", msg}
            };
            std::cout << ev.dump() << "\n" << std::flush;
        }
    };

    auto tt = solver.solve(data, progress_cb);
    g_active_solver = nullptr;

    data.setGrade(tt);
    const auto& stats = tt.estatisticas();

    if (!output_path.empty()) {
        if (!scheduler::serialization::JsonSerialization::salvarArquivo(output_path, data, err)) {
            std::cerr << nlohmann::json({{"type", "error"}, {"message", err}}).dump() << "\n";
            return 3;
        }
    }

    if (progress_jsonl) {
        nlohmann::json done_ev = {
            {"type", "done"},
            {"status", stats.status},
            {"elapsed_ms", stats.tempo_execucao_ms},
            {"objective", stats.valor_objetivo},
            {"allocated", stats.total_aulas_alocadas},
            {"expected", stats.total_aulas_esperadas},
            {"diagnostics", tt.mensagensDiagnostico()}
        };
        std::cout << done_ev.dump() << "\n" << std::flush;
    }

    if (g_cancelled.load()) {
        return 130;
    }
    if (stats.status == "OPTIMAL" || stats.status == "FEASIBLE") {
        return 0;
    }
    return 2;
}
