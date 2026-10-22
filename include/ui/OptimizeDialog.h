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
    explicit OptimizeDialog(const model::OptimizationConfig& current_config, QWidget* parent = nullptr);

    model::SolverAlgorithm getSelectedAlgorithm() const;
    void updateConfig(model::OptimizationConfig& cfg) const;

private slots:
    void onAlgorithmChanged(int index);

private:
    void setupUi();
    void updateAlgorithmDetails(model::SolverAlgorithm algo);

    model::OptimizationConfig initial_config_;

    QComboBox* combo_algorithm_{nullptr};
    QLabel* lbl_algo_desc_{nullptr};
    QLabel* lbl_algo_type_{nullptr};
    QLabel* lbl_algo_guarantee_{nullptr};
    QLabel* lbl_algo_speed_{nullptr};

    QDoubleSpinBox* spin_time_limit_{nullptr};
    QSpinBox* spin_threads_{nullptr};
    QCheckBox* chk_log_{nullptr};
};

} // namespace scheduler::ui
