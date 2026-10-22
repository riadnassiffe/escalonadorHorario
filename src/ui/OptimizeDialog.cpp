#include "ui/OptimizeDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFrame>

namespace scheduler::ui {

OptimizeDialog::OptimizeDialog(const model::OptimizationConfig& current_config, int num_aulas_em_tela, QWidget* parent)
    : QDialog(parent), initial_config_(current_config), num_aulas_em_tela_(num_aulas_em_tela) {
    setupUi();
}

void OptimizeDialog::setupUi() {
    setWindowTitle("Otimizar Grade - Seleção de Algoritmo");
    setMinimumWidth(560);
    setModal(true);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(16);
    main_layout->setContentsMargins(20, 20, 20, 20);

    // 1. Banner Superior
    auto* banner_card = new QWidget(this);
    banner_card->setStyleSheet(
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1e293b, stop:1 #172554);"
        "border: 1px solid #3b82f6; border-radius: 8px; padding: 10px;");
    auto* lay_banner = new QVBoxLayout(banner_card);
    lay_banner->setSpacing(4);

    auto* lbl_title = new QLabel("⚡ OTIMIZAÇÃO DA GRADE ESCOLAR", banner_card);
    lbl_title->setStyleSheet("font-size: 15px; font-weight: bold; color: #38bdf8;");
    auto* lbl_subtitle = new QLabel("Selecione o algoritmo desejado para resolver o problema de alocação de horários e configure os parâmetros de busca.", banner_card);
    lbl_subtitle->setWordWrap(true);
    lbl_subtitle->setStyleSheet("font-size: 12px; color: #cbd5e1;");

    lay_banner->addWidget(lbl_title);
    lay_banner->addWidget(lbl_subtitle);
    main_layout->addWidget(banner_card);

    // 2. Grupo de Seleção de Algoritmo
    auto* grp_algo = new QGroupBox("Algoritmo de Resolução", this);
    auto* lay_algo = new QVBoxLayout(grp_algo);
    lay_algo->setSpacing(12);

    auto* lay_combo = new QHBoxLayout();
    auto* lbl_combo_title = new QLabel("Algoritmo:", grp_algo);
    lbl_combo_title->setStyleSheet("font-weight: bold; font-size: 13px; color: #f8fafc; min-width: 80px;");
    
    combo_algorithm_ = new QComboBox(grp_algo);
    combo_algorithm_->addItem("Google OR-Tools CP-SAT (Restrições / Exato) [Recomendado]",
                              static_cast<int>(model::SolverAlgorithm::CP_SAT));
    combo_algorithm_->addItem("Heurística Construtiva Gulosa (Greedy Search) [Rápido]",
                              static_cast<int>(model::SolverAlgorithm::GREEDY_HEURISTIC));
    combo_algorithm_->addItem("Programação Linear Inteira Mista (MILP) [Comparativo]",
                              static_cast<int>(model::SolverAlgorithm::MILP));
    combo_algorithm_->addItem("Algoritmo Genético (Meta-heurística) [Estocástico]",
                              static_cast<int>(model::SolverAlgorithm::GENETIC_ALGORITHM));
    
    int initial_idx = combo_algorithm_->findData(static_cast<int>(initial_config_.algoritmo));
    if (initial_idx >= 0) {
        combo_algorithm_->setCurrentIndex(initial_idx);
    }
    
    lay_combo->addWidget(lbl_combo_title);
    lay_combo->addWidget(combo_algorithm_, 1);
    lay_algo->addLayout(lay_combo);

    // Card de Detalhes do Algoritmo Selecionado
    auto* card_details = new QFrame(grp_algo);
    card_details->setFrameShape(QFrame::StyledPanel);
    card_details->setStyleSheet(
        "background-color: #0b1329; border: 1px solid #1e3a8a; border-radius: 6px; padding: 10px;");
    auto* lay_details = new QVBoxLayout(card_details);
    lay_details->setSpacing(6);

    lbl_algo_desc_ = new QLabel(card_details);
    lbl_algo_desc_->setWordWrap(true);
    lbl_algo_desc_->setStyleSheet("color: #e2e8f0; font-size: 12px; line-height: 1.4;");

    auto* lay_badges = new QHBoxLayout();
    lay_badges->setSpacing(8);

    auto make_badge = [](const QString& title, QLabel*& lbl_val, const QString& bg_color, const QString& text_color) {
        auto* box = new QHBoxLayout();
        auto* tag = new QLabel(title + ": ");
        tag->setStyleSheet("color: #94a3b8; font-size: 11px; font-weight: 500;");
        lbl_val = new QLabel();
        lbl_val->setStyleSheet(QString("background-color: %1; color: %2; font-size: 11px; font-weight: bold; border-radius: 4px; padding: 2px 6px;")
                               .arg(bg_color, text_color));
        box->addWidget(tag);
        box->addWidget(lbl_val);
        return box;
    };

    lay_badges->addLayout(make_badge("Abordagem", lbl_algo_type_, "#1e293b", "#38bdf8"));
    lay_badges->addLayout(make_badge("Garantia", lbl_algo_guarantee_, "#14532d", "#4ade80"));
    lay_badges->addLayout(make_badge("Desempenho", lbl_algo_speed_, "#312e81", "#a5b4fc"));
    lay_badges->addStretch();

    lay_details->addWidget(lbl_algo_desc_);
    lay_details->addLayout(lay_badges);
    lay_algo->addWidget(card_details);

    main_layout->addWidget(grp_algo);

    // 3. Parâmetros Rápidos de Execução
    auto* grp_params = new QGroupBox("Parâmetros da Execução Atual", this);
    auto* form_params = new QFormLayout(grp_params);
    form_params->setSpacing(10);

    spin_time_limit_ = new QDoubleSpinBox(grp_params);
    spin_time_limit_->setRange(1.0, 3600.0);
    spin_time_limit_->setValue(initial_config_.tempo_limite_segundos);
    spin_time_limit_->setSuffix(" segundos");
    form_params->addRow("Tempo Limite:", spin_time_limit_);

    spin_threads_ = new QSpinBox(grp_params);
    spin_threads_->setRange(1, 64);
    spin_threads_->setValue(initial_config_.num_threads);
    spin_threads_->setSuffix(" threads");
    form_params->addRow("Processamento:", spin_threads_);

    chk_log_ = new QCheckBox("Exibir log detalhado de busca durante o processo", grp_params);
    chk_log_->setChecked(initial_config_.log_search_progress);
    form_params->addRow("", chk_log_);

    chk_usar_grade_atual_ = new QCheckBox("Usar distribuição de disciplinas atual em tela como entrada (Warm Start)", grp_params);
    chk_usar_grade_atual_->setChecked(initial_config_.usar_distribuicao_como_entrada && num_aulas_em_tela_ > 0);
    chk_usar_grade_atual_->setEnabled(num_aulas_em_tela_ > 0);
    chk_usar_grade_atual_->setStyleSheet("font-weight: bold; color: #38bdf8;");

    lbl_grade_atual_info_ = new QLabel(grp_params);
    lbl_grade_atual_info_->setWordWrap(true);
    if (num_aulas_em_tela_ > 0) {
        lbl_grade_atual_info_->setText(QString("💡 Grade em tela detectada com %1 aulas. O algoritmo utilizará essa distribuição como ponto de partida inicial para acelerar e guiar a busca.").arg(num_aulas_em_tela_));
        lbl_grade_atual_info_->setStyleSheet("color: #94a3b8; font-size: 11px;");
    } else {
        lbl_grade_atual_info_->setText("ℹ️ Nenhuma grade alocada na tela no momento. O algoritmo iniciará uma alocação do zero.");
        lbl_grade_atual_info_->setStyleSheet("color: #64748b; font-size: 11px;");
    }
    form_params->addRow("", chk_usar_grade_atual_);
    form_params->addRow("", lbl_grade_atual_info_);

    main_layout->addWidget(grp_params);

    // 4. Botões de Ação
    auto* lay_buttons = new QHBoxLayout();
    lay_buttons->addStretch();

    auto* btn_cancel = new QPushButton("Cancelar", this);
    btn_cancel->setObjectName("btnSecondary");
    connect(btn_cancel, &QPushButton::clicked, this, &QDialog::reject);

    auto* btn_start = new QPushButton("⚡ Iniciar Otimização", this);
    btn_start->setObjectName("btnOptimize");
    btn_start->setDefault(true);
    connect(btn_start, &QPushButton::clicked, this, &QDialog::accept);

    lay_buttons->addWidget(btn_cancel);
    lay_buttons->addWidget(btn_start);
    main_layout->addLayout(lay_buttons);

    connect(combo_algorithm_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OptimizeDialog::onAlgorithmChanged);

    onAlgorithmChanged(combo_algorithm_->currentIndex());
}

void OptimizeDialog::onAlgorithmChanged(int index) {
    auto algo = static_cast<model::SolverAlgorithm>(combo_algorithm_->itemData(index).toInt());
    updateAlgorithmDetails(algo);
}

void OptimizeDialog::updateAlgorithmDetails(model::SolverAlgorithm algo) {
    switch (algo) {
        case model::SolverAlgorithm::CP_SAT:
            lbl_algo_desc_->setText(
                "<b>Google OR-Tools CP-SAT (Constraint Programming com SAT)</b><br>"
                "Modelagem exata com restrições NoOverlap e intervalos temporais. "
                "Encontra a solução comprovadamente ótima global respeitando rigorosamente "
                "a jornada, salas teóricas fixas e penalidades de professores (Seção 4 & 7).");
            lbl_algo_type_->setText("Exato (CP-SAT)");
            lbl_algo_guarantee_->setText("Ótimo Global");
            lbl_algo_speed_->setText("Médio / Equilibrado");
            spin_time_limit_->setEnabled(true);
            spin_threads_->setEnabled(true);
            chk_log_->setEnabled(true);
            break;

        case model::SolverAlgorithm::GREEDY_HEURISTIC:
            lbl_algo_desc_->setText(
                "<b>Heurística Construtiva Gulosa (Greedy Priority Search)</b><br>"
                "Aloca as aulas ordenadas pelo grau de dificuldade (menor número de candidatos viáveis primeiro), "
                "atribuindo a cada aula o melhor horário e sala disponíveis para minimizar janelas e folgas. "
                "Gera a grade em poucos milissegundos.");
            lbl_algo_type_->setText("Heurístico Construtivo");
            lbl_algo_guarantee_->setText("Subótimo / Rápido");
            lbl_algo_speed_->setText("Instantâneo (< 0.1s)");
            spin_time_limit_->setEnabled(false);
            spin_threads_->setEnabled(false);
            chk_log_->setEnabled(false);
            break;

        case model::SolverAlgorithm::MILP:
            lbl_algo_desc_->setText(
                "<b>Programação Linear Inteira Mista (MILP)</b><br>"
                "Formulações lineares discretas via Branch-and-Cut (conforme Seção 7 das instruções). "
                "Utiliza relaxação contínua e planos de corte para convergir ao ótimo global. "
                "Se os solvers externos (SCIP/HiGHS) não estiverem instalados, integra via formulação CP-SAT.");
            lbl_algo_type_->setText("Programação Linear (MILP)");
            lbl_algo_guarantee_->setText("Ótimo Global");
            lbl_algo_speed_->setText("Moderado");
            spin_time_limit_->setEnabled(true);
            spin_threads_->setEnabled(true);
            chk_log_->setEnabled(true);
            break;

        case model::SolverAlgorithm::GENETIC_ALGORITHM:
            lbl_algo_desc_->setText(
                "<b>Algoritmo Genético / Meta-heurística Populacional</b><br>"
                "Evolução iterativa com múltiplas soluções candidatas, operadores de crossover, "
                "mutação de alocações e refinamento local por vizinhança. "
                "Excelente para explorar múltiplos cenários viáveis em problemas complexos.");
            lbl_algo_type_->setText("Meta-heurística Estocástica");
            lbl_algo_guarantee_->setText("Aproximado / Heurístico");
            lbl_algo_speed_->setText("Iterativo");
            spin_time_limit_->setEnabled(true);
            spin_threads_->setEnabled(true);
            chk_log_->setEnabled(false);
            break;
    }
}

model::SolverAlgorithm OptimizeDialog::getSelectedAlgorithm() const {
    return static_cast<model::SolverAlgorithm>(combo_algorithm_->currentData().toInt());
}

void OptimizeDialog::updateConfig(model::OptimizationConfig& cfg) const {
    cfg.algoritmo = getSelectedAlgorithm();
    cfg.tempo_limite_segundos = spin_time_limit_->value();
    cfg.num_threads = spin_threads_->value();
    cfg.log_search_progress = chk_log_->isChecked();
    if (chk_usar_grade_atual_) {
        cfg.usar_distribuicao_como_entrada = chk_usar_grade_atual_->isChecked();
    }
}

} // namespace scheduler::ui
