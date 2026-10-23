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
#include <QTabWidget>
#include <vector>
#include <array>

namespace scheduler::ui {

struct DayScheduleWidgets {
    QCheckBox* chk_dia_ativo{nullptr};
    QTimeEdit* time_inicio{nullptr};
    QTimeEdit* time_fim{nullptr};
    QCheckBox* chk_int1{nullptr};
    QTimeEdit* time_int1_ini{nullptr};
    QTimeEdit* time_int1_fim{nullptr};
    QCheckBox* chk_int2{nullptr};
    QTimeEdit* time_int2_ini{nullptr};
    QTimeEdit* time_int2_fim{nullptr};
    QCheckBox* chk_int3{nullptr};
    QTimeEdit* time_int3_ini{nullptr};
    QTimeEdit* time_int3_fim{nullptr};
    QLabel* lbl_status{nullptr};
    QWidget* content_panel{nullptr};
};

class TurmaEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit TurmaEditorDialog(const std::vector<model::Room>& salas_disponiveis,
                               QWidget* parent = nullptr,
                               const model::Turma* turma = nullptr);

    model::Turma getTurma() const;

private slots:
    void validateAllDays();
    void onSalaModeChanged();

private:
    void setupUi();
    void loadTurma(const model::Turma& turma);
    QWidget* createDayTab(int day_idx);
    void applyPresetToDay(int day_idx, model::ShiftType preset_type);
    void applyPresetToAllDays(model::ShiftType preset_type);
    void copyDayScheduleToOthers(int source_day_idx);

    static model::JornadaTurma getJornadaFromWidgets(const DayScheduleWidgets& w);
    static void setWidgetsFromJornada(DayScheduleWidgets& w, const model::JornadaTurma& j);

    const std::vector<model::Room>& salas_disponiveis_;

    QLineEdit* txt_id_{nullptr};
    QLineEdit* txt_nome_{nullptr};
    QComboBox* combo_turno_{nullptr};

    std::array<DayScheduleWidgets, 5> days_widgets_{};
    QTabWidget* tab_dias_{nullptr};
    QLabel* lbl_resumo_semanal_{nullptr};

    QRadioButton* rad_sala_fixa_{nullptr};
    QComboBox* combo_sala_fixa_{nullptr};
    QRadioButton* rad_sala_opt_{nullptr};
    QListWidget* list_salas_permitidas_{nullptr};
};

} // namespace scheduler::ui
