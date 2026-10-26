#pragma once

/**
 * @file Timetable.h
 * @brief Definições da grade horária resultante do escalonamento (Timetable), aulas alocadas e métricas.
 */

#include "model/TimeSlot.h"
#include "model/Lesson.h"
#include <string>
#include <vector>
#include <map>

namespace scheduler::model {

/**
 * @struct AllocatedLesson
 * @brief Representa uma ocorrência de aula efetivamente posicionada e alocada na grade semanal.
 *
 * Armazena a aula com dia da semana, intervalo de horário, professores associados,
 * turmas presentes, salas reservadas e metadados de regime/semestre.
 */
struct AllocatedLesson {
    std::string aula_id;           ///< Identificador único da ocorrência de aula (ex: "T01_MAT_1").
    std::string disciplina_id;     ///< Identificador da disciplina.
    std::string disciplina_nome;   ///< Nome descritivo da disciplina.
    std::string tipo{"TEORICA"};    ///< Tipo da aula ("TEORICA", "LABORATORIO").
    DayOfWeek dia{DayOfWeek::SEG}; ///< Dia da semana em que a aula ocorre.
    TimeInterval intervalo;        ///< Intervalo de horário [start_slot, end_slot) da aula.
    std::vector<std::string> professores; ///< IDs dos professores ministrantes.
    std::vector<std::string> turmas;      ///< IDs das turmas participantes.
    std::vector<std::string> salas;       ///< IDs das salas ocupadas pela aula.
    bool is_pinned{false};         ///< Indica se a aula está fixada manualmente pelo usuário.
    RegimeDisciplina regime{RegimeDisciplina::ANUAL}; ///< Regime letivo (ANUAL ou SEMESTRAL).
    SemestreLetivo semestre{SemestreLetivo::AMBOS};   ///< Semestre de vigência da disciplina.
    int semestre_alocado{1};       ///< Semestre específico da alocação (1 para 1º sem, 2 para 2º sem).

    /**
     * @brief Retorna o horário da aula formatado como "HH:MM - HH:MM".
     * @return String contendo o intervalo formatado.
     */
    std::string horarioFormatado() const {
        return intervalo.toString();
    }

    /**
     * @brief Retorna a sigla textual do dia da semana da aula ("SEG", "TER", etc.).
     * @return String com a sigla do dia.
     */
    std::string diaString() const {
        return dayToString(dia);
    }

    /**
     * @brief Retorna uma string formatada identificando regime e semestre de alocação da aula.
     * @return String descritiva (ex: "Anual", "Semestral (1º Sem)", "Semestral (2º Sem)").
     */
    std::string regimeSemestreFormatado() const {
        if (regime == RegimeDisciplina::ANUAL) return "Anual";
        if (semestre == SemestreLetivo::SEMESTRE_1 || semestre_alocado == 1) return "Semestral (1º Sem)";
        if (semestre == SemestreLetivo::SEMESTRE_2 || semestre_alocado == 2) return "Semestral (2º Sem)";
        return "Semestral";
    }
};

/**
 * @struct TimetableStatistics
 * @brief Consolida indicadores de qualidade e métricas da solução encontrada pelo solver.
 */
struct TimetableStatistics {
    std::string status{"NAO_EXECUTADO"}; ///< Status da resolução ("OPTIMAL", "FEASIBLE", "INFEASIBLE", "UNKNOWN", etc.).
    double tempo_execucao_ms{0.0};       ///< Tempo total despendido pelo otimizador em milissegundos.
    double valor_objetivo{0.0};          ///< Valor final da função objetivo minimizada.
    int penalidade_janelas_J{0};         ///< Penalidade total decorrente de janelas ociosas de professores.
    int penalidade_dias_D{0};            ///< Penalidade por dispersão de aulas em múltiplos dias.
    int penalidade_preferencias_P{0};    ///< Penalidade por alocação fora de horários ideais.
    int penalidade_preferencia_dias_W{0};///< Penalidade associada a dias menos desejados pelos docentes.
    int penalidade_lacunas_turma_JG{0};  ///< Penalidade por horários vazios/lacunas na grade dos estudantes.
    int total_aulas_alocadas{0};         ///< Quantidade de aulas acomodadas na grade com sucesso.
    int total_aulas_esperadas{0};        ///< Total de aulas que deveriam ser alocadas.
    std::string solver_info;             ///< Informações técnicas e diagnóstico emitidos pelo solver.
};

/**
 * @class Timetable
 * @brief Representa a grade horária completa da instituição gerada ou editada.
 *
 * Contém a lista de aulas alocadas, mapeamento das salas teóricas designadas para cada turma,
 * métricas de qualidade (estatísticas) e mensagens diagnósticas.
 */
class Timetable {
public:
    /**
     * @brief Construtor padrão da classe Timetable.
     */
    Timetable() = default;

    /**
     * @brief Insere uma nova aula alocada na grade.
     * @param lesson Objeto AllocatedLesson contendo horário, dia, participantes e salas.
     */
    void addAllocatedLesson(AllocatedLesson lesson) {
        alocacoes_.push_back(std::move(lesson));
    }

    /**
     * @brief Obtém a lista de todas as aulas alocadas na grade (somente leitura).
     * @return Referência constante para o vetor de AllocatedLesson.
     */
    const std::vector<AllocatedLesson>& alocacoes() const noexcept { return alocacoes_; }

    /**
     * @brief Obtém a lista de todas as aulas alocadas para edição direta.
     * @return Referência mutável para o vetor de AllocatedLesson.
     */
    std::vector<AllocatedLesson>& alocacoes() noexcept { return alocacoes_; }

    /**
     * @brief Limpa todas as alocações, salas fixadas e diagnósticos da grade.
     */
    void clear() {
        alocacoes_.clear();
        salas_teoricas_designadas_.clear();
        estatisticas_ = TimetableStatistics();
        mensagens_diagnostico_.clear();
    }

    /**
     * @brief Obtém o mapa de salas teóricas fixas designadas para cada turma.
     * @return Referência constante para o mapa associando turma_id a sala_id.
     */
    const std::map<std::string, std::string>& salasTeoricasDesignadas() const noexcept {
        return salas_teoricas_designadas_;
    }

    /**
     * @brief Registra a designação de uma sala teórica fixa para uma turma.
     * @param turma_id Identificador da turma.
     * @param sala_id Identificador da sala atribuída.
     */
    void setSalaTeoricaDesignada(std::string turma_id, std::string sala_id) {
        salas_teoricas_designadas_[std::move(turma_id)] = std::move(sala_id);
    }

    /**
     * @brief Consulta a sala teórica designada para determinada turma.
     * @param turma_id Identificador da turma.
     * @return ID da sala designada ou string vazia caso não haja sala fixa atribuída.
     */
    std::string salaTeoricaTurma(const std::string& turma_id) const {
        auto it = salas_teoricas_designadas_.find(turma_id);
        if (it != salas_teoricas_designadas_.end()) return it->second;
        return "";
    }

    /**
     * @brief Obtém as estatísticas e métricas de desempenho da grade horária.
     * @return Referência constante para a estrutura TimetableStatistics.
     */
    const TimetableStatistics& estatisticas() const noexcept { return estatisticas_; }

    /**
     * @brief Define as estatísticas da grade horária.
     * @param stats Objeto TimetableStatistics contendo status e custos calculados.
     */
    void setEstatisticas(TimetableStatistics stats) { estatisticas_ = std::move(stats); }

    /**
     * @brief Obtém as mensagens de diagnóstico e logs gerados durante o escalonamento.
     * @return Referência constante para o vetor de mensagens de texto.
     */
    const std::vector<std::string>& mensagensDiagnostico() const noexcept { return mensagens_diagnostico_; }

    /**
     * @brief Adiciona uma nova mensagem ao relatório de diagnóstico do escalonamento.
     * @param msg Texto informativo ou de alerta.
     */
    void addMensagemDiagnostico(std::string msg) { mensagens_diagnostico_.push_back(std::move(msg)); }

    /**
     * @brief Define a lista de mensagens de diagnóstico.
     * @param msgs Vetor de strings com os diagnósticos.
     */
    void setMensagensDiagnostico(std::vector<std::string> msgs) { mensagens_diagnostico_ = std::move(msgs); }

    /**
     * @brief Filtra e retorna as aulas de uma determinada turma em um dia da semana, ordenadas cronologicamente.
     * @param turma_id Identificador da turma pesquisada.
     * @param d Dia da semana desejado.
     * @param semestre_filtro Filtro opcional por semestre (0 = todos, 1 = 1º semestre, 2 = 2º semestre).
     * @return Vetor ordenado de aulas alocadas para a turma no dia d.
     */
    std::vector<AllocatedLesson> aulasPorTurma(const std::string& turma_id, DayOfWeek d, int semestre_filtro = 0) const;

    /**
     * @brief Filtra e retorna as aulas lecionadas por um professor em um dia da semana, ordenadas cronologicamente.
     * @param prof_id Identificador do professor pesquisado.
     * @param d Dia da semana desejado.
     * @param semestre_filtro Filtro opcional por semestre (0 = todos, 1 = 1º semestre, 2 = 2º semestre).
     * @return Vetor ordenado de aulas lecionadas pelo professor no dia d.
     */
    std::vector<AllocatedLesson> aulasPorProfessor(const std::string& prof_id, DayOfWeek d, int semestre_filtro = 0) const;

    /**
     * @brief Filtra e retorna as aulas que ocupam uma sala em um dia da semana, ordenadas cronologicamente.
     * @param sala_id Identificador da sala pesquisada.
     * @param d Dia da semana desejado.
     * @param semestre_filtro Filtro opcional por semestre (0 = todos, 1 = 1º semestre, 2 = 2º semestre).
     * @return Vetor ordenado de aulas alocadas na sala no dia d.
     */
    std::vector<AllocatedLesson> aulasPorSala(const std::string& sala_id, DayOfWeek d, int semestre_filtro = 0) const;

    /**
     * @brief Localiza o ponteiro de uma aula alocada na grade a partir do seu ID.
     * @param aula_id Identificador único da aula (ex: "T01_MAT_1").
     * @return Ponteiro mutável para a AllocatedLesson correspondente, ou nullptr se não encontrada.
     */
    AllocatedLesson* findAula(const std::string& aula_id);

private:
    std::vector<AllocatedLesson> alocacoes_;
    std::map<std::string, std::string> salas_teoricas_designadas_;
    TimetableStatistics estatisticas_;
    std::vector<std::string> mensagens_diagnostico_;
};

} // namespace scheduler::model
