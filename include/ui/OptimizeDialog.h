/**
 * @file OptimizeDialog.h
 * @brief Diálogo para configuração e disparo dos algoritmos de otimização de grade.
 */

#pragma once

#include "model/OptimizationConfig.h"
#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>

namespace scheduler::ui {

/**
 * @class OptimizeDialog
 * @brief Diálogo para seleção do resolvedor (CP-SAT, Guloso, MILP, Genético) e parâmetros de execução.
 *
 * Permite ao usuário escolher o algoritmo, limites de tempo computacional, paralelismo,
 * escopo de semestres letivos e reutilização de grade pré-existente como ponto de partida (warm-start).
 */
class OptimizeDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constrói a janela de parametrização da otimização.
     * @param current_config Configurações de otimização atuais.
     * @param num_aulas_em_tela Quantidade de aulas já alocadas na tela (para warm-start).
     * @param parent Widget pai na hierarquia Qt.
     */
    explicit OptimizeDialog(const model::OptimizationConfig& current_config,
                            int num_aulas_em_tela = 0,
                            QWidget* parent = nullptr);

    /**
     * @brief Retorna o algoritmo selecionado pelo usuário no diálogo.
     * @return model::SolverAlgorithm escolhido.
     */
    model::SolverAlgorithm getSelectedAlgorithm() const;

    /**
     * @brief Atualiza a estrutura de configuração fornecida com as opções marcadas na tela.
     * @param cfg Estrutura de configuração que receberá os novos valores.
     */
    void updateConfig(model::OptimizationConfig& cfg) const;

private slots:
    /**
     * @brief Atualiza o painel descritivo e as opções ativas quando o algoritmo muda.
     * @param index Índice do item selecionado no combo box de algoritmos.
     */
    void onAlgorithmChanged(int index);

private:
    /**
     * @brief Monta a interface gráfica, grupos de campos e cartões explicativos.
     */
    void setupUi();

    /**
     * @brief Atualiza as propriedades e garantias teóricas exibidas sobre o algoritmo selecionado.
     * @param algo Algoritmo de resolução.
     */
    void updateAlgorithmDetails(model::SolverAlgorithm algo);
    void setTimeLimitInputsEnabled(bool enabled);

    model::OptimizationConfig initial_config_;
    int num_aulas_em_tela_{0};

    QComboBox* combo_algorithm_{nullptr};
    QLabel* lbl_algo_desc_{nullptr};
    QLabel* lbl_algo_type_{nullptr};
    QLabel* lbl_algo_guarantee_{nullptr};
    QLabel* lbl_algo_speed_{nullptr};

    QSpinBox* spin_time_hours_{nullptr};
    QSpinBox* spin_time_minutes_{nullptr};
    QSpinBox* spin_time_seconds_{nullptr};
    QCheckBox* chk_sem_limite_tempo_{nullptr};
    QSpinBox* spin_mem_limit_{nullptr};
    QCheckBox* chk_sem_limite_memoria_{nullptr};
    QSpinBox* spin_threads_{nullptr};
    QCheckBox* chk_log_{nullptr};

    QComboBox* combo_escopo_semestre_{nullptr};
    QCheckBox* chk_sincronizar_anuais_{nullptr};

    QCheckBox* chk_usar_grade_atual_{nullptr};
    QLabel* lbl_grade_atual_info_{nullptr};
};

} // namespace scheduler::ui
