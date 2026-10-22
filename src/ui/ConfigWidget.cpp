#include "ui/ConfigWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>

namespace scheduler::ui {

ConfigWidget::ConfigWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void ConfigWidget::setupUi() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(16, 16, 16, 16);
    main_layout->setSpacing(16);

    // 1. Pesos Institucionais da Otimização (Seção 4 & 8)
    auto* grp_pesos = new QGroupBox("Pesos Institucionais da Otimização (Lambda_i) - Seção 4 & 8", this);
    auto* lay_pesos = new QVBoxLayout(grp_pesos);
    lay_pesos->setSpacing(12);

    auto make_slider_row = [this, grp_pesos](const QString& title, const QString& tooltip,
                                             QSlider*& slider, QLabel*& lbl_val, int initial) {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(title, grp_pesos);
        lbl->setToolTip(tooltip);
        lbl->setFixedWidth(300);
        lbl->setStyleSheet("font-weight: 500;");
        row->addWidget(lbl);

        slider = new QSlider(Qt::Horizontal, grp_pesos);
        slider->setRange(0, 20);
        slider->setValue(initial);
        slider->setTickInterval(2);
        slider->setTickPosition(QSlider::TicksBelow);
        row->addWidget(slider);

        lbl_val = new QLabel(QString::number(initial), grp_pesos);
        lbl_val->setFixedWidth(35);
        lbl_val->setStyleSheet("font-weight: bold; color: #38bdf8; font-size: 14px;");
        row->addWidget(lbl_val);

        connect(slider, &QSlider::valueChanged, this, &ConfigWidget::updateSliderLabels);
        return row;
    };

    lay_pesos->addLayout(make_slider_row("Reduzir janelas entre aulas dos professores (λ1):",
                                         "Penaliza tempo ocioso entre a primeira e a última aula de cada professor.",
                                         slider_janelas_, lbl_val_janelas_, 8));

    lay_pesos->addLayout(make_slider_row("Concentrar aulas em menos dias presenciais (λ2):",
                                         "Favorece folgas semanais completas para os professores.",
                                         slider_dias_, lbl_val_dias_, 5));

    lay_pesos->addLayout(make_slider_row("Respeitar horários preferidos no dia (λ3):",
                                         "Evita faixas horárias pouco desejadas.",
                                         slider_pref_horarios_, lbl_val_pref_horarios_, 3));

    lay_pesos->addLayout(make_slider_row("Priorizar dias preferidos dos professores (λ5):",
                                         "Considera as notas de 0 a 10 atribuídas pelos professores aos dias da semana.",
                                         slider_pref_dias_, lbl_val_pref_dias_, 7));

    lay_pesos->addLayout(make_slider_row("Reduzir lacunas na grade das turmas (λ4):",
                                         "Compacta os horários das turmas eliminando aulas vagas intermediárias.",
                                         slider_lacunas_turma_, lbl_val_lacunas_turma_, 2));

    main_layout->addWidget(grp_pesos);

    // 2. Semântica da Preferência e Regras Acadêmicas
    auto* grp_regras = new QGroupBox("Semântica da Preferência e Regras Acadêmicas (Seção 4.4 & 5)", this);
    auto* form_regras = new QFormLayout(grp_regras);
    form_regras->setSpacing(10);

    combo_modelo_dias_ = new QComboBox(grp_regras);
    combo_modelo_dias_->addItem("Por Dia Trabalhado [Padrão: W = sum (10 - ω_p,d) * y_p,d]",
                                static_cast<int>(model::ModeloPreferenciaDias::POR_DIA_TRABALHADO));
    combo_modelo_dias_->addItem("Por Minuto de Aula [Alternativa: W_min = sum (10 - ω_p,d) * m_p,d]",
                                static_cast<int>(model::ModeloPreferenciaDias::POR_MINUTO_AULA));
    form_regras->addRow("Modelo de Preferência Diária:", combo_modelo_dias_);

    chk_preenchimento_integral_ = new QCheckBox("Preenchimento integral da jornada da turma (Seção 3.10)", grp_regras);
    chk_preenchimento_integral_->setToolTip("Exige que todos os minutos letivos sejam ocupados por aulas, sem períodos vagos.");
    form_regras->addRow("", chk_preenchimento_integral_);

    chk_distribuir_semana_ = new QCheckBox("Distribuir matérias na semana (evitar aulas em excesso no mesmo dia)", grp_regras);
    chk_distribuir_semana_->setChecked(true);
    form_regras->addRow("", chk_distribuir_semana_);

    spin_max_aulas_dia_ = new QSpinBox(grp_regras);
    spin_max_aulas_dia_->setRange(1, 6);
    spin_max_aulas_dia_->setValue(2);
    spin_max_aulas_dia_->setSuffix(" aulas diárias por matéria para a turma");
    form_regras->addRow("Limite Diário por Disciplina:", spin_max_aulas_dia_);

    main_layout->addWidget(grp_regras);

    // 3. Parâmetros do Solver e Algoritmo
    auto* grp_solver = new QGroupBox("Configurações do Algoritmo e Parâmetros de Execução", this);
    auto* form_solver = new QFormLayout(grp_solver);
    form_solver->setSpacing(10);

    combo_algoritmo_ = new QComboBox(grp_solver);
    combo_algoritmo_->addItem("Google OR-Tools CP-SAT (Restrições / Exato - Recomendado)",
                              static_cast<int>(model::SolverAlgorithm::CP_SAT));
    combo_algoritmo_->addItem("Heurística Construtiva Gulosa (Rápido / Prioridade)",
                              static_cast<int>(model::SolverAlgorithm::GREEDY_HEURISTIC));
    combo_algoritmo_->addItem("Programação Linear Inteira Mista (MILP)",
                              static_cast<int>(model::SolverAlgorithm::MILP));
    combo_algoritmo_->addItem("Algoritmo Genético (Meta-heurística)",
                              static_cast<int>(model::SolverAlgorithm::GENETIC_ALGORITHM));
    form_solver->addRow("Algoritmo de Otimização Padrão:", combo_algoritmo_);

    spin_tempo_limite_ = new QDoubleSpinBox(grp_solver);
    spin_tempo_limite_->setRange(1.0, 3600.0);
    spin_tempo_limite_->setValue(30.0);
    spin_tempo_limite_->setSuffix(" segundos");
    form_solver->addRow("Tempo Limite de Otimização:", spin_tempo_limite_);

    spin_threads_ = new QSpinBox(grp_solver);
    spin_threads_->setRange(1, 64);
    spin_threads_->setValue(4);
    spin_threads_->setSuffix(" threads (processamento paralelo)");
    form_solver->addRow("Número de Threads:", spin_threads_);

    chk_log_solver_ = new QCheckBox("Habilitar log de busca em tempo real do CP-SAT", grp_solver);
    form_solver->addRow("", chk_log_solver_);

    main_layout->addWidget(grp_solver);
    main_layout->addStretch();
}

void ConfigWidget::updateSliderLabels() {
    lbl_val_janelas_->setText(QString::number(slider_janelas_->value()));
    lbl_val_dias_->setText(QString::number(slider_dias_->value()));
    lbl_val_pref_horarios_->setText(QString::number(slider_pref_horarios_->value()));
    lbl_val_pref_dias_->setText(QString::number(slider_pref_dias_->value()));
    lbl_val_lacunas_turma_->setText(QString::number(slider_lacunas_turma_->value()));
}

void ConfigWidget::loadConfig(const model::OptimizationConfig& cfg) {
    slider_janelas_->setValue(cfg.lambda_janelas);
    slider_dias_->setValue(cfg.lambda_dias);
    slider_pref_horarios_->setValue(cfg.lambda_preferencias);
    slider_pref_dias_->setValue(cfg.lambda_preferencia_dias);
    slider_lacunas_turma_->setValue(cfg.lambda_lacunas_turma);

    int idx_dias = combo_modelo_dias_->findData(static_cast<int>(cfg.modelo_preferencia_dias));
    if (idx_dias >= 0) combo_modelo_dias_->setCurrentIndex(idx_dias);

    chk_preenchimento_integral_->setChecked(cfg.preenchimento_integral_jornada);
    chk_distribuir_semana_->setChecked(cfg.distribuir_disciplinas_na_semana);
    spin_max_aulas_dia_->setValue(cfg.max_aulas_disciplina_dia_turma);

    int idx_algo = combo_algoritmo_->findData(static_cast<int>(cfg.algoritmo));
    if (idx_algo >= 0) combo_algoritmo_->setCurrentIndex(idx_algo);

    spin_tempo_limite_->setValue(cfg.tempo_limite_segundos);
    spin_threads_->setValue(cfg.num_threads);
    chk_log_solver_->setChecked(cfg.log_search_progress);

    updateSliderLabels();
}

model::OptimizationConfig ConfigWidget::getConfig() const {
    model::OptimizationConfig cfg;
    cfg.lambda_janelas = slider_janelas_->value();
    cfg.lambda_dias = slider_dias_->value();
    cfg.lambda_preferencias = slider_pref_horarios_->value();
    cfg.lambda_preferencia_dias = slider_pref_dias_->value();
    cfg.lambda_lacunas_turma = slider_lacunas_turma_->value();

    cfg.modelo_preferencia_dias = static_cast<model::ModeloPreferenciaDias>(
        combo_modelo_dias_->currentData().toInt());

    cfg.preenchimento_integral_jornada = chk_preenchimento_integral_->isChecked();
    cfg.distribuir_disciplinas_na_semana = chk_distribuir_semana_->isChecked();
    cfg.max_aulas_disciplina_dia_turma = spin_max_aulas_dia_->value();

    cfg.algoritmo = static_cast<model::SolverAlgorithm>(
        combo_algoritmo_->currentData().toInt());

    cfg.tempo_limite_segundos = spin_tempo_limite_->value();
    cfg.num_threads = spin_threads_->value();
    cfg.log_search_progress = chk_log_solver_->isChecked();

    return cfg;
}

} // namespace scheduler::ui
