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
};

} // namespace scheduler::model
