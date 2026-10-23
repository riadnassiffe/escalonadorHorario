#pragma once

#include "model/InstitutionData.h"
#include "ui/TimetableGridWidget.h"
#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <vector>

namespace scheduler::ui {

class TimetableView : public QWidget {
    Q_OBJECT

public:
    explicit TimetableView(model::InstitutionData& data, QWidget* parent = nullptr);

    void refreshView();

signals:
    void lessonPinChanged(const QString& aula_id, bool pinned);
    void lessonMoved(const QString& aula_id, const QString& details);
    void timetableModified();
    void requestReoptimization();

public slots:
    void onLessonDropRequested(const QString& aula_id, int target_row, int target_col);
    void moverAulaManual(const QString& aula_id);
    void mudarSalaManual(const QString& aula_id);

private slots:
    void onViewTypeChanged(int index);
    void onTargetEntityChanged(int index);
    void onSemestreFiltroChanged(int index);
    void onCellContextMenu(const QPoint& pos);
    void onCellDoubleClicked(int row, int col);
    void onSearchLessonChanged(const QString& query);

private:
    void setupUi();
    void populateTargetCombo();
    void renderTimetable();

    std::vector<std::string> verificarConflitosMovimentacao(const std::string& aid,
                                                            model::DayOfWeek dia,
                                                            const model::TimeInterval& intervalo) const;

    model::InstitutionData& data_;
    QComboBox* combo_view_type_{nullptr};
    QComboBox* combo_target_{nullptr};
    QComboBox* combo_semestre_filtro_{nullptr};
    QLabel* lbl_info_banner_{nullptr};
    QLineEdit* search_lesson_{nullptr};
    QLabel* lbl_search_count_{nullptr};
    TimetableGridWidget* table_{nullptr};

    std::vector<model::TimeInterval> row_intervals_;
};

} // namespace scheduler::ui

