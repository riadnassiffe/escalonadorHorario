/**
 * @file MainWindow.cpp
 * @brief Implementação da janela principal e controladores de interface da aplicação.
 */

#include "ui/MainWindow.h"
#include "ui/SmartTableWidgetItem.h"
#include "ui/ProfessorEditorDialog.h"
#include "ui/TurmaEditorDialog.h"
#include "ui/RoomEditorDialog.h"
#include "ui/LessonEditorDialog.h"
#include "ui/OptimizeDialog.h"
#include "ui/ScheduleReportExporter.h"
#include "ui/ModernTheme.h"
#include "serialization/JsonSerialization.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QToolBar>
#include <QToolButton>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QDateTime>
#include <QShortcut>
#include <set>

namespace scheduler::ui {

/**
 * @brief Construtor da janela principal.
 * @param parent Widget pai na hierarquia Qt.
 */
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setupUi();
    onLoadDefaultExample();
}

/**
 * @brief Inicializa e organiza os componentes visuais principais, abas e banners.
 */
void MainWindow::setupUi() {
    setWindowTitle("Escalonador Inteligente de Horários Escolares");
    resize(1300, 850);

    setupMenuBar();
    setupToolBar();

    auto* central_widget = new QWidget(this);
    auto* main_layout = new QVBoxLayout(central_widget);
    main_layout->setContentsMargins(12, 8, 12, 12);
    main_layout->setSpacing(10);

    setupKpiBanner();
    main_layout->addWidget(lbl_kpi_status_->parentWidget());

    tab_widget_ = new QTabWidget(this);

    timetable_view_ = new TimetableView(data_, this);
    connect(timetable_view_, &TimetableView::timetableModified, this, &MainWindow::updateKpiBanner);
    connect(timetable_view_, &TimetableView::lessonMoved, this, [this](const QString& aid, const QString& details) {
        statusBar()->showMessage("Aula " + aid + " modificada na grade: " + details, 6000);
        updateKpiBanner();
    });
    config_widget_ = new ConfigWidget(this);

    tab_widget_->addTab(timetable_view_, "📅 Grade Semanal");
    tab_widget_->addTab(createProfessoresTab(), "👨‍🏫 Professores");
    tab_widget_->addTab(createTurmasTab(), "🎓 Turmas & Jornadas");
    tab_widget_->addTab(createSalasTab(), "🏫 Salas & Laboratórios");
    tab_widget_->addTab(createDisciplinasTab(), "📚 Disciplinas");
    tab_widget_->addTab(config_widget_, "⚙️ Parâmetros da Otimização");
    tab_widget_->addTab(createLogsTab(), "📝 Diagnóstico & Logs");

    main_layout->addWidget(tab_widget_);
    setCentralWidget(central_widget);

    // Barra de status
    progress_bar_ = new QProgressBar(this);
    progress_bar_->setMaximumWidth(200);
    progress_bar_->setVisible(false);
    statusBar()->addPermanentWidget(progress_bar_);
    statusBar()->showMessage("Pronto para otimizar.");
}

/**
 * @brief Configura a barra de menus com opções de projeto, exportação e temas.
 */
void MainWindow::setupMenuBar() {
    auto* menu_arquivo = menuBar()->addMenu("&Arquivo");
    menu_arquivo->addAction("Novo Projeto", QKeySequence::New, this, &MainWindow::onNewProject);
    menu_arquivo->addAction("Abrir JSON...", QKeySequence::Open, this, &MainWindow::onOpenFile);
    menu_arquivo->addAction("Salvar JSON...", QKeySequence::Save, this, &MainWindow::onSaveFile);
    menu_arquivo->addAction("Carregar Exemplo Padrão", this, &MainWindow::onLoadDefaultExample);
    menu_arquivo->addSeparator();
    menu_arquivo->addAction("Exportar Grade HTML...", this, &MainWindow::onExportHtml);
    menu_arquivo->addAction("Exportar Grade CSV...", this, &MainWindow::onExportCsv);
    menu_arquivo->addSeparator();
    menu_arquivo->addAction("Sair", QKeySequence::Quit, this, &QWidget::close);

    auto* menu_grade = menuBar()->addMenu("&Grade");
    auto* act_alocar = menu_grade->addAction("➕ Alocar Disciplina Manualmente...", this, [this]() {
        tab_widget_->setCurrentWidget(timetable_view_);
        timetable_view_->alocarAulaManual(-1, -1);
    });
    act_alocar->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));

    menu_grade->addSeparator();
    menu_grade->addAction("📌 Fixar Todas as Aulas da Grade", this, [this]() {
        for (auto& aloc : data_.grade().alocacoes()) {
            auto* a_model = data_.findAula(aloc.aula_id);
            if (a_model) {
                a_model->setPinned(true);
                a_model->setPinnedDay(aloc.dia);
                a_model->setPinnedStartSlot(aloc.intervalo.start_slot);
                a_model->setPinnedRooms(aloc.salas);
                aloc.is_pinned = true;
            }
        }
        timetable_view_->refreshView();
        updateKpiBanner();
    });
    menu_grade->addAction("🔓 Desafixar Todas as Aulas da Grade", this, [this]() {
        for (auto& aloc : data_.grade().alocacoes()) {
            auto* a_model = data_.findAula(aloc.aula_id);
            if (a_model) {
                a_model->unpin();
                aloc.is_pinned = false;
            }
        }
        timetable_view_->refreshView();
        updateKpiBanner();
    });
    menu_grade->addSeparator();
    menu_grade->addAction("🗑️ Limpar Toda a Grade...", this, [this]() {
        timetable_view_->limparGradeCompleta();
        updateKpiBanner();
    });

    auto* menu_exibir = menuBar()->addMenu("&Exibir");
    auto* menu_tema = menu_exibir->addMenu("🎨 &Tema Visual");

    auto* grp_tema = new QActionGroup(this);
    grp_tema->setExclusive(true);

    auto add_theme_action = [this, menu_tema, grp_tema](const QString& title, ModernTheme::ThemeMode mode, bool checked = false) {
        auto* act = menu_tema->addAction(title);
        act->setCheckable(true);
        act->setChecked(checked);
        grp_tema->addAction(act);
        connect(act, &QAction::triggered, this, [this, mode]() {
            ModernTheme::setThemeMode(*qApp, mode);
            refreshAllTables();
            if (timetable_view_) {
                timetable_view_->refreshView();
            }
            updateKpiBanner();
        });
        return act;
    };

    add_theme_action("Padrão do Sistema (KDE Plasma)", ModernTheme::ThemeMode::SystemKDE, true);
    add_theme_action("KDE Breeze Nativo", ModernTheme::ThemeMode::Breeze, false);
    add_theme_action("Qt Fusion", ModernTheme::ThemeMode::Fusion, false);
    add_theme_action("Escuro Moderno (Slate Dark)", ModernTheme::ThemeMode::ModernDark, false);
}

/**
 * @brief Monta a barra de ferramentas superior com atalhos de otimização, arquivo e exportação.
 */
void MainWindow::setupToolBar() {
    auto* toolbar = addToolBar("Ações");
    toolbar->setMovable(false);

    auto* btn_optimize = new QToolButton(toolbar);
    btn_optimize->setObjectName("btnOptimize");
    btn_optimize->setText("⚡ OTIMIZAR GRADE");
    btn_optimize->setToolTip("Selecionar algoritmo e otimizar grade horária escolar (Seção 4 & 7)");
    connect(btn_optimize, &QToolButton::clicked, this, &MainWindow::onOptimize);
    toolbar->addWidget(btn_optimize);

    toolbar->addSeparator();

    auto* btn_new = new QToolButton(toolbar);
    btn_new->setText("Novo Projeto");
    connect(btn_new, &QToolButton::clicked, this, &MainWindow::onNewProject);
    toolbar->addWidget(btn_new);

    auto* btn_open = new QToolButton(toolbar);
    btn_open->setText("Abrir JSON");
    connect(btn_open, &QToolButton::clicked, this, &MainWindow::onOpenFile);
    toolbar->addWidget(btn_open);

    auto* btn_save = new QToolButton(toolbar);
    btn_save->setText("Salvar JSON");
    connect(btn_save, &QToolButton::clicked, this, &MainWindow::onSaveFile);
    toolbar->addWidget(btn_save);

    auto* btn_example = new QToolButton(toolbar);
    btn_example->setText("Carregar Exemplo Padrão");
    btn_example->setToolTip("Carrega o cenário acadêmico completo com 45/50 min, turmas, laboratórios e preferências");
    connect(btn_example, &QToolButton::clicked, this, &MainWindow::onLoadDefaultExample);
    toolbar->addWidget(btn_example);

    toolbar->addSeparator();

    auto* btn_export_html = new QToolButton(toolbar);
    btn_export_html->setText("Exportar HTML");
    btn_export_html->setToolTip("Gera relatório visual em HTML pronto para impressão");
    connect(btn_export_html, &QToolButton::clicked, this, &MainWindow::onExportHtml);
    toolbar->addWidget(btn_export_html);

    auto* btn_export_csv = new QToolButton(toolbar);
    btn_export_csv->setText("Exportar CSV");
    connect(btn_export_csv, &QToolButton::clicked, this, &MainWindow::onExportCsv);
    toolbar->addWidget(btn_export_csv);
}

/**
 * @brief Constrói o painel horizontal de cartões com os indicadores-chave de desempenho (KPIs).
 */
void MainWindow::setupKpiBanner() {
    auto* banner_card = new QFrame(this);
    banner_card->setFrameShape(QFrame::StyledPanel);
    banner_card->setObjectName("kpiBannerCard");
    banner_card->setStyleSheet(
        "QFrame#kpiBannerCard { background-color: palette(base); border: 1px solid palette(mid); border-radius: 8px; padding: 4px; }");

    auto* layout = new QHBoxLayout(banner_card);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(20);

    auto make_kpi = [](const QString& label, QLabel*& val_label, const QString& init_val, const QString& color = "") {
        auto* box = new QVBoxLayout();
        box->setSpacing(2);
        val_label = new QLabel(init_val);
        if (color.isEmpty()) {
            val_label->setStyleSheet("font-size: 15px; font-weight: bold; color: palette(highlight);");
        } else {
            val_label->setStyleSheet(QString("font-size: 15px; font-weight: bold; color: %1;").arg(color));
        }
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet("font-size: 11px; font-weight: 500;");
        box->addWidget(val_label);
        box->addWidget(lbl);
        return box;
    };

    layout->addLayout(make_kpi("Status do Solver", lbl_kpi_status_, "NÃO EXECUTADO", "#64748b"));
    layout->addLayout(make_kpi("Tempo de Resolução", lbl_kpi_tempo_, "0.0 ms"));
    layout->addLayout(make_kpi("Aulas Alocadas", lbl_kpi_aulas_, "0 / 0", "#16a34a"));
    layout->addLayout(make_kpi("Janelas Professores (J)", lbl_kpi_janelas_, "0 min", "#d97706"));
    layout->addLayout(make_kpi("Dias Trabalhados (D)", lbl_kpi_dias_, "0"));
    layout->addLayout(make_kpi("Penalidade Pref. Dias (W)", lbl_kpi_pref_dias_, "0"));
    layout->addStretch();
}

/**
 * @brief Atualiza os valores e indicadores visuais dos KPIs a partir das estatísticas da grade atual.
 */
void MainWindow::updateKpiBanner() {
    const auto& s = data_.grade().estatisticas();
    lbl_kpi_status_->setText(QString::fromStdString(s.status));
    if (s.status == "OPTIMAL") {
        lbl_kpi_status_->setStyleSheet("font-size: 15px; font-weight: bold; color: #16a34a;");
    } else if (s.status == "FEASIBLE") {
        lbl_kpi_status_->setStyleSheet("font-size: 15px; font-weight: bold; color: #0284c7;");
    } else if (s.status == "INFEASIBLE") {
        lbl_kpi_status_->setStyleSheet("font-size: 15px; font-weight: bold; color: #dc2626;");
    } else {
        lbl_kpi_status_->setStyleSheet("font-size: 15px; font-weight: bold; color: #64748b;");
    }

    lbl_kpi_tempo_->setText(QString::number(s.tempo_execucao_ms, 'f', 1) + " ms");
    lbl_kpi_aulas_->setText(QString("%1 / %2").arg(s.total_aulas_alocadas).arg(s.total_aulas_esperadas));
    lbl_kpi_janelas_->setText(QString::number(s.penalidade_janelas_J) + " min");
    lbl_kpi_dias_->setText(QString::number(s.penalidade_dias_D));
    lbl_kpi_pref_dias_->setText(QString::number(s.penalidade_preferencia_dias_W));
}

/**
 * @brief Constrói a aba de gerenciamento de professores, incluindo tabela, busca e ações CRUD.
 * @return QWidget* da aba de professores.
 */
QWidget* MainWindow::createProfessoresTab() {
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(10, 10, 10, 10);

    auto* btn_bar = new QHBoxLayout();
    auto* btn_add = new QPushButton("+ Novo Professor", widget);
    auto* btn_edit = new QPushButton("Editar", widget);
    btn_edit->setObjectName("btnSecondary");
    auto* btn_rem = new QPushButton("Remover", widget);
    btn_rem->setObjectName("btnDanger");
    btn_rem->setToolTip("Remover professor(es) selecionado(s) em massa (Delete)");
    btn_bar->addWidget(btn_add);
    btn_bar->addWidget(btn_edit);
    btn_bar->addWidget(btn_rem);
    btn_bar->addStretch();

    table_professores_ = new QTableWidget(widget);
    table_professores_->setColumnCount(5);
    table_professores_->setHorizontalHeaderLabels({"ID", "Nome", "Pesos Semanais (SEG..SEX)", "Carga Máx. Diária", "Disponibilidade"});
    table_professores_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_professores_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_professores_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_professores_->setColumnHidden(0, true);
    layout->addWidget(table_professores_);

    search_bar_professores_ = new TableSearchSortBar(widget);
    search_bar_professores_->setupWithTable(table_professores_, "professor", "professores", 1 /* Nome */);
    btn_bar->addWidget(search_bar_professores_);
    layout->insertLayout(0, btn_bar);

    connect(btn_add, &QPushButton::clicked, this, &MainWindow::onAddProfessor);
    connect(btn_edit, &QPushButton::clicked, this, &MainWindow::onEditProfessor);
    connect(btn_rem, &QPushButton::clicked, this, &MainWindow::onRemoveProfessor);
    connect(table_professores_, &QTableWidget::cellDoubleClicked, this, &MainWindow::onEditProfessor);
    connect(new QShortcut(QKeySequence::Delete, table_professores_), &QShortcut::activated, this, &MainWindow::onRemoveProfessor);

    return widget;
}

/**
 * @brief Constrói a aba de gerenciamento de turmas, jornadas e salas teóricas.
 * @return QWidget* da aba de turmas.
 */
QWidget* MainWindow::createTurmasTab() {
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(10, 10, 10, 10);

    auto* btn_bar = new QHBoxLayout();
    auto* btn_add = new QPushButton("+ Nova Turma", widget);
    auto* btn_edit = new QPushButton("Editar", widget);
    btn_edit->setObjectName("btnSecondary");
    auto* btn_rem = new QPushButton("Remover", widget);
    btn_rem->setObjectName("btnDanger");
    btn_rem->setToolTip("Remover turma(s) selecionada(s) em massa (Delete)");
    btn_bar->addWidget(btn_add);
    btn_bar->addWidget(btn_edit);
    btn_bar->addWidget(btn_rem);
    btn_bar->addStretch();

    table_turmas_ = new QTableWidget(widget);
    table_turmas_->setColumnCount(6);
    table_turmas_->setHorizontalHeaderLabels({"ID", "Nome", "Turno", "Jornada", "Intervalos Obrigatórios (2)", "Sala Teórica"});
    table_turmas_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_turmas_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_turmas_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_turmas_->setColumnHidden(0, true);
    layout->addWidget(table_turmas_);

    search_bar_turmas_ = new TableSearchSortBar(widget);
    search_bar_turmas_->setupWithTable(table_turmas_, "turma", "turmas", 1 /* Nome */);
    btn_bar->addWidget(search_bar_turmas_);
    layout->insertLayout(0, btn_bar);

    connect(btn_add, &QPushButton::clicked, this, &MainWindow::onAddTurma);
    connect(btn_edit, &QPushButton::clicked, this, &MainWindow::onEditTurma);
    connect(btn_rem, &QPushButton::clicked, this, &MainWindow::onRemoveTurma);
    connect(table_turmas_, &QTableWidget::cellDoubleClicked, this, &MainWindow::onEditTurma);
    connect(new QShortcut(QKeySequence::Delete, table_turmas_), &QShortcut::activated, this, &MainWindow::onRemoveTurma);

    return widget;
}

/**
 * @brief Constrói a aba de gerenciamento de salas e laboratórios disponíveis.
 * @return QWidget* da aba de salas.
 */
QWidget* MainWindow::createSalasTab() {
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(10, 10, 10, 10);

    auto* btn_bar = new QHBoxLayout();
    auto* btn_add = new QPushButton("+ Nova Sala", widget);
    auto* btn_edit = new QPushButton("Editar", widget);
    btn_edit->setObjectName("btnSecondary");
    auto* btn_rem = new QPushButton("Remover", widget);
    btn_rem->setObjectName("btnDanger");
    btn_rem->setToolTip("Remover sala(s) selecionada(s) em massa (Delete)");
    btn_bar->addWidget(btn_add);
    btn_bar->addWidget(btn_edit);
    btn_bar->addWidget(btn_rem);
    btn_bar->addStretch();

    table_salas_ = new QTableWidget(widget);
    table_salas_->setColumnCount(5);
    table_salas_->setHorizontalHeaderLabels({"ID", "Nome", "Tipo", "Capacidade", "Recursos"});
    table_salas_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_salas_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_salas_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_salas_->setColumnHidden(0, true);
    layout->addWidget(table_salas_);

    search_bar_salas_ = new TableSearchSortBar(widget);
    search_bar_salas_->setupWithTable(table_salas_, "sala", "salas", 1 /* Nome */);
    btn_bar->addWidget(search_bar_salas_);
    layout->insertLayout(0, btn_bar);

    connect(btn_add, &QPushButton::clicked, this, &MainWindow::onAddSala);
    connect(btn_edit, &QPushButton::clicked, this, &MainWindow::onEditSala);
    connect(btn_rem, &QPushButton::clicked, this, &MainWindow::onRemoveSala);
    connect(table_salas_, &QTableWidget::cellDoubleClicked, this, &MainWindow::onEditSala);
    connect(new QShortcut(QKeySequence::Delete, table_salas_), &QShortcut::activated, this, &MainWindow::onRemoveSala);

    return widget;
}

/**
 * @brief Constrói a aba de gerenciamento de disciplinas curriculares e restrições pedagógicas.
 * @return QWidget* da aba de disciplinas.
 */
QWidget* MainWindow::createDisciplinasTab() {
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(10, 10, 10, 10);

    auto* btn_bar = new QHBoxLayout();
    auto* btn_add = new QPushButton("+ Nova Disciplina", widget);
    auto* btn_edit = new QPushButton("Editar", widget);
    btn_edit->setObjectName("btnSecondary");
    auto* btn_rem = new QPushButton("Remover", widget);
    btn_rem->setObjectName("btnDanger");
    btn_rem->setToolTip("Remover disciplina(s) selecionada(s) em massa (Delete)");
    btn_bar->addWidget(btn_add);
    btn_bar->addWidget(btn_edit);
    btn_bar->addWidget(btn_rem);
    btn_bar->addStretch();

    table_disciplinas_ = new QTableWidget(widget);
    table_disciplinas_->setColumnCount(8);
    table_disciplinas_->setHorizontalHeaderLabels({"ID", "Nome", "Regime / Semestre", "Tipo", "Duração", "Aulas/Sem", "Professor", "Turma"});
    table_disciplinas_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_disciplinas_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_disciplinas_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_disciplinas_->setColumnHidden(0, true);
    layout->addWidget(table_disciplinas_);

    search_bar_disciplinas_ = new TableSearchSortBar(widget);
    search_bar_disciplinas_->setupWithTable(table_disciplinas_, "disciplina", "disciplinas", 1 /* Nome */);
    btn_bar->addWidget(search_bar_disciplinas_);
    layout->insertLayout(0, btn_bar);

    connect(btn_add, &QPushButton::clicked, this, &MainWindow::onAddDisciplina);
    connect(btn_edit, &QPushButton::clicked, this, &MainWindow::onEditDisciplina);
    connect(btn_rem, &QPushButton::clicked, this, &MainWindow::onRemoveDisciplina);
    connect(table_disciplinas_, &QTableWidget::cellDoubleClicked, this, &MainWindow::onEditDisciplina);
    connect(new QShortcut(QKeySequence::Delete, table_disciplinas_), &QShortcut::activated, this, &MainWindow::onRemoveDisciplina);

    return widget;
}

/**
 * @brief Constrói a aba de diagnóstico e console de mensagens de resolução do solver.
 * @return QWidget* da aba de logs.
 */
QWidget* MainWindow::createLogsTab() {
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(10, 10, 10, 10);

    auto* btn_clear = new QPushButton("Limpar Logs", widget);
    btn_clear->setObjectName("btnSecondary");
    btn_clear->setFixedWidth(120);
    layout->addWidget(btn_clear);

    txt_logs_ = new QPlainTextEdit(widget);
    txt_logs_->setReadOnly(true);
    layout->addWidget(txt_logs_);

    connect(btn_clear, &QPushButton::clicked, txt_logs_, &QPlainTextEdit::clear);
    return widget;
}

/**
 * @brief Reseta o estado atual e inicializa um novo projeto vazio.
 */
void MainWindow::onNewProject() {
    if (QMessageBox::question(this, "Novo Projeto", "Deseja iniciar um novo projeto limpo? Todos os dados não salvos serão perdidos.",
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        data_.clear();
        refreshAllTables();
        timetable_view_->refreshView();
        updateKpiBanner();
        txt_logs_->appendPlainText("[SISTEMA] Novo projeto iniciado.");
    }
}

/**
 * @brief Abre diálogo para carregar um arquivo JSON contendo dados da instituição.
 */
void MainWindow::onOpenFile() {
    QString path = QFileDialog::getOpenFileName(this, "Abrir Arquivo JSON", "", "Arquivos JSON (*.json)");
    if (path.isEmpty()) return;

    std::string err;
    if (serialization::JsonSerialization::carregarArquivo(path.toStdString(), data_, err)) {
        refreshAllTables();
        config_widget_->loadConfig(data_.config());
        timetable_view_->refreshView();
        updateKpiBanner();
        txt_logs_->appendPlainText("[ARQUIVO] Carregado com sucesso: " + path);
        statusBar()->showMessage("Arquivo carregado com sucesso.", 4000);
    } else {
        QMessageBox::critical(this, "Erro ao Abrir", QString::fromStdString(err));
    }
}

/**
 * @brief Abre diálogo para salvar os dados acadêmicos e grade atual em formato JSON.
 */
void MainWindow::onSaveFile() {
    QString path = QFileDialog::getSaveFileName(this, "Salvar Arquivo JSON", "escola_grade.json", "Arquivos JSON (*.json)");
    if (path.isEmpty()) return;

    data_.setConfig(config_widget_->getConfig());
    std::string err;
    if (serialization::JsonSerialization::salvarArquivo(path.toStdString(), data_, err)) {
        txt_logs_->appendPlainText("[ARQUIVO] Salvo com sucesso: " + path);
        statusBar()->showMessage("Arquivo salvo com sucesso.", 4000);
    } else {
        QMessageBox::critical(this, "Erro ao Salvar", QString::fromStdString(err));
    }
}

/**
 * @brief Carrega o cenário acadêmico demonstrativo padrão com 6 professores, 6 salas, 3 turmas e 19 disciplinas.
 */
void MainWindow::onLoadDefaultExample() {
    data_ = model::InstitutionData::criarExemploPadrao();
    refreshAllTables();
    config_widget_->loadConfig(data_.config());
    timetable_view_->refreshView();
    updateKpiBanner();
    txt_logs_->appendPlainText("[EXEMPLO] Cenário padrão carregado: 6 Professores, 6 Salas/Labs, 3 Turmas, 19 Disciplinas mistas (45 e 50 min).");
    statusBar()->showMessage("Cenário de exemplo carregado. Clique em '⚡ OTIMIZAR GRADE' para executar.", 5000);
}

/**
 * @brief Exibe o diálogo de seleção do algoritmo e parâmetros e executa a otimização da grade.
 */
void MainWindow::onOptimize() {
    auto current_cfg = config_widget_->getConfig();

    // Apresenta a caixa para seleção do algoritmo para resolver o problema
    OptimizeDialog dlg(current_cfg, static_cast<int>(data_.grade().alocacoes().size()), this);
    if (dlg.exec() != QDialog::Accepted) {
        return; // Usuário cancelou a operação
    }

    dlg.updateConfig(current_cfg);
    data_.setConfig(current_cfg);
    config_widget_->loadConfig(current_cfg);

    data_.gerarOcorrenciasAulas(true, current_cfg.escopo_semestre); // Preserva aulas fixadas e respeita escopo

    std::string algo_name = model::solverAlgorithmDisplayName(current_cfg.algoritmo);
    txt_logs_->appendPlainText("--------------------------------------------------");
    txt_logs_->appendPlainText("[ESCOPO] Escopo de Otimização: " + QString::fromStdString(model::escopoSemestreToString(current_cfg.escopo_semestre)));
    txt_logs_->appendPlainText("[OTIMIZAÇÃO] Algoritmo selecionado: " + QString::fromStdString(algo_name));
    if (current_cfg.sem_limite_tempo) {
        txt_logs_->appendPlainText("[PARÂMETRO] Tempo limite: Sem restrição de tempo (ilimitado)");
    } else {
        txt_logs_->appendPlainText(QString("[PARÂMETRO] Tempo limite: %1 s").arg(current_cfg.tempo_limite_segundos));
    }
    if (current_cfg.sem_limite_memoria || current_cfg.max_memoria_mb <= 0) {
        txt_logs_->appendPlainText("[PARÂMETRO] Limite de memória: Sem restrição (ilimitado)");
    } else {
        txt_logs_->appendPlainText(QString("[PARÂMETRO] Limite de memória: %1 MB").arg(current_cfg.max_memoria_mb));
    }
    if (current_cfg.usar_distribuicao_como_entrada && !data_.grade().alocacoes().empty()) {
        txt_logs_->appendPlainText(QString("[ENTRADA] Distribuição atual da tela (%1 aulas) fornecida como entrada / Warm Start.")
            .arg(data_.grade().alocacoes().size()));
    }
    txt_logs_->appendPlainText("[OTIMIZAÇÃO] Iniciando resolução: " + QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm:ss"));

    progress_bar_->setVisible(true);
    progress_bar_->setValue(10);
    statusBar()->showMessage("Executando " + QString::fromStdString(algo_name) + "...");

    auto tt = solver_.solve(data_, [this, algo_name](const std::string& msg, double ratio) {
        progress_bar_->setValue(static_cast<int>(ratio * 100));
        statusBar()->showMessage(QString("[%1] %2").arg(QString::fromStdString(algo_name), QString::fromStdString(msg)));
        txt_logs_->appendPlainText("[SOLVER] " + QString::fromStdString(msg));
    });

    data_.setGrade(tt);
    progress_bar_->setVisible(false);
    updateKpiBanner();
    timetable_view_->refreshView();

    const auto& s = tt.estatisticas();
    txt_logs_->appendPlainText(QString("[RESULTADO] Status: %1 | Tempo: %2 ms | Objetivo: %3")
        .arg(QString::fromStdString(s.status))
        .arg(s.tempo_execucao_ms, 0, 'f', 1)
        .arg(s.valor_objetivo, 0, 'f', 1));

    for (const auto& msg : tt.mensagensDiagnostico()) {
        txt_logs_->appendPlainText(QString::fromStdString(msg));
    }

    if (s.status == "OPTIMAL" || s.status == "FEASIBLE") {
        statusBar()->showMessage(QString("Otimização concluída com sucesso (%1)! %2 aulas alocadas.")
            .arg(QString::fromStdString(s.status)).arg(s.total_aulas_alocadas), 6000);
        tab_widget_->setCurrentIndex(0); // Alterna para a visualização da grade
    } else {
        statusBar()->showMessage("Falha na otimização: Restrições inviáveis.", 6000);
        QMessageBox::warning(this, "Inviabilidade",
            "O solver não encontrou solução viável que satisfaça todas as restrições obrigatórias.\n\n"
            "Verifique os diagnósticos na aba '📝 Diagnóstico & Logs'.");
        tab_widget_->setCurrentIndex(6); // Alterna para a aba de logs
    }
}

/**
 * @brief Abre diálogo para salvar a grade e KPIs em formato HTML estilizado.
 */
void MainWindow::onExportHtml() {
    if (data_.grade().alocacoes().empty()) {
        QMessageBox::information(this, "Atenção", "Gere a grade primeiro antes de exportar o relatório.");
        return;
    }
    QString path = QFileDialog::getSaveFileName(this, "Exportar Relatório HTML", "grade_horarios.html", "Arquivos HTML (*.html)");
    if (path.isEmpty()) return;

    std::string err;
    if (ScheduleReportExporter::exportarHtml(path.toStdString(), data_, err)) {
        txt_logs_->appendPlainText("[EXPORT] Relatório HTML exportado: " + path);
        QMessageBox::information(this, "Exportação Concluída", "Relatório HTML gerado com sucesso!\nSalvo em: " + path);
    } else {
        QMessageBox::critical(this, "Erro na Exportação", QString::fromStdString(err));
    }
}

/**
 * @brief Abre diálogo para salvar as alocações da grade em arquivo CSV.
 */
void MainWindow::onExportCsv() {
    if (data_.grade().alocacoes().empty()) {
        QMessageBox::information(this, "Atenção", "Gere a grade primeiro antes de exportar os dados.");
        return;
    }
    QString path = QFileDialog::getSaveFileName(this, "Exportar Dados CSV", "grade_horarios.csv", "Arquivos CSV (*.csv)");
    if (path.isEmpty()) return;

    std::string err;
    if (ScheduleReportExporter::exportarCsv(path.toStdString(), data_, err)) {
        txt_logs_->appendPlainText("[EXPORT] Dados CSV exportados: " + path);
        QMessageBox::information(this, "Exportação Concluída", "Dados CSV gerados com sucesso!\nSalvo em: " + path);
    } else {
        QMessageBox::critical(this, "Erro na Exportação", QString::fromStdString(err));
    }
}

/**
 * @brief Atualiza simultaneamente as 4 tabelas de cadastro no painel.
 */
void MainWindow::refreshAllTables() {
    refreshProfessoresTable();
    refreshTurmasTable();
    refreshSalasTable();
    refreshDisciplinasTable();
}

/**
 * @brief Preenche a tabela de professores e atualiza contadores e ordenação.
 */
void MainWindow::refreshProfessoresTable() {
    table_professores_->setSortingEnabled(false);
    table_professores_->setRowCount(0);
    for (const auto& p : data_.professores()) {
        int r = table_professores_->rowCount();
        table_professores_->insertRow(r);

        table_professores_->setItem(r, 0, new SmartTableWidgetItem(QString::fromStdString(p.id())));
        table_professores_->setItem(r, 1, new SmartTableWidgetItem(QString::fromStdString(p.nome())));

        QString pesos = QString("SEG=%1 TER=%2 QUA=%3 QUI=%4 SEX=%5")
            .arg(p.pesoDia(model::DayOfWeek::SEG))
            .arg(p.pesoDia(model::DayOfWeek::TER))
            .arg(p.pesoDia(model::DayOfWeek::QUA))
            .arg(p.pesoDia(model::DayOfWeek::QUI))
            .arg(p.pesoDia(model::DayOfWeek::SEX));
        table_professores_->setItem(r, 2, new SmartTableWidgetItem(pesos));

        QString carga = (p.cargaMaximaDiariaMinutos() > 0)
            ? QString::number(p.cargaMaximaDiariaMinutos()) + " min"
            : "Sem limite";
        table_professores_->setItem(r, 3, new SmartTableWidgetItem(carga));

        int total_inters = 0;
        for (const auto& [d, inters] : p.todasDisponibilidades()) total_inters += static_cast<int>(inters.size());
        table_professores_->setItem(r, 4, new SmartTableWidgetItem(QString("%1 turnos cadastrados").arg(total_inters)));
    }
    table_professores_->setSortingEnabled(true);
    if (search_bar_professores_) {
        search_bar_professores_->reapplyFilterAndSort();
    }
}

/**
 * @brief Preenche a tabela de turmas com detalhes de jornada, intervalos e salas teóricas.
 */
void MainWindow::refreshTurmasTable() {
    table_turmas_->setSortingEnabled(false);
    table_turmas_->setRowCount(0);
    for (const auto& t : data_.turmas()) {
        int r = table_turmas_->rowCount();
        table_turmas_->insertRow(r);

        table_turmas_->setItem(r, 0, new SmartTableWidgetItem(QString::fromStdString(t.id())));
        table_turmas_->setItem(r, 1, new SmartTableWidgetItem(QString::fromStdString(t.nome())));
        table_turmas_->setItem(r, 2, new SmartTableWidgetItem(QString::fromStdString(model::shiftToString(t.turno()))));

        QString jornada_str;
        QString intervals_str;
        QString tooltip_str = "<b>Horários e Intervalos da Turma:</b><br>";

        for (auto d : model::allDays()) {
            if (!t.funcionaNoDia(d)) {
                tooltip_str += QString("• <b>%1</b>: Sem aula (não letivo)<br>").arg(QString::fromStdString(model::dayToString(d)));
                continue;
            }
            auto jd = t.jornadaParaDia(d);
            QStringList ints;
            for (const auto& inter : jd.intervalosAtivos()) {
                ints << QString::fromStdString(inter.toString());
            }
            tooltip_str += QString("• <b>%1</b>: %2 [Intervalos: %3]<br>")
                .arg(QString::fromStdString(model::dayToString(d)))
                .arg(QString::fromStdString(jd.jornada_total.toString()))
                .arg(ints.isEmpty() ? "Nenhum" : ints.join(", "));
        }

        if (t.temExcecoes()) {
            QStringList diff_parts;
            for (const auto& [d, exc] : t.excecoesPorDia()) {
                diff_parts << QString("%1: %2").arg(QString::fromStdString(model::dayToString(d)),
                                                     QString::fromStdString(exc.jornada_total.toString()));
            }
            jornada_str = QString("%1 (%2)")
                .arg(QString::fromStdString(t.jornadaPadrao().jornada_total.toString()),
                     diff_parts.join("; "));
            intervals_str = "Personalizado por dia (passe o mouse)";
        } else {
            const auto& jp = t.jornadaPadrao();
            jornada_str = QString::fromStdString(jp.jornada_total.toString());
            QStringList int_strs;
            if (jp.int1_ativo) int_strs << QString("Int1: %1").arg(QString::fromStdString(jp.intervalo_1.toString()));
            if (jp.int2_ativo) int_strs << QString("Int2: %1").arg(QString::fromStdString(jp.intervalo_2.toString()));
            if (jp.int3_ativo) int_strs << QString("Int3: %1").arg(QString::fromStdString(jp.intervalo_3.toString()));
            intervals_str = int_strs.isEmpty() ? "Nenhum" : int_strs.join(" | ");
        }

        auto* item_jornada = new SmartTableWidgetItem(jornada_str);
        item_jornada->setToolTip(tooltip_str);
        table_turmas_->setItem(r, 3, item_jornada);

        auto* item_int = new SmartTableWidgetItem(intervals_str);
        item_int->setToolTip(tooltip_str);
        table_turmas_->setItem(r, 4, item_int);

        QString sala = t.temSalaTeoricaPredefinida()
            ? QString::fromStdString(t.salaTeoricaId() + " (Fixa)")
            : QString("A escolher (%1)").arg(t.salasTeoricasPermitidas().size());
        table_turmas_->setItem(r, 5, new SmartTableWidgetItem(sala));
    }
    table_turmas_->setSortingEnabled(true);
    if (search_bar_turmas_) {
        search_bar_turmas_->reapplyFilterAndSort();
    }
}

/**
 * @brief Preenche a tabela de salas e laboratórios com tipos e recursos disponíveis.
 */
void MainWindow::refreshSalasTable() {
    table_salas_->setSortingEnabled(false);
    table_salas_->setRowCount(0);
    for (const auto& s : data_.salas()) {
        int r = table_salas_->rowCount();
        table_salas_->insertRow(r);

        table_salas_->setItem(r, 0, new SmartTableWidgetItem(QString::fromStdString(s.id())));
        table_salas_->setItem(r, 1, new SmartTableWidgetItem(QString::fromStdString(s.nome())));
        table_salas_->setItem(r, 2, new SmartTableWidgetItem(QString::fromStdString(s.tipo())));
        table_salas_->setItem(r, 3, new SmartTableWidgetItem(QString::number(s.capacidade()) + " alunos"));

        QString recs;
        for (size_t i = 0; i < s.recursos().size(); ++i) {
            if (i > 0) recs += ", ";
            recs += QString::fromStdString(s.recursos()[i]);
        }
        table_salas_->setItem(r, 4, new SmartTableWidgetItem(recs));
    }
    table_salas_->setSortingEnabled(true);
    if (search_bar_salas_) {
        search_bar_salas_->reapplyFilterAndSort();
    }
}

/**
 * @brief Preenche a tabela de disciplinas com detalhes de regime, duração e professores associados.
 */
void MainWindow::refreshDisciplinasTable() {
    table_disciplinas_->setSortingEnabled(false);
    table_disciplinas_->setRowCount(0);
    for (const auto& d : data_.disciplinas()) {
        int r = table_disciplinas_->rowCount();
        table_disciplinas_->insertRow(r);

        table_disciplinas_->setItem(r, 0, new SmartTableWidgetItem(QString::fromStdString(d.id())));
        table_disciplinas_->setItem(r, 1, new SmartTableWidgetItem(QString::fromStdString(d.nome())));

        auto* item_regime = new SmartTableWidgetItem(QString::fromStdString(d.regimeSemestreFormatado()));
        bool is_dark = ModernTheme::isDarkTheme();
        if (d.regime() == model::RegimeDisciplina::ANUAL) {
            item_regime->setForeground(QBrush(is_dark ? QColor(56, 189, 248) : QColor(2, 132, 199))); // #0284c7 / #38bdf8
        } else if (d.semestre() == model::SemestreLetivo::SEMESTRE_1) {
            item_regime->setForeground(QBrush(is_dark ? QColor(251, 146, 60) : QColor(217, 119, 6))); // #d97706 / #fb923c
        } else {
            item_regime->setForeground(QBrush(is_dark ? QColor(52, 211, 153) : QColor(22, 163, 74))); // #16a34a / #34d399
        }
        table_disciplinas_->setItem(r, 2, item_regime);

        table_disciplinas_->setItem(r, 3, new SmartTableWidgetItem(QString::fromStdString(d.tipo())));
        table_disciplinas_->setItem(r, 4, new SmartTableWidgetItem(QString::number(d.duracaoMinutos()) + " min"));
        table_disciplinas_->setItem(r, 5, new SmartTableWidgetItem(QString::number(d.aulasSemanais())));

        QString profs;
        for (size_t i = 0; i < d.professores().size(); ++i) {
            if (i > 0) profs += ", ";
            const auto* p = data_.findProfessor(d.professores()[i]);
            profs += QString::fromStdString(p ? p->nome() : d.professores()[i]);
        }
        table_disciplinas_->setItem(r, 6, new SmartTableWidgetItem(profs));

        QString turmas;
        for (size_t i = 0; i < d.turmas().size(); ++i) {
            if (i > 0) turmas += ", ";
            const auto* t = data_.findTurma(d.turmas()[i]);
            turmas += QString::fromStdString(t ? t->nome() : d.turmas()[i]);
        }
        table_disciplinas_->setItem(r, 7, new SmartTableWidgetItem(turmas));
    }
    table_disciplinas_->setSortingEnabled(true);
    if (search_bar_disciplinas_) {
        search_bar_disciplinas_->reapplyFilterAndSort();
    }
}

/**
 * @brief Abre o diálogo de cadastro para adicionar um novo professor à instituição.
 */
void MainWindow::onAddProfessor() {
    ProfessorEditorDialog dlg(this, nullptr, data_.nextProfessorId());
    if (dlg.exec() == QDialog::Accepted) {
        data_.addProfessor(dlg.getProfessor());
        refreshProfessoresTable();
        timetable_view_->refreshView();
    }
}

/**
 * @brief Abre o diálogo de edição para o professor atualmente selecionado na tabela.
 */
void MainWindow::onEditProfessor() {
    int r = table_professores_->currentRow();
    if (r < 0 || !table_professores_->item(r, 0)) return;
    std::string id = table_professores_->item(r, 0)->text().toStdString();
    auto* p = data_.findProfessor(id);
    if (p) {
        ProfessorEditorDialog dlg(this, p);
        if (dlg.exec() == QDialog::Accepted) {
            data_.addProfessor(dlg.getProfessor());
            refreshProfessoresTable();
            timetable_view_->refreshView();
        }
    }
}

/**
 * @brief Solicita confirmação e remove o(s) professor(es) selecionado(s) da instituição.
 */
void MainWindow::onRemoveProfessor() {
    auto selected_rows = table_professores_->selectionModel()->selectedRows();
    if (selected_rows.isEmpty()) {
        int r = table_professores_->currentRow();
        if (r >= 0) {
            selected_rows.append(table_professores_->model()->index(r, 0));
        }
    }
    if (selected_rows.isEmpty()) {
        QMessageBox::information(this, "Remover Professor", "Nenhum professor selecionado para remoção.");
        return;
    }

    std::vector<std::string> ids_to_remove;
    QStringList names_to_remove;
    std::set<std::string> seen_ids;
    for (const auto& idx : selected_rows) {
        int r = idx.row();
        auto* item_id = table_professores_->item(r, 0);
        auto* item_nome = table_professores_->item(r, 1);
        if (item_id) {
            std::string id = item_id->text().toStdString();
            if (seen_ids.insert(id).second) {
                ids_to_remove.push_back(id);
                names_to_remove.append(item_nome ? item_nome->text() : QString::fromStdString(id));
            }
        }
    }

    if (ids_to_remove.empty()) return;

    QString msg;
    if (ids_to_remove.size() == 1) {
        msg = QString("Deseja realmente remover o professor \"%1\"?").arg(names_to_remove.first());
    } else {
        msg = QString("Deseja realmente remover os %1 professores selecionados em massa?\n\nItens:\n")
                  .arg(ids_to_remove.size());
        for (int i = 0; i < std::min<int>(names_to_remove.size(), 8); ++i) {
            msg += QString("• %1\n").arg(names_to_remove[i]);
        }
        if (names_to_remove.size() > 8) {
            msg += QString("... e mais %1 outro(s).").arg(names_to_remove.size() - 8);
        }
    }

    if (QMessageBox::question(this, "Confirmar Remoção", msg,
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        for (const auto& id : ids_to_remove) {
            data_.removeProfessor(id);
        }
        refreshProfessoresTable();
        refreshDisciplinasTable();
        timetable_view_->refreshView();
    }
}

/**
 * @brief Abre o diálogo de cadastro para adicionar uma nova turma.
 */
void MainWindow::onAddTurma() {
    TurmaEditorDialog dlg(data_.salas(), this, nullptr, data_.nextTurmaId());
    if (dlg.exec() == QDialog::Accepted) {
        data_.addTurma(dlg.getTurma());
        refreshTurmasTable();
        timetable_view_->refreshView();
    }
}

/**
 * @brief Abre o diálogo de edição para a turma atualmente selecionada.
 */
void MainWindow::onEditTurma() {
    int r = table_turmas_->currentRow();
    if (r < 0 || !table_turmas_->item(r, 0)) return;
    std::string id = table_turmas_->item(r, 0)->text().toStdString();
    auto* t = data_.findTurma(id);
    if (t) {
        TurmaEditorDialog dlg(data_.salas(), this, t);
        if (dlg.exec() == QDialog::Accepted) {
            data_.addTurma(dlg.getTurma());
            refreshTurmasTable();
            timetable_view_->refreshView();
        }
    }
}

/**
 * @brief Solicita confirmação e remove a(s) turma(s) selecionada(s).
 */
void MainWindow::onRemoveTurma() {
    auto selected_rows = table_turmas_->selectionModel()->selectedRows();
    if (selected_rows.isEmpty()) {
        int r = table_turmas_->currentRow();
        if (r >= 0) {
            selected_rows.append(table_turmas_->model()->index(r, 0));
        }
    }
    if (selected_rows.isEmpty()) {
        QMessageBox::information(this, "Remover Turma", "Nenhuma turma selecionada para remoção.");
        return;
    }

    std::vector<std::string> ids_to_remove;
    QStringList names_to_remove;
    std::set<std::string> seen_ids;
    for (const auto& idx : selected_rows) {
        int r = idx.row();
        auto* item_id = table_turmas_->item(r, 0);
        auto* item_nome = table_turmas_->item(r, 1);
        if (item_id) {
            std::string id = item_id->text().toStdString();
            if (seen_ids.insert(id).second) {
                ids_to_remove.push_back(id);
                names_to_remove.append(item_nome ? item_nome->text() : QString::fromStdString(id));
            }
        }
    }

    if (ids_to_remove.empty()) return;

    QString msg;
    if (ids_to_remove.size() == 1) {
        msg = QString("Deseja realmente remover a turma \"%1\"?").arg(names_to_remove.first());
    } else {
        msg = QString("Deseja realmente remover as %1 turmas selecionadas em massa?\n\nItens:\n")
                  .arg(ids_to_remove.size());
        for (int i = 0; i < std::min<int>(names_to_remove.size(), 8); ++i) {
            msg += QString("• %1\n").arg(names_to_remove[i]);
        }
        if (names_to_remove.size() > 8) {
            msg += QString("... e mais %1 outra(s).").arg(names_to_remove.size() - 8);
        }
    }

    if (QMessageBox::question(this, "Confirmar Remoção", msg,
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        for (const auto& id : ids_to_remove) {
            data_.removeTurma(id);
        }
        refreshTurmasTable();
        refreshDisciplinasTable();
        timetable_view_->refreshView();
    }
}

/**
 * @brief Abre o diálogo de cadastro para adicionar uma nova sala ou laboratório.
 */
void MainWindow::onAddSala() {
    RoomEditorDialog dlg(this, nullptr, data_.nextSalaId());
    if (dlg.exec() == QDialog::Accepted) {
        data_.addSala(dlg.getRoom());
        refreshSalasTable();
        timetable_view_->refreshView();
    }
}

/**
 * @brief Abre o diálogo de edição para a sala atualmente selecionada.
 */
void MainWindow::onEditSala() {
    int r = table_salas_->currentRow();
    if (r < 0 || !table_salas_->item(r, 0)) return;
    std::string id = table_salas_->item(r, 0)->text().toStdString();
    auto* s = data_.findSala(id);
    if (s) {
        RoomEditorDialog dlg(this, s);
        if (dlg.exec() == QDialog::Accepted) {
            data_.addSala(dlg.getRoom());
            refreshSalasTable();
            timetable_view_->refreshView();
        }
    }
}

/**
 * @brief Solicita confirmação e remove a(s) sala(s) selecionada(s).
 */
void MainWindow::onRemoveSala() {
    auto selected_rows = table_salas_->selectionModel()->selectedRows();
    if (selected_rows.isEmpty()) {
        int r = table_salas_->currentRow();
        if (r >= 0) {
            selected_rows.append(table_salas_->model()->index(r, 0));
        }
    }
    if (selected_rows.isEmpty()) {
        QMessageBox::information(this, "Remover Sala", "Nenhuma sala selecionada para remoção.");
        return;
    }

    std::vector<std::string> ids_to_remove;
    QStringList names_to_remove;
    std::set<std::string> seen_ids;
    for (const auto& idx : selected_rows) {
        int r = idx.row();
        auto* item_id = table_salas_->item(r, 0);
        auto* item_nome = table_salas_->item(r, 1);
        if (item_id) {
            std::string id = item_id->text().toStdString();
            if (seen_ids.insert(id).second) {
                ids_to_remove.push_back(id);
                names_to_remove.append(item_nome ? item_nome->text() : QString::fromStdString(id));
            }
        }
    }

    if (ids_to_remove.empty()) return;

    QString msg;
    if (ids_to_remove.size() == 1) {
        msg = QString("Deseja realmente remover a sala \"%1\"?").arg(names_to_remove.first());
    } else {
        msg = QString("Deseja realmente remover as %1 salas selecionadas em massa?\n\nItens:\n")
                  .arg(ids_to_remove.size());
        for (int i = 0; i < std::min<int>(names_to_remove.size(), 8); ++i) {
            msg += QString("• %1\n").arg(names_to_remove[i]);
        }
        if (names_to_remove.size() > 8) {
            msg += QString("... e mais %1 outra(s).").arg(names_to_remove.size() - 8);
        }
    }

    if (QMessageBox::question(this, "Confirmar Remoção", msg,
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        for (const auto& id : ids_to_remove) {
            data_.removeSala(id);
        }
        refreshSalasTable();
        refreshDisciplinasTable();
        timetable_view_->refreshView();
    }
}

/**
 * @brief Abre o diálogo de cadastro para adicionar uma nova disciplina curricular.
 */
void MainWindow::onAddDisciplina() {
    LessonEditorDialog dlg(data_.professores(), data_.turmas(), data_.salas(), this, nullptr, data_.nextDisciplinaId());
    if (dlg.exec() == QDialog::Accepted) {
        data_.addDisciplina(dlg.getDisciplina());
        data_.gerarOcorrenciasAulas(true, data_.config().escopo_semestre);
        refreshDisciplinasTable();
    }
}

/**
 * @brief Abre o diálogo de edição para a disciplina atualmente selecionada.
 */
void MainWindow::onEditDisciplina() {
    int r = table_disciplinas_->currentRow();
    if (r < 0 || !table_disciplinas_->item(r, 0)) return;
    std::string id = table_disciplinas_->item(r, 0)->text().toStdString();
    auto* d = data_.findDisciplina(id);
    if (d) {
        LessonEditorDialog dlg(data_.professores(), data_.turmas(), data_.salas(), this, d);
        if (dlg.exec() == QDialog::Accepted) {
            data_.addDisciplina(dlg.getDisciplina());
            data_.gerarOcorrenciasAulas(true, data_.config().escopo_semestre);
            refreshDisciplinasTable();
        }
    }
}

/**
 * @brief Solicita confirmação e remove a(s) disciplina(s) selecionada(s).
 */
void MainWindow::onRemoveDisciplina() {
    auto selected_rows = table_disciplinas_->selectionModel()->selectedRows();
    if (selected_rows.isEmpty()) {
        int r = table_disciplinas_->currentRow();
        if (r >= 0) {
            selected_rows.append(table_disciplinas_->model()->index(r, 0));
        }
    }
    if (selected_rows.isEmpty()) {
        QMessageBox::information(this, "Remover Disciplina", "Nenhuma disciplina selecionada para remoção.");
        return;
    }

    std::vector<std::string> ids_to_remove;
    QStringList names_to_remove;
    std::set<std::string> seen_ids;
    for (const auto& idx : selected_rows) {
        int r = idx.row();
        auto* item_id = table_disciplinas_->item(r, 0);
        auto* item_nome = table_disciplinas_->item(r, 1);
        if (item_id) {
            std::string id = item_id->text().toStdString();
            if (seen_ids.insert(id).second) {
                ids_to_remove.push_back(id);
                names_to_remove.append(item_nome ? item_nome->text() : QString::fromStdString(id));
            }
        }
    }

    if (ids_to_remove.empty()) return;

    QString msg;
    if (ids_to_remove.size() == 1) {
        msg = QString("Deseja realmente remover a disciplina \"%1\"?").arg(names_to_remove.first());
    } else {
        msg = QString("Deseja realmente remover as %1 disciplinas selecionadas em massa?\n\nItens:\n")
                  .arg(ids_to_remove.size());
        for (int i = 0; i < std::min<int>(names_to_remove.size(), 8); ++i) {
            msg += QString("• %1\n").arg(names_to_remove[i]);
        }
        if (names_to_remove.size() > 8) {
            msg += QString("... e mais %1 outra(s).").arg(names_to_remove.size() - 8);
        }
    }

    if (QMessageBox::question(this, "Confirmar Remoção", msg,
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        for (const auto& id : ids_to_remove) {
            data_.removeDisciplina(id);
        }
        data_.gerarOcorrenciasAulas(true, data_.config().escopo_semestre);
        refreshDisciplinasTable();
        timetable_view_->refreshView();
    }
}

} // namespace scheduler::ui
