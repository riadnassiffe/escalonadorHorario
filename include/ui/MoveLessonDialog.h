#pragma once

#include "model/InstitutionData.h"
#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>

namespace scheduler::ui {

/**
 * @brief Diálogo moderno para movimentação e reatribuição manual de horário de uma aula.
 * Exibe prévia dos dados da disciplina, seleção de dia/horário e validação de conflitos em tempo real.
 */
class MoveLessonDialog : public QDialog {
    Q_OBJECT

public:
    MoveLessonDialog(model::InstitutionData& data,
                     const std::string& aula_id,
                     const std::vector<model::TimeInterval>& available_slots,
                     QWidget* parent = nullptr);

    model::DayOfWeek selectedDay() const;
    model::TimeInterval selectedInterval() const;
    std::string selectedRoomId() const;

private slots:
    void onSelectionChanged();

private:
    void setupUi();
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
