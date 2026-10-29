/**
 * @file OptimizeDialog.cpp
 * @brief Implementação da janela de seleção e parametrização do resolvedor de horários.
 */

#include "ui/OptimizeDialog.h"
#include "ui/ModernTheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFrame>

namespace scheduler::ui {

/**
 * @brief Construtor do diálogo de otimização de grade.
 * @param current_config Configuração inicial de otimização.
 * @param num_aulas_em_tela Número de aulas atualmente renderizadas na grade.
 * @param parent Widget pai na hierarquia Qt.
 */
OptimizeDialog::OptimizeDialog(const model::OptimizationConfig& current_config, int num_aulas_em_tela, QWidget* parent)
    : QDialog(parent), initial_config_(current_config), num_aulas_em_tela_(num_aulas_em_tela) {
    setupUi();
}

/**
 * @brief Monta a interface gráfica, campos de tempo, paralelismo, escopo e cartões informativos.
 */
void OptimizeDialog::setupUi() {
    setWindowTitle("Otimizar Grade - Seleção de Algoritmo");
    setMinimumWidth(560);
    setModal(true);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(16);
    main_layout->setContentsMargins(20, 20, 20, 20);

    // 1. Banner Superior
    auto* banner_card = new QFrame(this);
    banner_card->setFrameShape(QFrame::StyledPanel);
    banner_card->setStyleSheet(
        "QFrame { background-color: palette(base); border: 1px solid palette(highlight); border-radius: 8px; padding: 10px; }");
    auto* lay_banner = new QVBoxLayout(banner_card);
    lay_banner->setSpacing(4);

    auto* lbl_title = new QLabel("⚡ OTIMIZAÇÃO DA GRADE ESCOLAR", banner_card);
    lbl_title->setStyleSheet("font-size: 15px; font-weight: bold; color: palette(highlight);");
    auto* lbl_subtitle = new QLabel("Selecione o algoritmo desejado para resolver o problema de alocação de horários e configure os parâmetros de busca.", banner_card);
    lbl_subtitle->setWordWrap(true);
    lbl_subtitle->setStyleSheet("font-size: 12px;");

    lay_banner->addWidget(lbl_title);
    lay_banner->addWidget(lbl_subtitle);
    main_layout->addWidget(banner_card);

    // 2. Grupo de Seleção de Algoritmo
    auto* grp_algo = new QGroupBox("Algoritmo de Resolução", this);
    auto* lay_algo = new QVBoxLayout(grp_algo);
    lay_algo->setSpacing(12);

    auto* lay_combo = new QHBoxLayout();
    auto* lbl_combo_title = new QLabel("Algoritmo:", grp_algo);
    lbl_combo_title->setStyleSheet("font-weight: bold; font-size: 13px; min-width: 80px;");
    
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
        "QFrame { background-color: palette(base); border: 1px solid palette(mid); border-radius: 6px; padding: 10px; }");
    auto* lay_details = new QVBoxLayout(card_details);
    lay_details->setSpacing(6);

    lbl_algo_desc_ = new QLabel(card_details);
    lbl_algo_desc_->setWordWrap(true);
    lbl_algo_desc_->setStyleSheet("font-size: 12px; line-height: 1.4;");

    auto* lay_badges = new QHBoxLayout();
    lay_badges->setSpacing(8);

    auto make_badge = [](const QString& title, QLabel*& lbl_val, const QString& bg_color, const QString& text_color) {
        auto* box = new QHBoxLayout();
        auto* tag = new QLabel(title + ": ");
        tag->setStyleSheet("font-size: 11px; font-weight: 500;");
        lbl_val = new QLabel();
        lbl_val->setStyleSheet(QString("background-color: %1; color: %2; font-size: 11px; font-weight: bold; border-radius: 4px; padding: 2px 6px;")
                               .arg(bg_color, text_color));
        box->addWidget(tag);
        box->addWidget(lbl_val);
        return box;
    };

    bool is_dark = ModernTheme::isDarkTheme();
    lay_badges->addLayout(make_badge("Abordagem", lbl_algo_type_,
                                     is_dark ? "#1e293b" : "rgba(2, 132, 199, 0.15)",
                                     is_dark ? "#38bdf8" : "#0284c7"));
    lay_badges->addLayout(make_badge("Garantia", lbl_algo_guarantee_,
                                     is_dark ? "#14532d" : "rgba(22, 163, 74, 0.15)",
                                     is_dark ? "#4ade80" : "#16a34a"));
    lay_badges->addLayout(make_badge("Desempenho", lbl_algo_speed_,
                                     is_dark ? "#312e81" : "rgba(109, 40, 217, 0.15)",
                                     is_dark ? "#a5b4fc" : "#6d28d9"));
    lay_badges->addStretch();

    lay_details->addWidget(lbl_algo_desc_);
    lay_details->addLayout(lay_badges);
    lay_algo->addWidget(card_details);

    main_layout->addWidget(grp_algo);

    // 3. Parâmetros Rápidos de Execução
    auto* grp_params = new QGroupBox("Parâmetros da Execução Atual", this);
    auto* form_params = new QFormLayout(grp_params);
    form_params->setSpacing(10);

    combo_escopo_semestre_ = new QComboBox(grp_params);
    combo_escopo_semestre_->addItem("Todas as Disciplinas (Geral)", static_cast<int>(model::EscopoSemestreOtimizacao::TODAS_SEM_FILTRO));
    combo_escopo_semestre_->addItem("1º Semestre (Anuais + 1º Sem)", static_cast<int>(model::EscopoSemestreOtimizacao::SEMESTRE_1));
    combo_escopo_semestre_->addItem("2º Semestre (Anuais + 2º Sem)", static_cast<int>(model::EscopoSemestreOtimizacao::SEMESTRE_2));
    combo_escopo_semestre_->addItem("Ambos os Semestres (Multi-Semestre Integrado)", static_cast<int>(model::EscopoSemestreOtimizacao::AMBOS_INTEGRADO));
    int idx_esc = combo_escopo_semestre_->findData(static_cast<int>(initial_config_.escopo_semestre));
    if (idx_esc >= 0) combo_escopo_semestre_->setCurrentIndex(idx_esc);
    form_params->addRow("Escopo de Semestre:", combo_escopo_semestre_);

    chk_sincronizar_anuais_ = new QCheckBox("Sincronizar horários de disciplinas anuais entre semestres", grp_params);
    chk_sincronizar_anuais_->setChecked(initial_config_.sincronizar_anuais_entre_semestres);
    form_params->addRow("", chk_sincronizar_anuais_);

    auto* lay_time = new QHBoxLayout();
    lay_time->setSpacing(12);

    spin_time_limit_ = new QDoubleSpinBox(grp_params);
    spin_time_limit_->setRange(1.0, 3600.0);
    spin_time_limit_->setValue(initial_config_.tempo_limite_segundos);
    spin_time_limit_->setSuffix(" segundos");

    chk_sem_limite_tempo_ = new QCheckBox("Sem restrição de tempo", grp_params);
    chk_sem_limite_tempo_->setChecked(initial_config_.sem_limite_tempo);
    chk_sem_limite_tempo_->setToolTip("Executa o solver sem tempo limite máximo, buscando até a convergência ou prova de viabilidade/inviabilidade.");

    connect(chk_sem_limite_tempo_, &QCheckBox::toggled, this, [this](bool checked) {
        auto algo = getSelectedAlgorithm();
        if (algo != model::SolverAlgorithm::GREEDY_HEURISTIC) {
            spin_time_limit_->setEnabled(!checked);
        }
    });

    lay_time->addWidget(spin_time_limit_);
    lay_time->addWidget(chk_sem_limite_tempo_);
    lay_time->addStretch();
    form_params->addRow("Tempo Limite:", lay_time);

    auto* lay_mem = new QHBoxLayout();
    lay_mem->setSpacing(12);

    spin_mem_limit_ = new QSpinBox(grp_params);
    spin_mem_limit_->setRange(256, 131072);
    spin_mem_limit_->setSingleStep(256);
    spin_mem_limit_->setValue(initial_config_.max_memoria_mb > 0 ? initial_config_.max_memoria_mb : 2048);
    spin_mem_limit_->setSuffix(" MB");

    chk_sem_limite_memoria_ = new QCheckBox("Sem restrição de memória", grp_params);
    chk_sem_limite_memoria_->setChecked(initial_config_.sem_limite_memoria);
    chk_sem_limite_memoria_->setToolTip("Executa o solver sem impor teto explícito de memória RAM.");

    connect(chk_sem_limite_memoria_, &QCheckBox::toggled, this, [this](bool checked) {
        auto algo = getSelectedAlgorithm();
        if (algo != model::SolverAlgorithm::GREEDY_HEURISTIC) {
            spin_mem_limit_->setEnabled(!checked);
        }
    });

    lay_mem->addWidget(spin_mem_limit_);
    lay_mem->addWidget(chk_sem_limite_memoria_);
    lay_mem->addStretch();
    form_params->addRow("Limite de Memória:", lay_mem);

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
    chk_usar_grade_atual_->setStyleSheet("font-weight: bold; color: palette(highlight);");

    lbl_grade_atual_info_ = new QLabel(grp_params);
    lbl_grade_atual_info_->setWordWrap(true);
    if (num_aulas_em_tela_ > 0) {
        lbl_grade_atual_info_->setText(QString("💡 Grade em tela detectada com %1 aulas. O algoritmo utilizará essa distribuição como ponto de partida inicial para acelerar e guiar a busca.").arg(num_aulas_em_tela_));
        lbl_grade_atual_info_->setStyleSheet("font-size: 11px;");
    } else {
        lbl_grade_atual_info_->setText("ℹ️ Nenhuma grade alocada na tela no momento. O algoritmo iniciará uma alocação do zero.");
        lbl_grade_atual_info_->setStyleSheet("font-size: 11px;");
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

/**
 * @brief Slot acionado na mudança do algoritmo no combobox.
 * @param index Índice do novo item selecionado.
 */
void OptimizeDialog::onAlgorithmChanged(int index) {
    auto algo = static_cast<model::SolverAlgorithm>(combo_algorithm_->itemData(index).toInt());
    updateAlgorithmDetails(algo);
}

/**
 * @brief Atualiza a ficha técnica informativa do algoritmo (tipo, garantia, velocidade e limites).
 * @param algo Algoritmo de resolução selecionado.
 */
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
            chk_sem_limite_tempo_->setEnabled(true);
            spin_time_limit_->setEnabled(!chk_sem_limite_tempo_->isChecked());
            chk_sem_limite_memoria_->setEnabled(true);
            spin_mem_limit_->setEnabled(!chk_sem_limite_memoria_->isChecked());
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
            chk_sem_limite_tempo_->setEnabled(false);
            spin_time_limit_->setEnabled(false);
            chk_sem_limite_memoria_->setEnabled(false);
            spin_mem_limit_->setEnabled(false);
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
            chk_sem_limite_tempo_->setEnabled(true);
            spin_time_limit_->setEnabled(!chk_sem_limite_tempo_->isChecked());
            chk_sem_limite_memoria_->setEnabled(true);
            spin_mem_limit_->setEnabled(!chk_sem_limite_memoria_->isChecked());
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
            chk_sem_limite_tempo_->setEnabled(true);
            spin_time_limit_->setEnabled(!chk_sem_limite_tempo_->isChecked());
            chk_sem_limite_memoria_->setEnabled(true);
            spin_mem_limit_->setEnabled(!chk_sem_limite_memoria_->isChecked());
            spin_threads_->setEnabled(true);
            chk_log_->setEnabled(false);
            break;
    }
}

/**
 * @brief Retorna o algoritmo atualmente selecionado.
 * @return model::SolverAlgorithm selecionado.
 */
model::SolverAlgorithm OptimizeDialog::getSelectedAlgorithm() const {
    return static_cast<model::SolverAlgorithm>(combo_algorithm_->currentData().toInt());
}

/**
 * @brief Grava as opções selecionadas pelo usuário na estrutura de configuração fornecida.
 * @param cfg Estrutura model::OptimizationConfig que será atualizada.
 */
void OptimizeDialog::updateConfig(model::OptimizationConfig& cfg) const {
    cfg.algoritmo = getSelectedAlgorithm();
    if (combo_escopo_semestre_) {
        cfg.escopo_semestre = static_cast<model::EscopoSemestreOtimizacao>(combo_escopo_semestre_->currentData().toInt());
    }
    if (chk_sincronizar_anuais_) {
        cfg.sincronizar_anuais_entre_semestres = chk_sincronizar_anuais_->isChecked();
    }
    cfg.tempo_limite_segundos = spin_time_limit_->value();
    if (chk_sem_limite_tempo_) {
        cfg.sem_limite_tempo = chk_sem_limite_tempo_->isChecked();
    }
    cfg.max_memoria_mb = spin_mem_limit_->value();
    if (chk_sem_limite_memoria_) {
        cfg.sem_limite_memoria = chk_sem_limite_memoria_->isChecked();
    }
    cfg.num_threads = spin_threads_->value();
    cfg.log_search_progress = chk_log_->isChecked();
    if (chk_usar_grade_atual_) {
        cfg.usar_distribuicao_como_entrada = chk_usar_grade_atual_->isChecked();
    }
}

} // namespace scheduler::ui
