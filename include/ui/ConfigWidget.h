#pragma once

#include "model/OptimizationConfig.h"
#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>

namespace scheduler::ui {

class ConfigWidget : public QWidget {
    Q_OBJECT

public:
    explicit ConfigWidget(QWidget* parent = nullptr);

    void loadConfig(const model::OptimizationConfig& cfg);
    model::OptimizationConfig getConfig() const;

private slots:
    void updateSliderLabels();

private:
    void setupUi();

    QSlider* slider_janelas_{nullptr};
    QLabel* lbl_val_janelas_{nullptr};

    QSlider* slider_dias_{nullptr};
    QLabel* lbl_val_dias_{nullptr};

    QSlider* slider_pref_horarios_{nullptr};
    QLabel* lbl_val_pref_horarios_{nullptr};

    QSlider* slider_pref_dias_{nullptr};
    QLabel* lbl_val_pref_dias_{nullptr};

    QSlider* slider_lacunas_turma_{nullptr};
    QLabel* lbl_val_lacunas_turma_{nullptr};

    QComboBox* combo_modelo_dias_{nullptr};
    QCheckBox* chk_preenchimento_integral_{nullptr};
    QCheckBox* chk_distribuir_semana_{nullptr};
    QSpinBox* spin_max_aulas_dia_{nullptr};

    QComboBox* combo_algoritmo_{nullptr};
    QComboBox* combo_escopo_semestre_{nullptr};
    QCheckBox* chk_sincronizar_anuais_{nullptr};
    QDoubleSpinBox* spin_tempo_limite_{nullptr};
    QSpinBox* spin_threads_{nullptr};
    QCheckBox* chk_log_solver_{nullptr};
    QCheckBox* chk_usar_distribuicao_como_entrada_{nullptr};
};

} // namespace scheduler::ui
