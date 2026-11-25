#pragma once

/**
 * @file Lesson.h
 * @brief Definições de regimes letivos, disciplinas acadêmicas e ocorrências de aulas (Lesson).
 */

#include "model/TimeSlot.h"
#include <string>
#include <vector>
#include <optional>

namespace scheduler::model {

/**
 * @enum RegimeDisciplina
 * @brief Regime acadêmico de oferta da disciplina (anual ou semestral).
 */
enum class RegimeDisciplina {
    ANUAL,      ///< Disciplina anual: válida para o ano letivo completo (ambos os semestres).
    SEMESTRAL   ///< Disciplina semestral: válida para um semestre específico (1º ou 2º semestre).
};

/**
 * @brief Converte o enum RegimeDisciplina em string textual ("ANUAL" ou "SEMESTRAL").
 * @param r Regime a ser convertido.
 * @return String representativa do regime.
 */
inline std::string regimeToString(RegimeDisciplina r) {
    switch (r) {
        case RegimeDisciplina::ANUAL: return "ANUAL";
        case RegimeDisciplina::SEMESTRAL: return "SEMESTRAL";
    }
    return "ANUAL";
}

/**
 * @brief Converte uma string no enum RegimeDisciplina correspondente.
 * @param str String representativa (ex: "SEMESTRAL", "Semestral", "ANUAL").
 * @return RegimeDisciplina correspondente.
 */
inline RegimeDisciplina stringToRegime(const std::string& str) {
    if (str == "SEMESTRAL" || str == "Semestral" || str == "semestral") {
        return RegimeDisciplina::SEMESTRAL;
    }
    return RegimeDisciplina::ANUAL;
}

/**
 * @brief Retorna uma descrição amigável para exibição do regime na interface ("Anual (Ano Todo)", "Semestral").
 * @param r Regime acadêmico.
 * @return String formatada para exibição ao usuário.
 */
inline std::string regimeDisplayName(RegimeDisciplina r) {
    switch (r) {
        case RegimeDisciplina::ANUAL: return "Anual (Ano Todo)";
        case RegimeDisciplina::SEMESTRAL: return "Semestral";
    }
    return "Anual";
}

/**
 * @enum SemestreLetivo
 * @brief Identifica o período semestral de vigência de uma disciplina ou aula.
 */
enum class SemestreLetivo {
    SEMESTRE_1 = 1, ///< 1º Semestre letivo.
    SEMESTRE_2 = 2, ///< 2º Semestre letivo.
    AMBOS = 3       ///< Ambos os Semestres letivos (ano todo).
};

/**
 * @brief Converte o enum SemestreLetivo em código textual ("1", "2" ou "AMBOS").
 * @param s Semestre a ser convertido.
 * @return String com o código do semestre.
 */
inline std::string semestreToString(SemestreLetivo s) {
    switch (s) {
        case SemestreLetivo::SEMESTRE_1: return "1";
        case SemestreLetivo::SEMESTRE_2: return "2";
        case SemestreLetivo::AMBOS: return "AMBOS";
    }
    return "AMBOS";
}

/**
 * @brief Converte uma representação textual de semestre no enum SemestreLetivo correspondente.
 * @param str String contendo identificação de semestre (ex: "1", "2", "AMBOS", "1º Semestre").
 * @return Valor de SemestreLetivo correspondente.
 */
inline SemestreLetivo stringToSemestre(const std::string& str) {
    if (str == "1" || str == "SEMESTRE_1" || str == "1º Semestre" || str == "1o Semestre" || str == "1º sem") {
        return SemestreLetivo::SEMESTRE_1;
    }
    if (str == "2" || str == "SEMESTRE_2" || str == "2º Semestre" || str == "2o Semestre" || str == "2º sem") {
        return SemestreLetivo::SEMESTRE_2;
    }
    return SemestreLetivo::AMBOS;
}

/**
 * @brief Retorna o nome amigável para exibição em interface do semestre letivo.
 * @param s Semestre letivo.
 * @return String formatada (ex: "1º Semestre", "2º Semestre", "Ambos os Semestres").
 */
inline std::string semestreDisplayName(SemestreLetivo s) {
    switch (s) {
        case SemestreLetivo::SEMESTRE_1: return "1º Semestre";
        case SemestreLetivo::SEMESTRE_2: return "2º Semestre";
        case SemestreLetivo::AMBOS: return "Ambos os Semestres";
    }
    return "Ambos os Semestres";
}

/**
 * @class Disciplina
 * @brief Representa o componente curricular acadêmico oferecido na instituição.
 *
 * Contém a carga semanal de aulas, duração em minutos de cada aula, professores habilitados,
 * turmas atendidas, regime (anual/semestral) e requisitos de salas/laboratórios.
 */
class Disciplina {
public:
    /**
     * @brief Construtor padrão da classe Disciplina.
     */
    Disciplina() = default;

    /**
     * @brief Construtor parametrizado completo da classe Disciplina.
     * @param id Identificador único da disciplina (ex: "T01_MAT").
     * @param nome Nome descritivo da disciplina (ex: "Matemática").
     * @param tipo Tipo da disciplina ("TEORICA", "LABORATORIO", etc.).
     * @param duracao_minutos Duração de cada aula em minutos (ex: 45 ou 50).
     * @param aulas_semanais Número de aulas semanais requeridas.
     * @param regime Regime de oferta (RegimeDisciplina::ANUAL ou SEMESTRAL).
     * @param semestre Semestre de vigência (SemestreLetivo::AMBOS, SEMESTRE_1 ou SEMESTRE_2).
     */
    Disciplina(std::string id, std::string nome, std::string tipo = "TEORICA",
               int duracao_minutos = 50, int aulas_semanais = 4,
               RegimeDisciplina regime = RegimeDisciplina::ANUAL,
               SemestreLetivo semestre = SemestreLetivo::AMBOS)
        : id_(std::move(id)), nome_(std::move(nome)), tipo_(std::move(tipo)),
          duracao_minutos_(duracao_minutos), aulas_semanais_(aulas_semanais),
          regime_(regime), semestre_(semestre) {}

    /**
     * @brief Obtém o identificador único da disciplina.
     * @return Referência constante para a string do ID.
     */
    const std::string& id() const noexcept { return id_; }

    /**
     * @brief Define o identificador único da disciplina.
     * @param id Novo ID da disciplina.
     */
    void setId(std::string id) { id_ = std::move(id); }

    /**
     * @brief Obtém o nome da disciplina.
     * @return Referência constante para a string do nome.
     */
    const std::string& nome() const noexcept { return nome_; }

    /**
     * @brief Define o nome da disciplina.
     * @param nome Novo nome da disciplina.
     */
    void setNome(std::string nome) { nome_ = std::move(nome); }

    /**
     * @brief Obtém o tipo pedagógico da disciplina (ex: "TEORICA", "LABORATORIO").
     * @return Referência constante para o tipo.
     */
    const std::string& tipo() const noexcept { return tipo_; }

    /**
     * @brief Define o tipo da disciplina.
     * @param tipo Novo tipo (ex: "TEORICA", "LABORATORIO").
     */
    void setTipo(std::string tipo) { tipo_ = std::move(tipo); }

    /**
     * @brief Obtém a duração de cada aula em minutos.
     * @return Duração em minutos.
     */
    int duracaoMinutos() const noexcept { return duracao_minutos_; }

    /**
     * @brief Define a duração de cada aula em minutos.
     * @param duracao Duração em minutos (ex: 45 ou 50).
     */
    void setDuracaoMinutos(int duracao) noexcept { duracao_minutos_ = duracao; }

    /**
     * @brief Obtém a quantidade de aulas que devem ser ministradas por semana.
     * @return Quantidade de aulas semanais.
     */
    int aulasSemanais() const noexcept { return aulas_semanais_; }

    /**
     * @brief Define o número de aulas por semana.
     * @param aulas Quantidade de aulas semanais.
     */
    void setAulasSemanais(int aulas) noexcept { aulas_semanais_ = aulas; }

    /**
     * @brief Obtém o regime acadêmico da disciplina (ANUAL ou SEMESTRAL).
     * @return RegimeDisciplina configurado.
     */
    RegimeDisciplina regime() const noexcept { return regime_; }

    /**
     * @brief Define o regime acadêmico da disciplina.
     * @param r Novo RegimeDisciplina.
     */
    void setRegime(RegimeDisciplina r) noexcept { regime_ = r; }

    /**
     * @brief Obtém o semestre letivo em que a disciplina é ofertada.
     * @return SemestreLetivo configurado.
     */
    SemestreLetivo semestre() const noexcept { return semestre_; }

    /**
     * @brief Define o semestre letivo de oferta da disciplina.
     * @param s Novo SemestreLetivo.
     */
    void setSemestre(SemestreLetivo s) noexcept { semestre_ = s; }

    /**
     * @brief Verifica se a disciplina possui regime anual.
     * @return true se for anual, false se for semestral.
     */
    bool isAnual() const noexcept { return regime_ == RegimeDisciplina::ANUAL; }

    /**
     * @brief Verifica se a disciplina possui regime semestral.
     * @return true se for semestral, false se for anual.
     */
    bool isSemestral() const noexcept { return regime_ == RegimeDisciplina::SEMESTRAL; }

    /**
     * @brief Verifica se a disciplina deve ser ministrada no semestre informado.
     * @param sem Semestre analisado.
     * @return true se a disciplina atende ao semestre especificado, false caso contrário.
     */
    bool atendeSemestre(SemestreLetivo sem) const noexcept {
        if (isAnual() || sem == SemestreLetivo::AMBOS) return true;
        return semestre_ == sem || semestre_ == SemestreLetivo::AMBOS;
    }

    /**
     * @brief Retorna uma string formatada descrevendo o regime e semestre da disciplina.
     * @return String descritiva (ex: "Anual", "Semestral (1º Sem)", "Semestral (2º Sem)").
     */
    std::string regimeSemestreFormatado() const {
        if (isAnual()) return "Anual";
        if (semestre_ == SemestreLetivo::SEMESTRE_1) return "Semestral (1º Sem)";
        if (semestre_ == SemestreLetivo::SEMESTRE_2) return "Semestral (2º Sem)";
        return "Semestral";
    }

    /**
     * @brief Obtém os IDs dos professores habilitados ou responsáveis pela disciplina.
     * @return Referência constante para o vetor de IDs de professores.
     */
    const std::vector<std::string>& professores() const noexcept { return professores_; }

    /**
     * @brief Define os professores habilitados para lecionar a disciplina.
     * @param profs Vetor com os IDs dos professores.
     */
    void setProfessores(std::vector<std::string> profs) { professores_ = std::move(profs); }

    /**
     * @brief Adiciona um professor à lista de docentes habilitados para a disciplina.
     * @param prof_id Identificador do professor.
     */
    void addProfessor(std::string prof_id) { professores_.push_back(std::move(prof_id)); }

    /**
     * @brief Obtém os IDs das turmas vinculadas à disciplina.
     * @return Referência constante para o vetor de IDs de turmas.
     */
    const std::vector<std::string>& turmas() const noexcept { return turmas_; }

    /**
     * @brief Define as turmas que cursarão a disciplina.
     * @param turmas Vetor com os IDs das turmas vinculadas.
     */
    void setTurmas(std::vector<std::string> turmas) { turmas_ = std::move(turmas); }

    /**
     * @brief Adiciona uma turma à disciplina.
     * @param turma_id Identificador da turma.
     */
    void addTurma(std::string turma_id) { turmas_.push_back(std::move(turma_id)); }

    /**
     * @brief Obtém as combinações alternativas de salas admissíveis (seção 3.3).
     * @return Referência constante para o vetor de combinações de salas (ex: [["LAB01"], ["LAB02"]]).
     */
    const std::vector<std::vector<std::string>>& salasAlternativas() const noexcept { return salas_alternativas_; }

    /**
     * @brief Define os conjuntos de salas alternativas aceitáveis para as aulas.
     * @param salas Vetor de alternativas de salas.
     */
    void setSalasAlternativas(std::vector<std::vector<std::string>> salas) { salas_alternativas_ = std::move(salas); }

    /**
     * @brief Adiciona uma combinação alternativa de salas admitida.
     * @param combo Vetor com os IDs das salas que atendem a aula em conjunto.
     */
    void addCombinacaoSalas(std::vector<std::string> combo) { salas_alternativas_.push_back(std::move(combo)); }

    /**
     * @brief Indica se as aulas semanais da disciplina devem ocorrer de forma consecutiva (geminada).
     * @return true se exigir aulas consecutivas, false se puderem ser isoladas.
     */
    bool exigirAulasConsecutivas() const noexcept { return exigir_aulas_consecutivas_; }

    /**
     * @brief Define a restrição de exigência de aulas consecutivas.
     * @param exigir Flag indicando se as aulas devem ser agrupadas consecutivamente.
     */
    void setExigirAulasConsecutivas(bool exigir) noexcept { exigir_aulas_consecutivas_ = exigir; }

    /**
     * @brief Obtém o limite máximo de aulas diárias desta disciplina para uma mesma turma.
     * @return Número máximo de aulas por dia.
     */
    int maxAulasPorDiaTurma() const noexcept { return max_aulas_por_dia_turma_; }

    /**
     * @brief Define o limite máximo de aulas diárias desta disciplina para uma mesma turma.
     * @param max_aulas Quantidade máxima de aulas no mesmo dia.
     */
    void setMaxAulasPorDiaTurma(int max_aulas) noexcept { max_aulas_por_dia_turma_ = max_aulas; }

    /**
     * @brief Verifica se a disciplina possui natureza estritamente teórica.
     * @return true se o tipo for "TEORICA" ou "Teorica", false caso contrário.
     */
    bool isTeorica() const noexcept { return tipo_ == "TEORICA" || tipo_ == "Teorica"; }

private:
    std::string id_;
    std::string nome_;
    std::string tipo_{"TEORICA"};
    int duracao_minutos_{50};
    int aulas_semanais_{4};
    RegimeDisciplina regime_{RegimeDisciplina::ANUAL};
    SemestreLetivo semestre_{SemestreLetivo::AMBOS};
    std::vector<std::string> professores_;
    std::vector<std::string> turmas_;
    std::vector<std::vector<std::string>> salas_alternativas_;
    bool exigir_aulas_consecutivas_{false};
    int max_aulas_por_dia_turma_{2};
};

/**
 * @class Lesson
 * @brief Representa uma ocorrência atômica individual de aula gerada a partir de uma disciplina.
 *
 * É a entidade elementar alocada pelo solver na grade horária. Pode conter fixação manual
 * de horário, dia e salas atribuídas (pinned).
 */
class Lesson {
public:
    /**
     * @brief Construtor padrão da classe Lesson.
     */
    Lesson() = default;

    /**
     * @brief Construtor parametrizado completo para ocorrência de aula.
     * @param id Identificador único da ocorrência de aula (ex: "T01_MAT_1").
     * @param disciplina_id Identificador da disciplina de origem.
     * @param disciplina_nome Nome da disciplina.
     * @param tipo Tipo da aula (ex: "TEORICA", "LABORATORIO").
     * @param duracao_minutos Duração da aula em minutos.
     * @param professores Vetor de IDs dos professores atribuídos à aula.
     * @param turmas Vetor de IDs das turmas participantes.
     * @param regime Regime letivo (RegimeDisciplina::ANUAL ou SEMESTRAL).
     * @param semestre Semestre de vigência da aula.
     */
    Lesson(std::string id, std::string disciplina_id, std::string disciplina_nome,
           std::string tipo, int duracao_minutos,
           std::vector<std::string> professores, std::vector<std::string> turmas,
           RegimeDisciplina regime = RegimeDisciplina::ANUAL,
           SemestreLetivo semestre = SemestreLetivo::AMBOS)
        : id_(std::move(id)), disciplina_id_(std::move(disciplina_id)),
          disciplina_nome_(std::move(disciplina_nome)), tipo_(std::move(tipo)),
          duracao_minutos_(duracao_minutos), professores_(std::move(professores)),
          turmas_(std::move(turmas)), regime_(regime), semestre_(semestre) {}

    /**
     * @brief Obtém o identificador único da aula.
     * @return Referência constante para o ID.
     */
    const std::string& id() const noexcept { return id_; }

    /**
     * @brief Define o identificador único da aula.
     * @param id Novo ID da aula.
     */
    void setId(std::string id) { id_ = std::move(id); }

    /**
     * @brief Obtém o identificador da disciplina de origem.
     * @return Referência constante para o ID da disciplina.
     */
    const std::string& disciplinaId() const noexcept { return disciplina_id_; }

    /**
     * @brief Define o identificador da disciplina de origem.
     * @param id Novo ID da disciplina.
     */
    void setDisciplinaId(std::string id) { disciplina_id_ = std::move(id); }

    /**
     * @brief Obtém o nome da disciplina associada.
     * @return Referência constante para o nome da disciplina.
     */
    const std::string& disciplinaNome() const noexcept { return disciplina_nome_; }

    /**
     * @brief Define o nome da disciplina associada.
     * @param nome Novo nome da disciplina.
     */
    void setDisciplinaNome(std::string nome) { disciplina_nome_ = std::move(nome); }

    /**
     * @brief Obtém o tipo pedagógico da aula (ex: "TEORICA", "LABORATORIO").
     * @return String com o tipo da aula.
     */
    const std::string& tipo() const noexcept { return tipo_; }

    /**
     * @brief Define o tipo pedagógico da aula.
     * @param tipo Novo tipo da aula.
     */
    void setTipo(std::string tipo) { tipo_ = std::move(tipo); }

    /**
     * @brief Obtém a duração da aula em minutos.
     * @return Duração em minutos.
     */
    int duracaoMinutos() const noexcept { return duracao_minutos_; }

    /**
     * @brief Define a duração da aula em minutos.
     * @param duracao Duração em minutos.
     */
    void setDuracaoMinutos(int duracao) noexcept { duracao_minutos_ = duracao; }

    /**
     * @brief Obtém o regime letivo da aula (ANUAL ou SEMESTRAL).
     * @return RegimeDisciplina correspondente.
     */
    RegimeDisciplina regime() const noexcept { return regime_; }

    /**
     * @brief Define o regime letivo da aula.
     * @param r Regime acadêmico configurado.
     */
    void setRegime(RegimeDisciplina r) noexcept { regime_ = r; }

    /**
     * @brief Obtém o semestre letivo de oferta da aula.
     * @return SemestreLetivo configurado.
     */
    SemestreLetivo semestre() const noexcept { return semestre_; }

    /**
     * @brief Define o semestre letivo da aula.
     * @param s Semestre letivo configurado.
     */
    void setSemestre(SemestreLetivo s) noexcept { semestre_ = s; }

    /**
     * @brief Obtém o semestre letivo atualmente ativo para execução (1 ou 2).
     * @return Inteiro indicando o semestre ativo (1 ou 2).
     */
    int semestreAtivo() const noexcept { return semestre_ativo_; }

    /**
     * @brief Define qual semestre letivo está ativo na otimização para esta aula.
     * @param s Semestre ativo (1 ou 2).
     */
    void setSemestreAtivo(int s) noexcept { semestre_ativo_ = s; }

    /**
     * @brief Verifica se a aula é de regime anual.
     * @return true se for anual, false se for semestral.
     */
    bool isAnual() const noexcept { return regime_ == RegimeDisciplina::ANUAL; }

    /**
     * @brief Verifica se a aula é de regime semestral.
     * @return true se for semestral, false se for anual.
     */
    bool isSemestral() const noexcept { return regime_ == RegimeDisciplina::SEMESTRAL; }

    /**
     * @brief Retorna uma string formatada descrevendo o regime e semestre da aula.
     * @return String descritiva (ex: "Anual", "Semestral (1º Sem)", "Semestral (2º Sem)").
     */
    std::string regimeSemestreFormatado() const {
        if (isAnual()) return "Anual";
        if (semestre_ == SemestreLetivo::SEMESTRE_2 || semestre_ativo_ == 2) return "Semestral (2º Sem)";
        if (semestre_ == SemestreLetivo::SEMESTRE_1 || semestre_ativo_ == 1) return "Semestral (1º Sem)";
        return "Semestral";
    }

    /**
     * @brief Obtém os IDs dos professores alocados nesta aula.
     * @return Referência constante para o vetor de IDs de professores.
     */
    const std::vector<std::string>& professores() const noexcept { return professores_; }

    /**
     * @brief Define os professores atribuídos a esta aula.
     * @param profs Vetor com os IDs dos docentes.
     */
    void setProfessores(std::vector<std::string> profs) { professores_ = std::move(profs); }

    /**
     * @brief Obtém os IDs das turmas participantes desta aula.
     * @return Referência constante para o vetor de IDs de turmas.
     */
    const std::vector<std::string>& turmas() const noexcept { return turmas_; }

    /**
     * @brief Define as turmas participantes desta aula.
     * @param turmas Vetor com os IDs das turmas.
     */
    void setTurmas(std::vector<std::string> turmas) { turmas_ = std::move(turmas); }

    /**
     * @brief Obtém as opções de conjuntos de salas admissíveis para esta aula.
     * @return Referência constante para a matriz de alternativas de salas.
     */
    const std::vector<std::vector<std::string>>& salasAlternativas() const noexcept { return salas_alternativas_; }

    /**
     * @brief Define os conjuntos alternativos de salas admissíveis para a aula.
     * @param salas Alternativas de salas.
     */
    void setSalasAlternativas(std::vector<std::vector<std::string>> salas) { salas_alternativas_ = std::move(salas); }

    /**
     * @brief Indica se a aula possui ao menos uma combinação não vazia de salas explícitas.
     * @return true se houver salas alternativas explicitamente configuradas.
     */
    bool temSalasAlternativasExplicitas() const noexcept {
        for (const auto& combo : salas_alternativas_) {
            if (!combo.empty()) return true;
        }
        return false;
    }

    /**
     * @brief Verifica se a aula é puramente teórica.
     * @return true se for teórica, false se for prática ou laboratório.
     */
    bool isTeorica() const noexcept { return tipo_ == "TEORICA" || tipo_ == "Teorica"; }

    /**
     * @brief Indica se a aula está fixada manualmente em horário, dia e salas específicos.
     * @return true se estiver fixada (pinned), false se estiver livre para o solver escalonar.
     */
    bool isPinned() const noexcept { return is_pinned_; }

    /**
     * @brief Define o estado de fixação manual (pinned) da aula.
     * @param pinned true para travar a aula, false para destravá-la.
     */
    void setPinned(bool pinned) noexcept { is_pinned_ = pinned; }

    /**
     * @brief Obtém o dia da semana em que a aula está fixada.
     * @return DayOfWeek fixado.
     */
    DayOfWeek pinnedDay() const noexcept { return pinned_day_; }

    /**
     * @brief Define o dia da semana em que a aula deve ser fixada.
     * @param day Dia da semana da fixação.
     */
    void setPinnedDay(DayOfWeek day) noexcept { pinned_day_ = day; }

    /**
     * @brief Obtém o intervalo de horário em que a aula está fixada.
     * @return TimeInterval fixado [start_min, end_min).
     */
    const TimeInterval& pinnedInterval() const noexcept { return pinned_interval_; }

    /**
     * @brief Define o intervalo de horário da fixação da aula.
     * @param interval Intervalo [start_min, end_min) fixado.
     */
    void setPinnedInterval(const TimeInterval& interval) noexcept { pinned_interval_ = interval; }

    /**
     * @brief Obtém o minuto inicial em que a aula está fixada.
     * @return Minuto de início desde 00:00 (0 a 1439).
     */
    int pinnedStartMin() const noexcept { return pinned_interval_.start_min; }

    /**
     * @brief Define o minuto inicial da fixação (ajustando o fim pela duração da aula).
     * @param start_min Minuto inicial desde 00:00.
     */
    void setPinnedStartMin(int start_min) noexcept {
        pinned_interval_ = TimeInterval(start_min, start_min + duracao_minutos_);
    }

    /**
     * @brief Obtém a lista de IDs de salas fixadas para esta aula.
     * @return Referência constante para o vetor de IDs das salas fixadas.
     */
    const std::vector<std::string>& pinnedRooms() const noexcept { return pinned_rooms_; }

    /**
     * @brief Define a lista de salas fixadas para a aula.
     * @param rooms Vetor com os IDs das salas atribuídas.
     */
    void setPinnedRooms(std::vector<std::string> rooms) { pinned_rooms_ = std::move(rooms); }

    /**
     * @brief Desfaz a fixação manual da aula, liberando-a para re-otimização pelo solver.
     */
    void unpin() noexcept {
        is_pinned_ = false;
        pinned_rooms_.clear();
    }

private:
    std::string id_;
    std::string disciplina_id_;
    std::string disciplina_nome_;
    std::string tipo_{"TEORICA"};
    int duracao_minutos_{50};
    RegimeDisciplina regime_{RegimeDisciplina::ANUAL};
    SemestreLetivo semestre_{SemestreLetivo::AMBOS};
    int semestre_ativo_{1};
    std::vector<std::string> professores_;
    std::vector<std::string> turmas_;
    std::vector<std::vector<std::string>> salas_alternativas_;

    bool is_pinned_{false};
    DayOfWeek pinned_day_{DayOfWeek::SEG};
    TimeInterval pinned_interval_;
    std::vector<std::string> pinned_rooms_;
};

} // namespace scheduler::model
