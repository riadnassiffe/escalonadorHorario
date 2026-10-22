#pragma once

#include "model/Lesson.h"
#include "model/Professor.h"
#include "model/Turma.h"
#include "model/Room.h"
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QListWidget>
#include <QCheckBox>
#include <vector>

namespace scheduler::ui {

class LessonEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit LessonEditorDialog(const std::vector<model::Professor>& profs,
                                const std::vector<model::Turma>& turmas,
                                const std::vector<model::Room>& salas,
                                QWidget* parent = nullptr,
                                const model::Disciplina* disc = nullptr);

    model::Disciplina getDisciplina() const;

private slots:
    void onTipoChanged(int index);
    void onRegimeChanged(int index);

private:
    void setupUi();
    void loadDisciplina(const model::Disciplina& disc);

    const std::vector<model::Professor>& profs_;
    const std::vector<model::Turma>& turmas_;
    const std::vector<model::Room>& salas_;

    QLineEdit* txt_id_{nullptr};
    QLineEdit* txt_nome_{nullptr};
    QComboBox* combo_tipo_{nullptr};
    QComboBox* combo_regime_{nullptr};
    QComboBox* combo_semestre_{nullptr};
    QComboBox* combo_duracao_{nullptr};
    QSpinBox* spin_aulas_semanais_{nullptr};
    QComboBox* combo_prof_{nullptr};
    QComboBox* combo_turma_{nullptr};
    QListWidget* list_salas_alternativas_{nullptr};
    QWidget* widget_salas_alternativas_{nullptr};
    std::vector<QCheckBox*> check_turnos_;
    QCheckBox* check_consecutivas_{nullptr};
    QSpinBox* spin_max_dia_{nullptr};
};

} // namespace scheduler::ui
