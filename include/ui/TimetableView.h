#pragma once

#include "model/InstitutionData.h"
#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QLabel>

#include <QLineEdit>

namespace scheduler::ui {

class TimetableView : public QWidget {
    Q_OBJECT

public:
    explicit TimetableView(model::InstitutionData& data, QWidget* parent = nullptr);

    void refreshView();

signals:
    void lessonPinChanged(const QString& aula_id, bool pinned);
    void requestReoptimization();

private slots:
    void onViewTypeChanged(int index);
    void onTargetEntityChanged(int index);
    void onSemestreFiltroChanged(int index);
    void onCellContextMenu(const QPoint& pos);
    void onSearchLessonChanged(const QString& query);

private:
    void setupUi();
    void populateTargetCombo();
    void renderTimetable();

    model::InstitutionData& data_;
    QComboBox* combo_view_type_{nullptr};
    QComboBox* combo_target_{nullptr};
    QComboBox* combo_semestre_filtro_{nullptr};
    QLabel* lbl_info_banner_{nullptr};
    QLineEdit* search_lesson_{nullptr};
    QLabel* lbl_search_count_{nullptr};
    QTableWidget* table_{nullptr};
};

} // namespace scheduler::ui
