/**
 * @file MoveLessonDialog.h
 * @brief Diálogo para movimentação e reatribuição manual de horários e salas de aulas.
 */

#pragma once

#include "model/InstitutionData.h"
#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

namespace scheduler::ui {

/**
 * @class MoveLessonDialog
 * @brief Diálogo moderno para movimentação e reatribuição manual de horário de uma aula.
 *
 * Exibe prévia dos dados da disciplina, seleção de dia/horário/sala e validação
 * de conflitos (choques com turmas, professores, salas e intervalos) em tempo real.
 */
class MoveLessonDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constrói o diálogo para movimentação manual de aula.
     * @param data Referência para a base de dados acadêmicos.
     * @param aula_id ID da aula a ser movida.
     * @param available_slots Vetor de intervalos de tempo válidos da instituição.
     * @param parent Widget pai na hierarquia Qt.
     */
    MoveLessonDialog(model::InstitutionData& data,
                     const std::string& aula_id,
                     const std::vector<model::TimeInterval>& available_slots,
                     QWidget* parent = nullptr);

    /**
     * @brief Retorna o dia da semana selecionado para a nova alocação.
     * @return model::DayOfWeek correspondente.
     */
    model::DayOfWeek selectedDay() const;

    /**
     * @brief Retorna o intervalo de tempo selecionado para a aula.
     * @return model::TimeInterval do novo horário.
     */
    model::TimeInterval selectedInterval() const;

    /**
     * @brief Retorna o ID da sala de aula selecionada.
     * @return std::string com o ID da sala.
     */
    std::string selectedRoomId() const;

private slots:
    /**
     * @brief Slot acionado sempre que o dia, horário ou sala forem alterados na interface.
     */
    void onSelectionChanged();

private:
    /**
     * @brief Inicializa e monta a interface visual com os campos e botões.
     */
    void setupUi();

    /**
     * @brief Realiza a validação cruzada do novo horário e atualiza as mensagens de conflito.
     */
    void updateValidationStatus();

    model::InstitutionData& data_;
    std::string aula_id_;
    std::vector<model::TimeInterval> available_slots_;

    QComboBox* combo_dia_{nullptr};
    QComboBox* combo_horario_{nullptr};
    QComboBox* combo_sala_{nullptr};
    QLabel* lbl_conflito_status_{nullptr};
    QPushButton* btn_mover_{nullptr};
};

} // namespace scheduler::ui
