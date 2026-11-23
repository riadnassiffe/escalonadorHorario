#pragma once

/**
 * @file InstitutionData.h
 * @brief Definição da classe InstitutionData, repositório central de dados da instituição de ensino.
 */

#include "model/Professor.h"
#include "model/Room.h"
#include "model/Turma.h"
#include "model/Lesson.h"
#include "model/OptimizationConfig.h"
#include "model/Timetable.h"
#include <vector>
#include <string>
#include <memory>

namespace scheduler::model {

/**
 * @enum SlotConflictType
 * @brief Classificação dos tipos de choques ou violações de restrições detectadas em um horário.
 */
enum class SlotConflictType {
    TURMA_INATIVA,
    INTERVALO_RECREIO,
    FORA_JORNADA,
    SLOT_INCOMPATIVEL,
    INDISPONIBILIDADE_PROFESSOR,
    INCOMPATIBILIDADE_SALA,
    LIMITE_DIARIO_DISCIPLINA,
    CHOQUE_TURMA,
    CHOQUE_PROFESSOR,
    CHOQUE_SALA,
    DURACAO_INCOMPATIVEL
};

/**
 * @struct SlotConflictInfo
 * @brief Descrição estruturada de um choque ou violação de restrição para exibição na grade e diálogos.
 */
struct SlotConflictInfo {
    SlotConflictType type;
    std::string short_label;     ///< Rótulo resumido para exibição na célula (ex: "Prof. ocupado (2ºB)").
    std::string detail;          ///< Descrição completa para tooltip e diálogos de confirmação.
    std::string related_aula_id; ///< Identificador da aula conflitante, quando houver.
};

/**
 * @class InstitutionData
 * @brief Armazena e gerencia todas as entidades e configurações do sistema de agendamento escolar.
 *
 * Esta classe atua como modelo central de dados da instituição, agrupando professores,
 * salas, turmas, disciplinas, ocorrências de aulas geradas, parâmetros do otimizador
 * e a grade horária resultante.
 */
class InstitutionData {
public:
    /**
     * @brief Construtor padrão da classe InstitutionData.
     */
    InstitutionData();

    // ==========================================
    // Professores
    // ==========================================

    /**
     * @brief Obtém a lista de professores cadastrados (somente leitura).
     * @return Referência constante para o vetor de professores.
     */
    const std::vector<Professor>& professores() const noexcept { return professores_; }

    /**
     * @brief Obtém a lista de professores cadastrados para modificação.
     * @return Referência mutável para o vetor de professores.
     */
    std::vector<Professor>& professores() noexcept { return professores_; }

    /**
     * @brief Adiciona um novo professor ou substitui o existente com mesmo ID.
     * @param prof Objeto do tipo Professor a ser inserido ou atualizado.
     */
    void addProfessor(Professor prof);

    /**
     * @brief Remove um professor a partir do seu identificador.
     * @param id Identificador único (ID) do professor.
     * @return true se o professor foi encontrado e removido; false caso contrário.
     */
    bool removeProfessor(const std::string& id);

    /**
     * @brief Busca um professor pelo seu identificador (somente leitura).
     * @param id Identificador único (ID) do professor.
     * @return Ponteiro constante para o professor encontrado, ou nullptr se não existir.
     */
    const Professor* findProfessor(const std::string& id) const;

    /**
     * @brief Busca um professor pelo seu identificador para modificação.
     * @param id Identificador único (ID) do professor.
     * @return Ponteiro para o professor encontrado, ou nullptr se não existir.
     */
    Professor* findProfessor(const std::string& id);

    // ==========================================
    // Salas
    // ==========================================

    /**
     * @brief Obtém a lista de salas cadastradas (somente leitura).
     * @return Referência constante para o vetor de salas.
     */
    const std::vector<Room>& salas() const noexcept { return salas_; }

    /**
     * @brief Obtém a lista de salas cadastradas para modificação.
     * @return Referência mutável para o vetor de salas.
     */
    std::vector<Room>& salas() noexcept { return salas_; }

    /**
     * @brief Adiciona uma nova sala ou substitui a existente com mesmo ID.
     * @param sala Objeto do tipo Room a ser inserido ou atualizado.
     */
    void addSala(Room sala);

    /**
     * @brief Remove uma sala a partir do seu identificador.
     * @param id Identificador único (ID) da sala.
     * @return true se a sala foi encontrada e removida; false caso contrário.
     */
    bool removeSala(const std::string& id);

    /**
     * @brief Busca uma sala pelo seu identificador (somente leitura).
     * @param id Identificador único (ID) da sala.
     * @return Ponteiro constante para a sala encontrada, ou nullptr se não existir.
     */
    const Room* findSala(const std::string& id) const;

    /**
     * @brief Busca uma sala pelo seu identificador para modificação.
     * @param id Identificador único (ID) da sala.
     * @return Ponteiro para a sala encontrada, ou nullptr se não existir.
     */
    Room* findSala(const std::string& id);

    // ==========================================
    // Turmas
    // ==========================================

    /**
     * @brief Obtém a lista de turmas cadastradas (somente leitura).
     * @return Referência constante para o vetor de turmas.
     */
    const std::vector<Turma>& turmas() const noexcept { return turmas_; }

    /**
     * @brief Obtém a lista de turmas cadastradas para modificação.
     * @return Referência mutável para o vetor de turmas.
     */
    std::vector<Turma>& turmas() noexcept { return turmas_; }

    /**
     * @brief Adiciona uma nova turma ou substitui a existente com mesmo ID.
     * @param turma Objeto do tipo Turma a ser inserido ou atualizado.
     */
    void addTurma(Turma turma);

    /**
     * @brief Remove uma turma a partir do seu identificador.
     * @param id Identificador único (ID) da turma.
     * @return true se a turma foi encontrada e removida; false caso contrário.
     */
    bool removeTurma(const std::string& id);

    /**
     * @brief Busca uma turma pelo seu identificador (somente leitura).
     * @param id Identificador único (ID) da turma.
     * @return Ponteiro constante para a turma encontrada, ou nullptr se não existir.
     */
    const Turma* findTurma(const std::string& id) const;

    /**
     * @brief Busca uma turma pelo seu identificador para modificação.
     * @param id Identificador único (ID) da turma.
     * @return Ponteiro para a turma encontrada, ou nullptr se não existir.
     */
    Turma* findTurma(const std::string& id);

    // ==========================================
    // Disciplinas
    // ==========================================

    /**
     * @brief Obtém a lista de disciplinas cadastradas (somente leitura).
     * @return Referência constante para o vetor de disciplinas.
     */
    const std::vector<Disciplina>& disciplinas() const noexcept { return disciplinas_; }

    /**
     * @brief Obtém a lista de disciplinas cadastradas para modificação.
     * @return Referência mutável para o vetor de disciplinas.
     */
    std::vector<Disciplina>& disciplinas() noexcept { return disciplinas_; }

    /**
     * @brief Adiciona uma nova disciplina ou substitui a existente com mesmo ID.
     * @param disc Objeto do tipo Disciplina a ser inserido ou atualizado.
     */
    void addDisciplina(Disciplina disc);

    /**
     * @brief Remove uma disciplina a partir do seu identificador.
     * @param id Identificador único (ID) da disciplina.
     * @return true se a disciplina foi encontrada e removida; false caso contrário.
     */
    bool removeDisciplina(const std::string& id);

    /**
     * @brief Busca uma disciplina pelo seu identificador (somente leitura).
     * @param id Identificador único (ID) da disciplina.
     * @return Ponteiro constante para a disciplina encontrada, ou nullptr se não existir.
     */
    const Disciplina* findDisciplina(const std::string& id) const;

    /**
     * @brief Busca uma disciplina pelo seu identificador para modificação.
     * @param id Identificador único (ID) da disciplina.
     * @return Ponteiro para a disciplina encontrada, ou nullptr se não existir.
     */
    Disciplina* findDisciplina(const std::string& id);

    // ==========================================
    // Ocorrências de Aulas (A)
    // ==========================================

    /**
     * @brief Obtém a lista de ocorrências de aulas geradas (somente leitura).
     * @return Referência constante para o vetor de aulas (Lesson).
     */
    const std::vector<Lesson>& aulas() const noexcept { return aulas_; }

    /**
     * @brief Obtém a lista de ocorrências de aulas geradas para modificação.
     * @return Referência mutável para o vetor de aulas (Lesson).
     */
    std::vector<Lesson>& aulas() noexcept { return aulas_; }

    /**
     * @brief Define a lista de ocorrências de aulas.
     * @param aulas Vetor de aulas (Lesson) a ser atribuído.
     */
    void setAulas(std::vector<Lesson> aulas) { aulas_ = std::move(aulas); }

    /**
     * @brief Gera as ocorrências individuais de aulas (Lesson) a partir das disciplinas cadastradas.
     *
     * Desdobra cada disciplina na quantidade de aulas semanais requerida, aplicando filtros de semestre
     * e opcionalmente preservando o estado de fixação (pinned) de horários/salas de aulas pré-existentes.
     *
     * @param preservar_fixadas Se true, mantém dados de aulas que já estavam marcadas como fixadas.
     * @param escopo Escopo de semestre a considerar na geração (TODAS_SEM_FILTRO, SEMESTRE_1 ou SEMESTRE_2).
     */
    void gerarOcorrenciasAulas(bool preservar_fixadas = true,
                               EscopoSemestreOtimizacao escopo = EscopoSemestreOtimizacao::TODAS_SEM_FILTRO);

    /**
     * @brief Busca uma ocorrência de aula pelo seu identificador (somente leitura).
     * @param id Identificador único da aula (ex: "T01_MAT_1").
     * @return Ponteiro constante para a aula encontrada, ou nullptr se não existir.
     */
    const Lesson* findAula(const std::string& id) const;

    /**
     * @brief Busca uma ocorrência de aula pelo seu identificador para modificação.
     * @param id Identificador único da aula (ex: "T01_MAT_1").
     * @return Ponteiro para a aula encontrada, ou nullptr se não existir.
     */
    Lesson* findAula(const std::string& id);

    // ==========================================
    // Modelos padrão de slots por turno
    // ==========================================

    /**
     * @brief Obtém o mapa completo de modelos padrão de slots por turno.
     * @return Referência constante para o mapa ShiftType -> lista de TimeInterval.
     */
    const std::map<ShiftType, std::vector<TimeInterval>>& slotTemplates() const noexcept {
        return slot_templates_;
    }

    /**
     * @brief Obtém os slots padrão configurados para um turno específico.
     * @param shift Turno consultado.
     * @return Vetor de TimeInterval correspondente (ou padrão do sistema se não customizado).
     */
    std::vector<TimeInterval> slotTemplateForShift(ShiftType shift) const;

    /**
     * @brief Define os slots padrão de um turno para reutilização no cadastro de turmas.
     * @param shift Turno configurado.
     * @param slots Lista de slots ordenados.
     */
    void setSlotTemplateForShift(ShiftType shift, std::vector<TimeInterval> slots);

    // ==========================================
    // Configuração da Otimização
    // ==========================================

    /**
     * @brief Obtém a configuração dos parâmetros de otimização (somente leitura).
     * @return Referência constante para OptimizationConfig.
     */
    const OptimizationConfig& config() const noexcept { return config_; }

    /**
     * @brief Obtém a configuração dos parâmetros de otimização para modificação.
     * @return Referência mutável para OptimizationConfig.
     */
    OptimizationConfig& config() noexcept { return config_; }

    /**
     * @brief Define a configuração dos parâmetros de otimização.
     * @param cfg Nova configuração de otimização a ser aplicada.
     */
    void setConfig(OptimizationConfig cfg) { config_ = std::move(cfg); }

    // ==========================================
    // Grade gerada
    // ==========================================

    /**
     * @brief Obtém a grade horária calculada/gerada (somente leitura).
     * @return Referência constante para a Timetable da instituição.
     */
    const Timetable& grade() const noexcept { return grade_; }

    /**
     * @brief Obtém a grade horária calculada/gerada para modificação.
     * @return Referência mutável para a Timetable da instituição.
     */
    Timetable& grade() noexcept { return grade_; }

    /**
     * @brief Define a grade horária resultante do escalonamento.
     * @param grade Objeto Timetable com as alocações da grade.
     */
    void setGrade(Timetable grade) { grade_ = std::move(grade); }

    /**
     * @brief Realiza a auditoria completa de conflitos de restrições na grade horária atual.
     *
     * Valida choques de turma, choques de professor, choques de sala, indisponibilidades docentes,
     * intervalos/recreios das turmas, jornada diária e limites diários de aulas por disciplina.
     *
     * @return Mapa associando o identificador da aula (aula_id) a um vetor com os conflitos detectados.
     */
    std::map<std::string, std::vector<std::string>> verificarConflitosGrade() const;

    /**
     * @brief Realiza a auditoria detalhada e estruturada de conflitos na grade horária atual.
     * @return Mapa associando o identificador da aula (aula_id) a um vetor de SlotConflictInfo.
     */
    std::map<std::string, std::vector<SlotConflictInfo>> verificarConflitosGradeDetalhado() const;

    /**
     * @brief Avalia se a alocação ou movimentação de uma aula para determinado dia e horário gera choques.
     * @param aula_id ID da aula candidata (presente em grade() ou aulas()).
     * @param dia Dia da semana pretendido.
     * @param intervalo Intervalo de tempo pretendido.
     * @param ignore_aula_ids IDs de aulas na grade a desconsiderar (ex: a própria aula ou par de troca).
     * @param salas_override Salas alternativas a considerar no lugar das atuais (se não vazio).
     * @return Lista estruturada de choques e violações detectadas (vazia se o horário estiver livre).
     */
    std::vector<SlotConflictInfo> avaliarAulaNoHorario(
        const std::string& aula_id,
        DayOfWeek dia,
        const TimeInterval& intervalo,
        const std::vector<std::string>& ignore_aula_ids = {},
        const std::vector<std::string>& salas_override = {}) const;

    /**
     * @brief Avalia a viabilidade de permutar os horários entre duas aulas já alocadas na grade.
     * @param aula_a_id ID da primeira aula.
     * @param aula_b_id ID da segunda aula.
     * @return Lista estruturada de choques resultantes da troca nos dois sentidos (vazia se livre).
     */
    std::vector<SlotConflictInfo> avaliarTrocaAulas(
        const std::string& aula_a_id,
        const std::string& aula_b_id) const;

    /**
     * @brief Remove todos os dados cadastrados (professores, salas, turmas, disciplinas, aulas e grade).
     */
    void clear();

    // ==========================================
    // Geração dinâmica de IDs para novas entidades
    // ==========================================

    /**
     * @brief Gera o próximo identificador sequencial disponível para professores.
     * @return String formatada com o próximo ID livre (ex: "P01", "P02").
     */
    std::string nextProfessorId() const;

    /**
     * @brief Gera o próximo identificador sequencial disponível para turmas.
     * @return String formatada com o próximo ID livre (ex: "T01", "T02").
     */
    std::string nextTurmaId() const;

    /**
     * @brief Gera o próximo identificador sequencial disponível para salas.
     * @return String formatada com o próximo ID livre (ex: "S01", "S02").
     */
    std::string nextSalaId() const;

    /**
     * @brief Gera o próximo identificador sequencial disponível para disciplinas.
     * @return String formatada com o próximo ID livre (ex: "D01", "D02").
     */
    std::string nextDisciplinaId() const;

    // ==========================================
    // Fábrica de Exemplo
    // ==========================================

    /**
     * @brief Cria e inicializa uma instância de InstitutionData com o cenário acadêmico padrão.
     *
     * Popula a instituição com professores, preferências, disponibilidades, salas teóricas e laboratórios,
     * turmas e disciplinas anuais e semestrais conforme as especificações do sistema.
     *
     * @return Instância configurada de InstitutionData pronta para teste ou demonstração.
     */
    static InstitutionData criarExemploPadrao();

private:
    std::vector<Professor> professores_;
    std::vector<Room> salas_;
    std::vector<Turma> turmas_;
    std::vector<Disciplina> disciplinas_;
    std::vector<Lesson> aulas_;
    OptimizationConfig config_;
    Timetable grade_;
    std::map<ShiftType, std::vector<TimeInterval>> slot_templates_;
};

} // namespace scheduler::model
