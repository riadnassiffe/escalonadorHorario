#pragma once

#include "model/OptimizationConfig.h"
#include <QDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>

namespace scheduler::ui {

class OptimizeDialog : public QDialog {
    Q_OBJECT

public:
    explicit OptimizeDialog(const model::OptimizationConfig& current_config,
                            int num_aulas_em_tela = 0,
                            QWidget* parent = nullptr);

    model::SolverAlgorithm getSelectedAlgorithm() const;
    void updateConfig(model::OptimizationConfig& cfg) const;

private slots:
    void onAlgorithmChanged(int index);

private:
    void setupUi();
    void updateAlgorithmDetails(model::SolverAlgorithm algo);

    model::OptimizationConfig initial_config_;
    int num_aulas_em_tela_{0};

    QComboBox* combo_algorithm_{nullptr};
    QLabel* lbl_algo_desc_{nullptr};
    QLabel* lbl_algo_type_{nullptr};
    QLabel* lbl_algo_guarantee_{nullptr};
    QLabel* lbl_algo_speed_{nullptr};

    QDoubleSpinBox* spin_time_limit_{nullptr};
    QSpinBox* spin_threads_{nullptr};
    QCheckBox* chk_log_{nullptr};

    QCheckBox* chk_usar_grade_atual_{nullptr};
    QLabel* lbl_grade_atual_info_{nullptr};
};

} // namespace scheduler::ui
