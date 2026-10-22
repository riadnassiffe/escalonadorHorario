#pragma once

#include "model/Professor.h"
#include <QDialog>
#include <QLineEdit>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>
#include <QTableWidget>
#include <array>

namespace scheduler::ui {

class ProfessorEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProfessorEditorDialog(QWidget* parent = nullptr,
                                  const model::Professor* prof = nullptr);

    model::Professor getProfessor() const;

private slots:
    void onSliderValueChanged();
    void onAddInterval();
    void onRemoveInterval();
    void onSetDefaultMorning();
    void onSetDefaultAllDay();

private:
    void setupUi();
    void loadProfessor(const model::Professor& prof);

    QLineEdit* txt_id_{nullptr};
    QLineEdit* txt_nome_{nullptr};
    std::array<QSlider*, 5> sliders_dias_{};
    std::array<QLabel*, 5> lbls_valores_dias_{};
    std::array<QLabel*, 5> lbls_desc_dias_{};
    QTableWidget* table_disp_{nullptr};
    QSpinBox* spin_carga_max_{nullptr};
};

} // namespace scheduler::ui
