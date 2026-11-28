/**
 * @file ConfigWidget.h
 * @brief Painel de preferências e parâmetros globais de otimização da grade horária.
 */

#pragma once

#include "model/OptimizationConfig.h"
#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>

namespace scheduler::ui {

/**
 * @class ConfigWidget
 * @brief Widget de interface para configuração de pesos institucionais e opções do resolvedor.
 *
 * Oferece controles (sliders e comboboxes) para pesos de penalidade (janelas, concentração de dias,
 * preferências de horários, turnos e dias da semana), modelo de preferência de dias (W vs D),
 * e parâmetros computacionais (algoritmo, threads e timeout).
 */
class ConfigWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constrói o painel de configurações.
     * @param parent Widget pai na hierarquia Qt.
     */
    explicit ConfigWidget(QWidget* parent = nullptr);

    /**
     * @brief Carrega os valores de uma estrutura OptimizationConfig nos controles da tela.
     * @param cfg Estrutura com os parâmetros de otimização.
     */
    void loadConfig(const model::OptimizationConfig& cfg);

    /**
     * @brief Constrói e retorna uma nova model::OptimizationConfig a partir dos controles ativos.
     * @return model::OptimizationConfig preenchida.
     */
    model::OptimizationConfig getConfig() const;

private slots:
    /**
     * @brief Atualiza os rótulos numéricos ao lado dos sliders de ponderação.
     */
    void updateSliderLabels();

private:
    /**
     * @brief Constrói e posiciona todos os grupos de opções na interface.
     */
    void setupUi();

    QSlider* slider_janelas_{nullptr};
    QLabel* lbl_val_janelas_{nullptr};

    QSlider* slider_dias_{nullptr};
    QLabel* lbl_val_dias_{nullptr};

    QSlider* slider_pref_horarios_{nullptr};
    QLabel* lbl_val_pref_horarios_{nullptr};

    QSlider* slider_pref_dias_{nullptr};
    QLabel* lbl_val_pref_dias_{nullptr};

    QSlider* slider_pref_turnos_{nullptr};
    QLabel* lbl_val_pref_turnos_{nullptr};

    QSlider* slider_lacunas_turma_{nullptr};
    QLabel* lbl_val_lacunas_turma_{nullptr};

    QSlider* slider_aulas_consecutivas_{nullptr};
    QLabel* lbl_val_aulas_consecutivas_{nullptr};

    QComboBox* combo_modelo_dias_{nullptr};
    QCheckBox* chk_preenchimento_integral_{nullptr};
    QCheckBox* chk_distribuir_semana_{nullptr};
    QCheckBox* chk_consecutivas_rigido_{nullptr};

    QComboBox* combo_algoritmo_{nullptr};
    QComboBox* combo_escopo_semestre_{nullptr};
    QCheckBox* chk_sincronizar_anuais_{nullptr};
    QSpinBox* spin_tempo_horas_{nullptr};
    QSpinBox* spin_tempo_minutos_{nullptr};
    QSpinBox* spin_tempo_segundos_{nullptr};
    QCheckBox* chk_sem_limite_tempo_{nullptr};
    QSpinBox* spin_max_memoria_{nullptr};
    QCheckBox* chk_sem_limite_memoria_{nullptr};
    QSpinBox* spin_threads_{nullptr};
    QCheckBox* chk_log_solver_{nullptr};
    QCheckBox* chk_usar_distribuicao_como_entrada_{nullptr};
};

} // namespace scheduler::ui
