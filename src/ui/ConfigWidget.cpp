/**
 * @file ConfigWidget.cpp
 * @brief Implementação do painel de configuração e preferências globais do escalonador.
 */

#include "ui/ConfigWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>

namespace scheduler::ui {

/**
 * @brief Construtor do widget de configurações.
 * @param parent Widget pai na hierarquia Qt.
 */
ConfigWidget::ConfigWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
}

/**
 * @brief Constrói os grupos de pesos, parâmetros acadêmicos e do solver.
 */
void ConfigWidget::setupUi() {
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(0, 0, 0, 0);

    auto* scroll_area = new QScrollArea(this);
    scroll_area->setWidgetResizable(true);
    scroll_area->setFrameShape(QFrame::NoFrame);
    scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll_area->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto* content_widget = new QWidget(scroll_area);
    auto* main_layout = new QVBoxLayout(content_widget);
    main_layout->setContentsMargins(16, 16, 16, 16);
    main_layout->setSpacing(16);

    // 1. Pesos Institucionais da Otimização (Seção 4 & 8)
    auto* grp_pesos = new QGroupBox("Pesos Institucionais da Otimização (Lambda_i) - Seção 4 & 8", content_widget);
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
        lbl_val->setStyleSheet("font-weight: bold; color: palette(highlight); font-size: 14px;");
        row->addWidget(lbl_val);

        connect(slider, &QSlider::valueChanged, this, &ConfigWidget::updateSliderLabels);
        return row;
    };

    lay_pesos->addLayout(make_slider_row("Reduzir janelas entre aulas dos professores (λ1):",
                                         "Penaliza minutos ociosos entre a primeira e a última aula de cada professor no turno.",
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

    lay_pesos->addLayout(make_slider_row("Priorizar turnos pretendidos dos professores (λ6):",
                                         "Penaliza quando um professor é alocado em turno não pretendido no dia.",
                                         slider_pref_turnos_, lbl_val_pref_turnos_, 8));

    lay_pesos->addLayout(make_slider_row("Reduzir lacunas na grade das turmas (λ4):",
                                         "Compacta os horários das turmas eliminando aulas vagas intermediárias.",
                                         slider_lacunas_turma_, lbl_val_lacunas_turma_, 2));

    main_layout->addWidget(grp_pesos);

    // 2. Semântica da Preferência e Regras Acadêmicas
    auto* grp_regras = new QGroupBox("Semântica da Preferência e Regras Acadêmicas (Seção 4.4 & 5)", content_widget);
    auto* form_regras = new QFormLayout(grp_regras);
    form_regras->setSpacing(10);

    combo_modelo_dias_ = new QComboBox(grp_regras);
    combo_modelo_dias_->addItem("Por Dia Trabalhado [Padrão: W = sum (10 - ω_p,d) * y_p,d]",
                                static_cast<int>(model::ModeloPreferenciaDias::POR_DIA_TRABALHADO));
    combo_modelo_dias_->addItem("Por Minuto de Aula [Alternativa: W_min = sum (10 - ω_p,d) * m_p,d]",
                                static_cast<int>(model::ModeloPreferenciaDias::POR_MINUTO_AULA));
    form_regras->addRow("Modelo de Preferência Diária:", combo_modelo_dias_);

    chk_preenchimento_integral_ = new QCheckBox("Preenchimento integral dos slots letivos da turma (R10)", grp_regras);
    chk_preenchimento_integral_->setToolTip("Exige que todos os slots letivos cadastrados para a turma sejam ocupados por aulas.");
    form_regras->addRow("", chk_preenchimento_integral_);

    chk_distribuir_semana_ = new QCheckBox("Distribuir matérias na semana (evitar aulas em excesso no mesmo dia)", grp_regras);
    chk_distribuir_semana_->setChecked(true);
    form_regras->addRow("", chk_distribuir_semana_);

    spin_max_aulas_dia_ = new QSpinBox(grp_regras);
    spin_max_aulas_dia_->setRange(1, 6);
    spin_max_aulas_dia_->setValue(2);
    spin_max_aulas_dia_->setSuffix(" aulas diárias");
    spin_max_aulas_dia_->setToolTip("Limite de aulas diárias por matéria para a turma");
    spin_max_aulas_dia_->setFixedWidth(140);
    form_regras->addRow("Limite Diário por Disciplina:", spin_max_aulas_dia_);

    main_layout->addWidget(grp_regras);

    // 3. Parâmetros do Solver e Algoritmo
    auto* grp_solver = new QGroupBox("Configurações do Algoritmo e Parâmetros de Execução", content_widget);
    auto* form_solver = new QFormLayout(grp_solver);
    form_solver->setSpacing(12);

    combo_algoritmo_ = new QComboBox(grp_solver);
    combo_algoritmo_->addItem("Google OR-Tools CP-SAT (Restrições / Exato - Recomendado)",
                              static_cast<int>(model::SolverAlgorithm::CP_SAT));
    combo_algoritmo_->addItem("NVIDIA cuOpt - Otimização Matemática Acelerada por GPU (MILP/LP)",
                              static_cast<int>(model::SolverAlgorithm::CUOPT));
    combo_algoritmo_->addItem("SCIP - Solving Constraint Integer Programs (Branch-and-Cut)",
                              static_cast<int>(model::SolverAlgorithm::SCIP));
    combo_algoritmo_->addItem("HiGHS - High Performance Optimization Software (MILP)",
                              static_cast<int>(model::SolverAlgorithm::HIGHS));
    combo_algoritmo_->addItem("Heurística Construtiva Gulosa (Rápido / Prioridade)",
                              static_cast<int>(model::SolverAlgorithm::GREEDY_HEURISTIC));
    combo_algoritmo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    form_solver->addRow("Algoritmo de Otimização Padrão:", combo_algoritmo_);

    combo_escopo_semestre_ = new QComboBox(grp_solver);
    combo_escopo_semestre_->addItem("Todas as Disciplinas (Geral)", static_cast<int>(model::EscopoSemestreOtimizacao::TODAS_SEM_FILTRO));
    combo_escopo_semestre_->addItem("1º Semestre (Anuais + 1º Sem)", static_cast<int>(model::EscopoSemestreOtimizacao::SEMESTRE_1));
    combo_escopo_semestre_->addItem("2º Semestre (Anuais + 2º Sem)", static_cast<int>(model::EscopoSemestreOtimizacao::SEMESTRE_2));
    combo_escopo_semestre_->addItem("Ambos os Semestres (Multi-Semestre Integrado)", static_cast<int>(model::EscopoSemestreOtimizacao::AMBOS_INTEGRADO));
    combo_escopo_semestre_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    form_solver->addRow("Escopo de Semestre Padrão:", combo_escopo_semestre_);

    chk_sincronizar_anuais_ = new QCheckBox("Sincronizar horários de disciplinas anuais entre semestres", grp_solver);
    chk_sincronizar_anuais_->setChecked(true);
    form_solver->addRow("", chk_sincronizar_anuais_);

    auto* lay_tempo = new QHBoxLayout();
    lay_tempo->setSpacing(12);

    spin_tempo_limite_ = new QDoubleSpinBox(grp_solver);
    spin_tempo_limite_->setRange(1.0, 3600.0);
    spin_tempo_limite_->setValue(30.0);
    spin_tempo_limite_->setSuffix(" segundos");
    spin_tempo_limite_->setFixedWidth(140);

    chk_sem_limite_tempo_ = new QCheckBox("Sem restrição de tempo", grp_solver);
    chk_sem_limite_tempo_->setToolTip("Executa o solver sem tempo limite máximo.");

    connect(chk_sem_limite_tempo_, &QCheckBox::toggled, this, [this](bool checked) {
        spin_tempo_limite_->setEnabled(!checked);
    });

    lay_tempo->addWidget(spin_tempo_limite_);
    lay_tempo->addWidget(chk_sem_limite_tempo_);
    lay_tempo->addStretch();
    form_solver->addRow("Tempo Limite de Otimização:", lay_tempo);

    auto* lay_mem = new QHBoxLayout();
    lay_mem->setSpacing(12);

    spin_max_memoria_ = new QSpinBox(grp_solver);
    spin_max_memoria_->setRange(256, 131072);
    spin_max_memoria_->setSingleStep(256);
    spin_max_memoria_->setValue(2048);
    spin_max_memoria_->setSuffix(" MB");
    spin_max_memoria_->setFixedWidth(140);

    chk_sem_limite_memoria_ = new QCheckBox("Sem restrição de memória", grp_solver);
    chk_sem_limite_memoria_->setToolTip("Executa o solver sem impor limite máximo explícito de memória RAM.");

    connect(chk_sem_limite_memoria_, &QCheckBox::toggled, this, [this](bool checked) {
        spin_max_memoria_->setEnabled(!checked);
    });

    lay_mem->addWidget(spin_max_memoria_);
    lay_mem->addWidget(chk_sem_limite_memoria_);
    lay_mem->addStretch();
    form_solver->addRow("Limite de Memória RAM:", lay_mem);

    spin_threads_ = new QSpinBox(grp_solver);
    spin_threads_->setRange(1, 64);
    spin_threads_->setValue(4);
    spin_threads_->setSuffix(" threads");
    spin_threads_->setToolTip("Número de threads para processamento paralelo");
    spin_threads_->setFixedWidth(140);
    form_solver->addRow("Número de Threads:", spin_threads_);

    chk_log_solver_ = new QCheckBox("Habilitar log de busca em tempo real do CP-SAT", grp_solver);
    form_solver->addRow("", chk_log_solver_);

    chk_usar_distribuicao_como_entrada_ = new QCheckBox("Usar distribuição de disciplinas atual em tela como entrada (Warm Start)", grp_solver);
    chk_usar_distribuicao_como_entrada_->setToolTip("Se ativado, utiliza a distribuição de disciplinas atualmente na tela como ponto de partida (solução inicial) para o próximo algoritmo.");
    chk_usar_distribuicao_como_entrada_->setChecked(true);
    form_solver->addRow("", chk_usar_distribuicao_como_entrada_);

    main_layout->addWidget(grp_solver);
    main_layout->addStretch();

    scroll_area->setWidget(content_widget);
    root_layout->addWidget(scroll_area);
}

/**
 * @brief Atualiza o texto dos labels indicadores de valor ao lado de cada slider.
 */
void ConfigWidget::updateSliderLabels() {
    lbl_val_janelas_->setText(QString::number(slider_janelas_->value()));
    lbl_val_dias_->setText(QString::number(slider_dias_->value()));
    lbl_val_pref_horarios_->setText(QString::number(slider_pref_horarios_->value()));
    lbl_val_pref_dias_->setText(QString::number(slider_pref_dias_->value()));
    lbl_val_pref_turnos_->setText(QString::number(slider_pref_turnos_->value()));
    lbl_val_lacunas_turma_->setText(QString::number(slider_lacunas_turma_->value()));
}

/**
 * @brief Carrega os valores contidos em model::OptimizationConfig para os campos da interface.
 * @param cfg Estrutura com os valores de configuração.
 */
void ConfigWidget::loadConfig(const model::OptimizationConfig& cfg) {
    slider_janelas_->setValue(cfg.lambda_janelas);
    slider_dias_->setValue(cfg.lambda_dias);
    slider_pref_horarios_->setValue(cfg.lambda_preferencias);
    slider_pref_dias_->setValue(cfg.lambda_preferencia_dias);
    slider_pref_turnos_->setValue(cfg.lambda_preferencia_turnos);
    slider_lacunas_turma_->setValue(cfg.lambda_lacunas_turma);

    int idx_dias = combo_modelo_dias_->findData(static_cast<int>(cfg.modelo_preferencia_dias));
    if (idx_dias >= 0) combo_modelo_dias_->setCurrentIndex(idx_dias);

    chk_preenchimento_integral_->setChecked(cfg.preenchimento_integral_jornada);
    chk_distribuir_semana_->setChecked(cfg.distribuir_disciplinas_na_semana);
    spin_max_aulas_dia_->setValue(cfg.max_aulas_disciplina_dia_turma);

    int target_algo = static_cast<int>(cfg.algoritmo);
    if (cfg.algoritmo == model::SolverAlgorithm::MILP) {
        target_algo = static_cast<int>(model::SolverAlgorithm::HIGHS);
    }
    int idx_algo = combo_algoritmo_->findData(target_algo);
    if (idx_algo >= 0) combo_algoritmo_->setCurrentIndex(idx_algo);

    int idx_esc = combo_escopo_semestre_->findData(static_cast<int>(cfg.escopo_semestre));
    if (idx_esc >= 0) combo_escopo_semestre_->setCurrentIndex(idx_esc);

    chk_sincronizar_anuais_->setChecked(cfg.sincronizar_anuais_entre_semestres);

    spin_tempo_limite_->setValue(cfg.tempo_limite_segundos);
    if (chk_sem_limite_tempo_) {
        chk_sem_limite_tempo_->setChecked(cfg.sem_limite_tempo);
        spin_tempo_limite_->setEnabled(!cfg.sem_limite_tempo);
    }
    spin_max_memoria_->setValue(cfg.max_memoria_mb > 0 ? cfg.max_memoria_mb : 2048);
    if (chk_sem_limite_memoria_) {
        chk_sem_limite_memoria_->setChecked(cfg.sem_limite_memoria);
        spin_max_memoria_->setEnabled(!cfg.sem_limite_memoria);
    }
    spin_threads_->setValue(cfg.num_threads);
    chk_log_solver_->setChecked(cfg.log_search_progress);
    chk_usar_distribuicao_como_entrada_->setChecked(cfg.usar_distribuicao_como_entrada);

    updateSliderLabels();
}

/**
 * @brief Extrai uma model::OptimizationConfig preenchida a partir dos controles da tela.
 * @return model::OptimizationConfig com os valores ativos.
 */
model::OptimizationConfig ConfigWidget::getConfig() const {
    model::OptimizationConfig cfg;
    cfg.lambda_janelas = slider_janelas_->value();
    cfg.lambda_dias = slider_dias_->value();
    cfg.lambda_preferencias = slider_pref_horarios_->value();
    cfg.lambda_preferencia_dias = slider_pref_dias_->value();
    cfg.lambda_preferencia_turnos = slider_pref_turnos_->value();
    cfg.lambda_lacunas_turma = slider_lacunas_turma_->value();

    cfg.modelo_preferencia_dias = static_cast<model::ModeloPreferenciaDias>(
        combo_modelo_dias_->currentData().toInt());

    cfg.preenchimento_integral_jornada = chk_preenchimento_integral_->isChecked();
    cfg.distribuir_disciplinas_na_semana = chk_distribuir_semana_->isChecked();
    cfg.max_aulas_disciplina_dia_turma = spin_max_aulas_dia_->value();

    cfg.algoritmo = static_cast<model::SolverAlgorithm>(
        combo_algoritmo_->currentData().toInt());

    cfg.escopo_semestre = static_cast<model::EscopoSemestreOtimizacao>(
        combo_escopo_semestre_->currentData().toInt());
    cfg.sincronizar_anuais_entre_semestres = chk_sincronizar_anuais_->isChecked();

    cfg.tempo_limite_segundos = spin_tempo_limite_->value();
    cfg.sem_limite_tempo = chk_sem_limite_tempo_ ? chk_sem_limite_tempo_->isChecked() : false;
    cfg.max_memoria_mb = spin_max_memoria_->value();
    cfg.sem_limite_memoria = chk_sem_limite_memoria_ ? chk_sem_limite_memoria_->isChecked() : false;
    cfg.num_threads = spin_threads_->value();
    cfg.log_search_progress = chk_log_solver_->isChecked();
    cfg.usar_distribuicao_como_entrada = chk_usar_distribuicao_como_entrada_->isChecked();

    return cfg;
}

} // namespace scheduler::ui
