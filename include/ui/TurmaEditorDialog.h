#pragma once

#include "model/Turma.h"
#include "model/Room.h"
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QTimeEdit>
#include <QRadioButton>
#include <QListWidget>
#include <QLabel>
#include <vector>

namespace scheduler::ui {

class TurmaEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit TurmaEditorDialog(const std::vector<model::Room>& salas_disponiveis,
                               QWidget* parent = nullptr,
                               const model::Turma* turma = nullptr);

    model::Turma getTurma() const;

private slots:
    void validateJornada();
    void onSalaModeChanged();

private:
    void setupUi();
    void loadTurma(const model::Turma& turma);

    const std::vector<model::Room>& salas_disponiveis_;

    QLineEdit* txt_id_{nullptr};
    QLineEdit* txt_nome_{nullptr};
    QComboBox* combo_turno_{nullptr};
    std::vector<QCheckBox*> check_dias_;

    QTimeEdit* time_inicio_{nullptr};
    QTimeEdit* time_fim_{nullptr};
    QCheckBox* chk_int1_{nullptr};
    QTimeEdit* time_int1_ini_{nullptr};
    QTimeEdit* time_int1_fim_{nullptr};
    QCheckBox* chk_int2_{nullptr};
    QTimeEdit* time_int2_ini_{nullptr};
    QTimeEdit* time_int2_fim_{nullptr};
    QCheckBox* chk_int3_{nullptr};
    QTimeEdit* time_int3_ini_{nullptr};
    QTimeEdit* time_int3_fim_{nullptr};
    QLabel* lbl_status_jornada_{nullptr};

    QRadioButton* rad_sala_fixa_{nullptr};
    QComboBox* combo_sala_fixa_{nullptr};
    QRadioButton* rad_sala_opt_{nullptr};
    QListWidget* list_salas_permitidas_{nullptr};
};

} // namespace scheduler::ui
