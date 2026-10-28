/**
 * @file AddLessonToScheduleDialog.h
 * @brief Diálogo para alocação manual de disciplinas e aulas na grade horária semanal.
 */

#pragma once

#include "model/InstitutionData.h"
#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <vector>
#include <string>

namespace scheduler::ui {

/**
 * @class AddLessonToScheduleDialog
 * @brief Diálogo moderno para seleção e alocação manual de disciplinas na grade horária semanal.
 *
 * Permite ao usuário escolher uma disciplina cadastrada, selecionar a ocorrência semanal da aula,
 * definir o dia da semana, o horário letivo, a sala de aula e o semestre, validando choques e
 * restrições (turma, professor, sala e intervalo) em tempo real para compor uma grade inicial.
 */
class AddLessonToScheduleDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constrói o diálogo para alocação manual de disciplina na grade.
     * @param data Referência para o modelo de dados acadêmicos e grade horária da instituição.
     * @param parent Widget pai na hierarquia Qt.
     * @param default_day Dia da semana inicialmente sugerido ou selecionado na célula da tabela.
     * @param default_interval Intervalo de tempo inicialmente sugerido ou selecionado na célula.
     * @param default_turma_id ID opcional da turma pré-selecionada no filtro da visão.
     * @param default_professor_id ID opcional do professor pré-selecionado no filtro da visão.
     * @param default_room_id ID opcional da sala pré-selecionada no filtro da visão.
     */
    AddLessonToScheduleDialog(model::InstitutionData& data,
                              QWidget* parent = nullptr,
                              model::DayOfWeek default_day = model::DayOfWeek::SEG,
                              const model::TimeInterval& default_interval = model::TimeInterval(),
                              const std::string& default_turma_id = "",
                              const std::string& default_professor_id = "",
                              const std::string& default_room_id = "");

    /**
     * @brief Obtém a aula alocada resultante da configuração definida pelo usuário.
     * @return Objeto AllocatedLesson pronto para inserção na grade.
     */
    const model::AllocatedLesson& allocatedLesson() const noexcept { return result_allocation_; }

    /**
     * @brief Informa se a opção de fixação (Pin) foi marcada para proteger a aula contra o solver.
     * @return true se a aula deve ser fixada; false se deve ficar livre para o otimizador.
     */
    bool isPinned() const noexcept { return is_pinned_; }

    /**
     * @brief Retorna o identificador único da ocorrência da aula escolhida.
     * @return String com o ID da ocorrência da aula (ex: "MAT01_1").
     */
    std::string aulaId() const;

private slots:
    /**
     * @brief Atualiza os campos ao trocar a disciplina selecionada no combobox.
     * @param index Novo índice da disciplina.
     */
    void onDisciplinaChanged(int index);

    /**
     * @brief Atualiza a seleção da ocorrência da aula semanal.
     * @param index Novo índice da ocorrência.
     */
    void onOcorrenciaChanged(int index);

    /**
     * @brief Reavalia horários e validações ao alterar o dia da semana.
     * @param index Novo índice do dia da semana.
     */
    void onDiaChanged(int index);

    /**
     * @brief Revalida choques e restrições ao alterar o intervalo de horário selecionado.
     * @param index Novo índice do intervalo.
     */
    void onHorarioChanged(int index);

    /**
     * @brief Revalida choques e disponibilidade de sala ao alterar a sala selecionada.
     * @param index Novo índice da sala.
     */
    void onSalaChanged(int index);

    /**
     * @brief Atualiza os filtros de exibição de disciplinas (apenas turma atual ou apenas pendentes).
     */
    void onFiltrosChanged();

    /**
     * @brief Confirma a alocação e valida se há choques impeditivos antes de fechar o diálogo.
     */
    void onAcceptClicked();

private:
    /**
     * @brief Inicializa e monta os componentes gráficos do diálogo.
     */
    void setupUi();

    /**
     * @brief Popula a lista de disciplinas aplicando os filtros ativos.
     */
    void populateDisciplinas();

    /**
     * @brief Popula a lista de ocorrências da disciplina selecionada.
     */
    void populateOcorrencias();

    /**
     * @brief Preenche os horários letivos disponíveis para a turma no dia selecionado.
     */
    void populateHorarios();

    /**
     * @brief Preenche as salas disponíveis considerando o tipo de disciplina (teórica vs laboratório).
     */
    void populateSalas();

    /**
     * @brief Atualiza o card de resumo informativo com turmas, professores, duração e tipo.
     */
    void updateSummaryCard();

    /**
     * @brief Executa a auditoria completa de choques e atualiza o banner visual de status.
     */
    void updateValidationStatus();

    /**
     * @brief Realiza a verificação de conflitos de horário com turmas, professores e salas.
     * @return Vetor de mensagens descrevendo conflitos detectados.
     */
    std::vector<std::string> verificarConflitos() const;

    model::InstitutionData& data_;
    model::DayOfWeek default_day_;
    model::TimeInterval default_interval_;
    std::string default_turma_id_;
    std::string default_professor_id_;
    std::string default_room_id_;

    QCheckBox* chk_filtro_turma_{nullptr};
    QCheckBox* chk_apenas_pendentes_{nullptr};
    QComboBox* combo_disciplina_{nullptr};
    QLabel* lbl_disciplina_info_{nullptr};
    QComboBox* combo_ocorrencia_{nullptr};
    QComboBox* combo_dia_{nullptr};
    QComboBox* combo_horario_{nullptr};
    QComboBox* combo_sala_{nullptr};
    QComboBox* combo_semestre_{nullptr};
    QCheckBox* chk_fixar_{nullptr};
    QLabel* lbl_conflito_status_{nullptr};

    QPushButton* btn_alocar_{nullptr};
    QPushButton* btn_cancelar_{nullptr};

    model::AllocatedLesson result_allocation_;
    bool is_pinned_{true};
};

} // namespace scheduler::ui
