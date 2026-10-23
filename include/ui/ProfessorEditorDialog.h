#pragma once

#include "model/Professor.h"
#include <QDialog>
#include <QLineEdit>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>
#include <QTableWidget>
#include <QCheckBox>
#include <array>

namespace scheduler::ui {

class ProfessorEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit ProfessorEditorDialog(QWidget* parent = nullptr,
                                   const model::Professor* prof = nullptr,
                                   const std::string& default_id = "");

    model::Professor getProfessor() const;

private slots:
    void onSliderValueChanged();
    void onSliderJanelaChanged();
    void onAddInterval();
    void onRemoveInterval();
    void onSetDefaultMorning();
    void onSetDefaultAfternoon();
    void onSetDefaultNight();
    void onSetDefaultAllDay();
    void onSetDefaultWholeDay();
    void onClearAllIntervals();
    void onSelectAllShifts();
    void onSelectMorningOnly();
    void onSelectMorningAfternoon();

private:
    void setupUi();
    void loadProfessor(const model::Professor& prof);

    std::string id_{"P01"};
    QLineEdit* txt_nome_{nullptr};
    std::array<QSlider*, 5> sliders_dias_{};
    std::array<QLabel*, 5> lbls_valores_dias_{};
    std::array<QLabel*, 5> lbls_desc_dias_{};
    
    // Vontade individual do professor quanto a janelas
    QSlider* slider_peso_janela_{nullptr};
    QLabel* lbl_val_peso_janela_{nullptr};
    QLabel* lbl_desc_peso_janela_{nullptr};

    // Turnos pretendidos por dia da semana (5 dias x 3 turnos: 0=Manhã, 1=Tarde, 2=Noite)
    std::array<std::array<QCheckBox*, 3>, 5> check_turnos_dias_{};

    QTableWidget* table_disp_{nullptr};
    QSpinBox* spin_carga_max_{nullptr};
};

} // namespace scheduler::ui
