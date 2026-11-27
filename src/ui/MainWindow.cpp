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
#include "ui/ResponsiveUi.h"
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
#include <QCloseEvent>
#include <QPointer>
#include <QMetaObject>
#include <QStyle>
#include <QApplication>
#include <algorithm>
#include <set>

namespace scheduler::ui {

namespace {

QString formatHorasMin(int minutos) {
    const int safe = std::max(0, minutos);
    return QString("%1h%2").arg(safe / 60).arg(safe % 60, 2, 10, QChar('0'));
}

QString formatDisciplinasTooltip(const std::vector<model::DisciplinaCargaItem>& items, int max_items = 12) {
    if (items.empty()) {
        return "<i>Nenhuma disciplina vinculada.</i>";
    }
    QStringList lines;
    const int limit = std::min<int>(static_cast<int>(items.size()), max_items);
    for (int i = 0; i < limit; ++i) {
        const auto& item = items[i];
        const QString turma_info = item.turmas_resumo.empty()
            ? QString("")
            : QString(" [%1]").arg(QString::fromStdString(item.turmas_resumo).toHtmlEscaped());
        lines << QString("• <b>%1</b>%2 (%3) — %4a × %5m = %6m")
                     .arg(QString::fromStdString(item.disciplina_nome).toHtmlEscaped(),
                          turma_info,
                          QString::fromStdString(item.regime_semestre).toHtmlEscaped())
                     .arg(item.aulas_semanais)
                     .arg(item.duracao_minutos)
                     .arg(item.minutos_semanais);
    }
    if (static_cast<int>(items.size()) > max_items) {
        lines << QString("<i>... e mais %1 disciplina(s).</i>")
                     .arg(static_cast<int>(items.size()) - max_items);
    }
    return lines.join("<br>");
}

} // namespace

/**
 * @brief Construtor da janela principal.
 * @param parent Widget pai na hierarquia Qt.
 */
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setupUi();
    onLoadDefaultExample();
}

MainWindow::~MainWindow() {
    if (solver_thread_.joinable()) {
        solver_.requestStop();
        solver_thread_.join();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (is_optimizing_) {
        auto resp = QMessageBox::question(
            this,
            "Otimização em Andamento",
            "Uma otimização ainda está sendo executada em segundo plano.\n\n"
            "Deseja interromper a busca e fechar o aplicativo?",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (resp != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        solver_.requestStop();
        statusBar()->showMessage("Interrompendo otimização para encerrar...");
        QApplication::processEvents();
        if (solver_thread_.joinable()) {
            solver_thread_.join();
        }
    } else if (solver_thread_.joinable()) {
        solver_thread_.join();
    }
    event->accept();
}

/**
 * @brief Inicializa e organiza os componentes visuais principais, abas e banners.
 */
void MainWindow::setupUi() {
    setWindowTitle("Escalonador Inteligente de Horários Escolares");
    setWindowIcon(ModernTheme::icon("app_icon"));
    setMinimumSize(780, 500);

    setupMenuBar();
    setupToolBar();

    auto* central_widget = new QWidget(this);
    auto* main_layout = new QVBoxLayout(central_widget);
    main_layout->setContentsMargins(8, 6, 8, 8);
    main_layout->setSpacing(6);

    setupKpiBanner();
    main_layout->addWidget(kpi_banner_card_);

    tab_widget_ = new QTabWidget(this);
    tab_widget_->setUsesScrollButtons(true);

    timetable_view_ = new TimetableView(data_, this);
    connect(timetable_view_, &TimetableView::timetableModified, this, [this]() {
        updateKpiBanner();
        refreshProfessoresTable();
        refreshSalasTable();
    });
    connect(timetable_view_, &TimetableView::lessonPinChanged, this, [this](const QString&, bool) {
        updateKpiBanner();
    });
    connect(timetable_view_, &TimetableView::lessonMoved, this, [this](const QString& aid, const QString& details) {
        statusBar()->showMessage("Aula " + aid + " modificada na grade: " + details, 6000);
        updateKpiBanner();
        refreshProfessoresTable();
        refreshSalasTable();
    });
    config_widget_ = new ConfigWidget(this);
    mutating_widgets_.append(config_widget_);

    tab_widget_->addTab(timetable_view_, ModernTheme::icon("calendar"), "Grade Semanal");
    tab_widget_->addTab(createProfessoresTab(), ModernTheme::icon("professor"), "Professores");
    tab_widget_->addTab(createTurmasTab(), ModernTheme::icon("turma"), "Turmas & Jornadas");
    tab_widget_->addTab(createSalasTab(), ModernTheme::icon("room"), "Salas & Laboratórios");
    tab_widget_->addTab(createDisciplinasTab(), ModernTheme::icon("disciplina"), "Disciplinas");
    tab_widget_->addTab(config_widget_, ModernTheme::icon("settings"), "Parâmetros da Otimização");
    tab_widget_->addTab(createLogsTab(), ModernTheme::icon("logs"), "Diagnóstico & Logs");

    main_layout->addWidget(tab_widget_);
    setCentralWidget(central_widget);

    // Barra de status
    progress_bar_ = new QProgressBar(this);
    progress_bar_->setMaximumWidth(200);
    progress_bar_->setVisible(false);
    statusBar()->addPermanentWidget(progress_bar_);
    statusBar()->showMessage("Pronto para otimizar.");

    elapsed_timer_ = new QTimer(this);
    elapsed_timer_->setInterval(1000);
    connect(elapsed_timer_, &QTimer::timeout, this, &MainWindow::updateOptimizationStatusBar);

    fitWindowToScreen(this, QSize(1280, 780));
}

/**
 * @brief Configura a barra de menus com opções de projeto, exportação e temas.
 */
void MainWindow::setupMenuBar() {
    auto* menu_arquivo = menuBar()->addMenu("&Arquivo");
    mutating_actions_.append(menu_arquivo->addAction(ModernTheme::icon("new_project"), "Novo Projeto", QKeySequence::New, this, &MainWindow::onNewProject));
    mutating_actions_.append(menu_arquivo->addAction(ModernTheme::icon("open_file"), "Abrir JSON...", QKeySequence::Open, this, &MainWindow::onOpenFile));
    menu_arquivo->addAction(ModernTheme::icon("save_file"), "Salvar JSON...", QKeySequence::Save, this, &MainWindow::onSaveFile);
    mutating_actions_.append(menu_arquivo->addAction(ModernTheme::icon("example"), "Carregar Exemplo Padrão", this, &MainWindow::onLoadDefaultExample));
    menu_arquivo->addSeparator();
    menu_arquivo->addAction(ModernTheme::icon("export_html"), "Exportar Grade HTML...", this, &MainWindow::onExportHtml);
    menu_arquivo->addAction(ModernTheme::icon("export_csv"), "Exportar Grade CSV...", this, &MainWindow::onExportCsv);
    menu_arquivo->addSeparator();
    menu_arquivo->addAction("Sair", QKeySequence::Quit, this, &QWidget::close);

    auto* menu_grade = menuBar()->addMenu("&Grade");
    auto* act_alocar = menu_grade->addAction(ModernTheme::icon("add"), "Alocar Disciplina Manualmente...", this, [this]() {
        if (is_optimizing_) return;
        tab_widget_->setCurrentWidget(timetable_view_);
        timetable_view_->alocarAulaManual(-1, -1);
    });
    act_alocar->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    mutating_actions_.append(act_alocar);

    menu_grade->addSeparator();
    mutating_actions_.append(menu_grade->addAction(ModernTheme::icon("pin"), "Fixar Todas as Aulas da Grade", this, [this]() {
        if (is_optimizing_) return;
        for (auto& aloc : data_.grade().alocacoes()) {
            auto* a_model = data_.findAula(aloc.aula_id);
            if (a_model) {
                a_model->setPinned(true);
                a_model->setPinnedDay(aloc.dia);
                a_model->setPinnedInterval(aloc.intervalo);
                a_model->setPinnedRooms(aloc.salas);
                aloc.is_pinned = true;
            }
        }
        timetable_view_->refreshView();
        updateKpiBanner();
    }));
    mutating_actions_.append(menu_grade->addAction(ModernTheme::icon("unpin"), "Desafixar Todas as Aulas da Grade", this, [this]() {
        if (is_optimizing_) return;
        for (auto& aloc : data_.grade().alocacoes()) {
            auto* a_model = data_.findAula(aloc.aula_id);
            if (a_model) {
                a_model->unpin();
                aloc.is_pinned = false;
            }
        }
        timetable_view_->refreshView();
        updateKpiBanner();
    }));
    menu_grade->addSeparator();
    mutating_actions_.append(menu_grade->addAction(ModernTheme::icon("delete"), "Limpar Toda a Grade...", this, [this]() {
        if (is_optimizing_) return;
        timetable_view_->limparGradeCompleta();
        updateKpiBanner();
        refreshProfessoresTable();
        refreshSalasTable();
    }));

    auto* menu_exibir = menuBar()->addMenu("&Exibir");
    auto* menu_tema = menu_exibir->addMenu(ModernTheme::icon("theme"), "&Tema Visual");

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

    menu_exibir->addSeparator();
    auto* act_kpi = menu_exibir->addAction("Exibir Barra de Indicadores (KPIs)");
    act_kpi->setCheckable(true);
    act_kpi->setChecked(true);
    connect(act_kpi, &QAction::toggled, this, [this](bool checked) {
        if (kpi_banner_card_) {
            kpi_banner_card_->setVisible(checked);
        }
    });
}

/**
 * @brief Monta a barra de ferramentas superior com atalhos de otimização, arquivo e exportação.
 */
void MainWindow::setupToolBar() {
    auto* toolbar = addToolBar("Ações");
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(18, 18));

    btn_optimize_ = new QToolButton(toolbar);
    btn_optimize_->setObjectName("btnOptimize");
    btn_optimize_->setIcon(ModernTheme::icon("optimize"));
    btn_optimize_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn_optimize_->setText("OTIMIZAR GRADE");
    btn_optimize_->setToolTip("Selecionar algoritmo e otimizar grade horária escolar (Seção 4 & 7)");
    connect(btn_optimize_, &QToolButton::clicked, this, &MainWindow::onOptimize);
    toolbar->addWidget(btn_optimize_);

    toolbar->addSeparator();

    auto* btn_new = new QToolButton(toolbar);
    btn_new->setIcon(ModernTheme::icon("new_project"));
    btn_new->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn_new->setText("Novo Projeto");
    connect(btn_new, &QToolButton::clicked, this, &MainWindow::onNewProject);
    toolbar->addWidget(btn_new);
    mutating_widgets_.append(btn_new);

    auto* btn_open = new QToolButton(toolbar);
    btn_open->setIcon(ModernTheme::icon("open_file"));
    btn_open->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn_open->setText("Abrir JSON");
    connect(btn_open, &QToolButton::clicked, this, &MainWindow::onOpenFile);
    toolbar->addWidget(btn_open);
    mutating_widgets_.append(btn_open);

    auto* btn_save = new QToolButton(toolbar);
    btn_save->setIcon(ModernTheme::icon("save_file"));
    btn_save->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn_save->setText("Salvar JSON");
    connect(btn_save, &QToolButton::clicked, this, &MainWindow::onSaveFile);
    toolbar->addWidget(btn_save);

    auto* btn_example = new QToolButton(toolbar);
    btn_example->setIcon(ModernTheme::icon("example"));
    btn_example->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn_example->setText("Exemplo Padrão");
    btn_example->setToolTip("Carrega o cenário acadêmico completo com 45/50 min, turmas, laboratórios e preferências");
    connect(btn_example, &QToolButton::clicked, this, &MainWindow::onLoadDefaultExample);
    toolbar->addWidget(btn_example);
    mutating_widgets_.append(btn_example);

    toolbar->addSeparator();

    auto* btn_export_html = new QToolButton(toolbar);
    btn_export_html->setIcon(ModernTheme::icon("export_html"));
    btn_export_html->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn_export_html->setText("Exportar HTML");
    btn_export_html->setToolTip("Gera relatório visual em HTML pronto para impressão");
    connect(btn_export_html, &QToolButton::clicked, this, &MainWindow::onExportHtml);
    toolbar->addWidget(btn_export_html);

    auto* btn_export_csv = new QToolButton(toolbar);
    btn_export_csv->setIcon(ModernTheme::icon("export_csv"));
    btn_export_csv->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    btn_export_csv->setText("Exportar CSV");
    connect(btn_export_csv, &QToolButton::clicked, this, &MainWindow::onExportCsv);
    toolbar->addWidget(btn_export_csv);
}

/**
 * @brief Constrói o painel horizontal de cartões com os indicadores-chave de desempenho (KPIs).
 */
void MainWindow::setupKpiBanner() {
    kpi_banner_card_ = new QFrame(this);
    kpi_banner_card_->setFrameShape(QFrame::StyledPanel);
    kpi_banner_card_->setObjectName("kpiBannerCard");
    kpi_banner_card_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    updateKpiBannerTheme();

    auto* layout = new FlowLayout(kpi_banner_card_, 6, 16, 4);

    auto make_kpi = [this](const QString& label, QLabel*& val_label, const QString& init_val, const QString& color = "") {
        auto* item = new QWidget(kpi_banner_card_);
        auto* box = new QVBoxLayout(item);
        box->setContentsMargins(4, 2, 4, 2);
        box->setSpacing(1);
        val_label = new QLabel(init_val, item);
        if (color.isEmpty()) {
            val_label->setStyleSheet("font-size: 14px; font-weight: bold; color: palette(highlight);");
        } else {
            val_label->setStyleSheet(QString("font-size: 14px; font-weight: bold; color: %1;").arg(color));
        }
        auto* lbl = new QLabel(label, item);
        lbl->setStyleSheet("font-size: 11px; font-weight: 500;");
        box->addWidget(val_label);
        box->addWidget(lbl);
        return item;
    };

    layout->addWidget(make_kpi("Status do Solver", lbl_kpi_status_, "NÃO EXECUTADO", "#64748b"));
    layout->addWidget(make_kpi("Tempo de Resolução", lbl_kpi_tempo_, "0.0 ms"));
    layout->addWidget(make_kpi("Aulas Alocadas", lbl_kpi_aulas_, "0 / 0", "#16a34a"));
    layout->addWidget(make_kpi("Janelas Professores (J)", lbl_kpi_janelas_, "0 min", "#d97706"));
    layout->addWidget(make_kpi("Dias Trabalhados (D)", lbl_kpi_dias_, "0"));
    layout->addWidget(make_kpi("Penalidade Pref. Dias (W)", lbl_kpi_pref_dias_, "0"));
    layout->addWidget(make_kpi("Choques Ativos", lbl_kpi_choques_, "0", "#16a34a"));
}

/**
 * @brief Atualiza o estilo visual e o fundo do banner de KPIs de acordo com o tema ativo.
 */
void MainWindow::updateKpiBannerTheme() {
    if (!kpi_banner_card_) {
        return;
    }
    if (ModernTheme::isDarkTheme()) {
        kpi_banner_card_->setStyleSheet(
            "QFrame#kpiBannerCard { background-color: #1e293b; border: 1px solid #334155; border-radius: 8px; padding: 4px; }"
            "QFrame#kpiBannerCard QLabel { background-color: transparent; }");
    } else {
        kpi_banner_card_->setStyleSheet(
            "QFrame#kpiBannerCard { background-color: palette(base); border: 1px solid palette(mid); border-radius: 8px; padding: 4px; }"
            "QFrame#kpiBannerCard QLabel { background-color: transparent; }");
    }
}

/**
 * @brief Atualiza os valores e indicadores visuais dos KPIs a partir das estatísticas da grade atual.
 */
void MainWindow::updateKpiBanner() {
    updateKpiBannerTheme();

    const auto& s = data_.grade().estatisticas();
    lbl_kpi_status_->setText(QString::fromStdString(s.status));
    if (s.status == "OPTIMAL") {
        lbl_kpi_status_->setStyleSheet("font-size: 14px; font-weight: bold; color: #16a34a;");
    } else if (s.status == "FEASIBLE") {
        lbl_kpi_status_->setStyleSheet("font-size: 14px; font-weight: bold; color: #0284c7;");
    } else if (s.status == "INFEASIBLE") {
        lbl_kpi_status_->setStyleSheet("font-size: 14px; font-weight: bold; color: #dc2626;");
    } else {
        lbl_kpi_status_->setStyleSheet("font-size: 14px; font-weight: bold; color: #64748b;");
    }

    lbl_kpi_tempo_->setText(QString::number(s.tempo_execucao_ms, 'f', 1) + " ms");
    lbl_kpi_aulas_->setText(QString("%1 / %2").arg(s.total_aulas_alocadas).arg(s.total_aulas_esperadas));
    lbl_kpi_janelas_->setText(QString::number(s.penalidade_janelas_J) + " min");
    lbl_kpi_dias_->setText(QString::number(s.penalidade_dias_D));
    lbl_kpi_pref_dias_->setText(QString::number(s.penalidade_preferencia_dias_W));

    const auto conflitos = data_.verificarConflitosGradeDetalhado();
    if (conflitos.empty()) {
        lbl_kpi_choques_->setText("0");
        lbl_kpi_choques_->setStyleSheet("font-size: 14px; font-weight: bold; color: #16a34a;");
        lbl_kpi_choques_->setToolTip("Nenhum choque de turma, professor, sala ou indisponibilidade detectado.");
    } else {
        int n = static_cast<int>(conflitos.size());
        lbl_kpi_choques_->setText(QString("⚠️ %1 %2").arg(n).arg(n == 1 ? "aula" : "aulas"));
        lbl_kpi_choques_->setStyleSheet("font-size: 14px; font-weight: bold; color: #dc2626;");
        QStringList tip_lines;
        int shown = 0;
        for (const auto& [aid, infos] : conflitos) {
            if (shown >= 6) {
                tip_lines << QString("... e mais %1 aula(s) com choque.").arg(n - shown);
                break;
            }
            const auto* al = data_.grade().findAula(aid);
            QString title = al ? QString::fromStdString(al->disciplina_nome) : QString::fromStdString(aid);
            if (!infos.empty()) {
                tip_lines << QString("• %1: %2").arg(title, QString::fromStdString(infos.front().short_label));
            }
            shown++;
        }
        lbl_kpi_choques_->setToolTip(tip_lines.join("\n"));
    }
}

/**
 * @brief Constrói a aba de gerenciamento de professores, incluindo tabela, busca e ações CRUD.
 * @return QWidget* da aba de professores.
 */
QWidget* MainWindow::createProfessoresTab() {
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* top_bar = new QWidget(widget);
    top_bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto* btn_bar = new FlowLayout(top_bar, 0, 8, 6);
    auto* btn_add = new QPushButton(ModernTheme::icon("add"), "Novo Professor", top_bar);
    auto* btn_edit = new QPushButton(ModernTheme::icon("edit"), "Editar", top_bar);
    btn_edit->setObjectName("btnSecondary");
    auto* btn_rem = new QPushButton(ModernTheme::icon("delete"), "Remover", top_bar);
    btn_rem->setObjectName("btnDanger");
    btn_rem->setToolTip("Remover professor(es) selecionado(s) em massa (Delete)");
    btn_bar->addWidget(btn_add);
    btn_bar->addWidget(btn_edit);
    btn_bar->addWidget(btn_rem);
    mutating_widgets_.append(btn_add);
    mutating_widgets_.append(btn_edit);
    mutating_widgets_.append(btn_rem);

    table_professores_ = new QTableWidget(widget);
    table_professores_->setColumnCount(6);
    table_professores_->setHorizontalHeaderLabels({"ID", "Nome", "Pesos Semanais (SEG..SEX)", "Carga Máx. Diária", "Disponibilidade", "Carga Horária (S1 / S2)"});
    table_professores_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_professores_->horizontalHeader()->setMinimumSectionSize(75);
    table_professores_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table_professores_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_professores_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_professores_->setColumnHidden(0, true);
    layout->addWidget(table_professores_);

    search_bar_professores_ = new TableSearchSortBar(top_bar);
    search_bar_professores_->setupWithTable(table_professores_, "professor", "professores", 1 /* Nome */);
    btn_bar->addWidget(search_bar_professores_);
    layout->insertWidget(0, top_bar);

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
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* top_bar = new QWidget(widget);
    top_bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto* btn_bar = new FlowLayout(top_bar, 0, 8, 6);
    auto* btn_add = new QPushButton(ModernTheme::icon("add"), "Nova Turma", top_bar);
    auto* btn_edit = new QPushButton(ModernTheme::icon("edit"), "Editar", top_bar);
    btn_edit->setObjectName("btnSecondary");
    auto* btn_rem = new QPushButton(ModernTheme::icon("delete"), "Remover", top_bar);
    btn_rem->setObjectName("btnDanger");
    btn_rem->setToolTip("Remover turma(s) selecionada(s) em massa (Delete)");
    btn_bar->addWidget(btn_add);
    btn_bar->addWidget(btn_edit);
    btn_bar->addWidget(btn_rem);
    mutating_widgets_.append(btn_add);
    mutating_widgets_.append(btn_edit);
    mutating_widgets_.append(btn_rem);

    table_turmas_ = new QTableWidget(widget);
    table_turmas_->setColumnCount(7);
    table_turmas_->setHorizontalHeaderLabels({"ID", "Nome", "Turno", "Jornada", "Intervalos Obrigatórios (2)", "Sala Teórica", "Carga Semanal (S1 / S2 / Disp)"});
    table_turmas_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_turmas_->horizontalHeader()->setMinimumSectionSize(75);
    table_turmas_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table_turmas_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_turmas_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_turmas_->setColumnHidden(0, true);
    layout->addWidget(table_turmas_);

    search_bar_turmas_ = new TableSearchSortBar(top_bar);
    search_bar_turmas_->setupWithTable(table_turmas_, "turma", "turmas", 1 /* Nome */);
    btn_bar->addWidget(search_bar_turmas_);
    layout->insertWidget(0, top_bar);

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
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* top_bar = new QWidget(widget);
    top_bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto* btn_bar = new FlowLayout(top_bar, 0, 8, 6);
    auto* btn_add = new QPushButton(ModernTheme::icon("add"), "Nova Sala", top_bar);
    auto* btn_edit = new QPushButton(ModernTheme::icon("edit"), "Editar", top_bar);
    btn_edit->setObjectName("btnSecondary");
    auto* btn_rem = new QPushButton(ModernTheme::icon("delete"), "Remover", top_bar);
    btn_rem->setObjectName("btnDanger");
    btn_rem->setToolTip("Remover sala(s) selecionada(s) em massa (Delete)");
    btn_bar->addWidget(btn_add);
    btn_bar->addWidget(btn_edit);
    btn_bar->addWidget(btn_rem);
    mutating_widgets_.append(btn_add);
    mutating_widgets_.append(btn_edit);
    mutating_widgets_.append(btn_rem);

    table_salas_ = new QTableWidget(widget);
    table_salas_->setColumnCount(6);
    table_salas_->setHorizontalHeaderLabels({"ID", "Nome", "Tipo", "Capacidade", "Recursos", "Carga Horária (S1 / S2)"});
    table_salas_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_salas_->horizontalHeader()->setMinimumSectionSize(75);
    table_salas_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table_salas_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_salas_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_salas_->setColumnHidden(0, true);
    layout->addWidget(table_salas_);

    search_bar_salas_ = new TableSearchSortBar(top_bar);
    search_bar_salas_->setupWithTable(table_salas_, "sala", "salas", 1 /* Nome */);
    btn_bar->addWidget(search_bar_salas_);
    layout->insertWidget(0, top_bar);

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
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* top_bar = new QWidget(widget);
    top_bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    auto* btn_bar = new FlowLayout(top_bar, 0, 8, 6);
    auto* btn_add = new QPushButton(ModernTheme::icon("add"), "Nova Disciplina", top_bar);
    auto* btn_edit = new QPushButton(ModernTheme::icon("edit"), "Editar", top_bar);
    btn_edit->setObjectName("btnSecondary");
    auto* btn_rem = new QPushButton(ModernTheme::icon("delete"), "Remover", top_bar);
    btn_rem->setObjectName("btnDanger");
    btn_rem->setToolTip("Remover disciplina(s) selecionada(s) em massa (Delete)");
    btn_bar->addWidget(btn_add);
    btn_bar->addWidget(btn_edit);
    btn_bar->addWidget(btn_rem);
    mutating_widgets_.append(btn_add);
    mutating_widgets_.append(btn_edit);
    mutating_widgets_.append(btn_rem);

    table_disciplinas_ = new QTableWidget(widget);
    table_disciplinas_->setColumnCount(8);
    table_disciplinas_->setHorizontalHeaderLabels({"ID", "Nome", "Regime / Semestre", "Tipo", "Duração", "Aulas/Sem", "Professor", "Turma"});
    table_disciplinas_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_disciplinas_->horizontalHeader()->setMinimumSectionSize(75);
    table_disciplinas_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table_disciplinas_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_disciplinas_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_disciplinas_->setColumnHidden(0, true);
    layout->addWidget(table_disciplinas_);

    search_bar_disciplinas_ = new TableSearchSortBar(top_bar);
    search_bar_disciplinas_->setupWithTable(table_disciplinas_, "disciplina", "disciplinas", 1 /* Nome */);
    btn_bar->addWidget(search_bar_disciplinas_);
    layout->insertWidget(0, top_bar);

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

    auto* btn_clear = new QPushButton(ModernTheme::icon("delete"), "Limpar Logs", widget);
    btn_clear->setObjectName("btnSecondary");
    btn_clear->setFixedWidth(135);
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
    if (is_optimizing_) return;
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
    if (is_optimizing_) return;
    QString path = QFileDialog::getOpenFileName(this, "Abrir Arquivo JSON", "", "Arquivos JSON (*.json)");
    if (path.isEmpty()) return;
    loadProjectFile(path);
}

bool MainWindow::loadProjectFile(const QString& path) {
    std::string err;
    if (serialization::JsonSerialization::carregarArquivo(path.toStdString(), data_, err)) {
        data_.gerarOcorrenciasAulas(true, model::EscopoSemestreOtimizacao::TODAS_SEM_FILTRO);
        refreshAllTables();
        config_widget_->loadConfig(data_.config());
        timetable_view_->refreshView();
        updateKpiBanner();
        txt_logs_->appendPlainText("[ARQUIVO] Carregado com sucesso: " + path);
        statusBar()->showMessage("Arquivo carregado com sucesso.", 4000);
        return true;
    }
    if (isVisible()) {
        QMessageBox::critical(this, "Erro ao Abrir", QString::fromStdString(err));
    }
    return false;
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
    if (is_optimizing_) return;
    data_ = model::InstitutionData::criarExemploPadrao();
    refreshAllTables();
    config_widget_->loadConfig(data_.config());
    timetable_view_->refreshView();
    updateKpiBanner();
    txt_logs_->appendPlainText("[EXEMPLO] Cenário padrão carregado: 6 Professores, 6 Salas/Labs, 3 Turmas, 19 Disciplinas mistas (45 e 50 min).");
    statusBar()->showMessage("Cenário de exemplo carregado. Clique em '⚡ OTIMIZAR GRADE' para executar.", 5000);
}

void MainWindow::setOptimizationUiState(bool running) {
    is_optimizing_ = running;
    stop_requested_ui_ = false;

    if (btn_optimize_) {
        if (running) {
            btn_optimize_->setObjectName("btnStopOptimize");
            btn_optimize_->setIcon(ModernTheme::icon("stop"));
            btn_optimize_->setText("PARAR OTIMIZAÇÃO");
            btn_optimize_->setToolTip("Interromper a otimização em andamento (aplica melhor solução viável encontrada)");
        } else {
            btn_optimize_->setObjectName("btnOptimize");
            btn_optimize_->setIcon(ModernTheme::icon("optimize"));
            btn_optimize_->setText("OTIMIZAR GRADE");
            btn_optimize_->setToolTip("Selecionar algoritmo e otimizar grade horária escolar (Seção 4 & 7)");
        }
        btn_optimize_->setEnabled(true);
        if (running) {
            btn_optimize_->clearFocus();
        }
        btn_optimize_->style()->unpolish(btn_optimize_);
        btn_optimize_->style()->polish(btn_optimize_);
        btn_optimize_->update();
    }

    for (auto* w : mutating_widgets_) {
        if (w) w->setEnabled(!running);
    }
    for (auto* a : mutating_actions_) {
        if (a) a->setEnabled(!running);
    }
    if (timetable_view_) {
        timetable_view_->setReadOnlyMode(running);
    }

    if (running) {
        progress_bar_->setRange(0, 100);
        progress_bar_->setValue(5);
        progress_bar_->setVisible(true);
        optimization_elapsed_.restart();
        if (elapsed_timer_) elapsed_timer_->start();
    } else {
        if (elapsed_timer_) elapsed_timer_->stop();
        progress_bar_->setVisible(false);
    }
}

void MainWindow::updateOptimizationStatusBar() {
    if (!is_optimizing_) return;
    qint64 secs = optimization_elapsed_.isValid() ? (optimization_elapsed_.elapsed() / 1000) : 0;
    if (stop_requested_ui_) {
        statusBar()->showMessage(QString("[%1] Interrompendo otimização... (%2s)")
            .arg(active_algo_name_).arg(secs));
    } else {
        statusBar()->showMessage(QString("[%1] %2 (%3s)")
            .arg(active_algo_name_, last_solver_msg_).arg(secs));
    }
}

void MainWindow::onOptimizationProgress(const QString& algo_name, const QString& msg, double ratio) {
    if (!is_optimizing_) return;
    active_algo_name_ = algo_name;
    last_solver_msg_ = msg;
    int pct = std::clamp(static_cast<int>(ratio * 100.0), 0, 100);
    progress_bar_->setValue(pct);
    txt_logs_->appendPlainText("[SOLVER] " + msg);
    updateOptimizationStatusBar();
}

void MainWindow::onStopOptimization() {
    if (!is_optimizing_ || stop_requested_ui_) return;
    stop_requested_ui_ = true;
    if (btn_optimize_) {
        btn_optimize_->setEnabled(false);
        btn_optimize_->setText("PARANDO...");
    }
    txt_logs_->appendPlainText("[OTIMIZAÇÃO] Solicitação de parada enviada ao solver...");
    updateOptimizationStatusBar();
    solver_.requestStop();
}

void MainWindow::onOptimizationFinished(model::Timetable tt,
                                        bool was_stopped,
                                        model::EscopoSemestreOtimizacao solved_scope) {
    if (solver_thread_.joinable()) {
        solver_thread_.join();
    }

    setOptimizationUiState(false);

    const bool had_previous_grade = !data_.grade().alocacoes().empty();
    const auto s = tt.estatisticas();
    txt_logs_->appendPlainText(QString("[RESULTADO] Status: %1 | Tempo: %2 ms | Objetivo: %3")
        .arg(QString::fromStdString(s.status))
        .arg(s.tempo_execucao_ms, 0, 'f', 1)
        .arg(s.valor_objetivo, 0, 'f', 1));

    for (const auto& msg : tt.mensagensDiagnostico()) {
        txt_logs_->appendPlainText(QString::fromStdString(msg));
    }

    if (s.status == "OPTIMAL" || s.status == "FEASIBLE") {
        const bool applied = data_.aplicarResultadoOtimizacao(std::move(tt), solved_scope);
        updateKpiBanner();
        timetable_view_->refreshView();
        if (applied) {
            refreshProfessoresTable();
            refreshSalasTable();
            const int total_alocadas = data_.grade().estatisticas().total_aulas_alocadas;
            if (was_stopped && s.status != "OPTIMAL") {
                statusBar()->showMessage(
                    QString("Otimização interrompida pelo usuário. Melhor solução viável aplicada (%1 aulas alocadas).")
                        .arg(total_alocadas),
                    8000);
            } else {
                statusBar()->showMessage(
                    QString("Otimização concluída com sucesso (%1)! %2 aulas alocadas.")
                        .arg(QString::fromStdString(s.status)).arg(total_alocadas),
                    6000);
            }
            tab_widget_->setCurrentIndex(0); // Alterna para a visualização da grade
        } else {
            txt_logs_->appendPlainText("[SISTEMA] Solução retornada sem alocações. Grade anterior mantida.");
            statusBar()->showMessage("Solução sem alocações. Grade anterior mantida.", 6000);
        }
    } else if (s.status == "CANCELLED" ||
               (was_stopped && s.status != "INFEASIBLE" && s.status != "DADOS_INVALIDOS" && s.status != "MODEL_INVALID")) {
        statusBar()->showMessage("Otimização interrompida pelo usuário. Grade anterior mantida.", 6000);
    } else {
        data_.aplicarResultadoOtimizacao(std::move(tt), solved_scope);
        updateKpiBanner();
        timetable_view_->refreshView();
        if (had_previous_grade) {
            txt_logs_->appendPlainText(
                QString("[SISTEMA] A grade anterior (%1 aulas) foi mantida intacta.")
                    .arg(data_.grade().alocacoes().size()));
        }
        const QString kept_suffix = had_previous_grade ? " A grade anterior foi mantida." : "";
        const QString kept_modal = had_previous_grade
            ? "\n\nA grade anterior com suas aulas fixadas foi mantida intacta."
            : "";
        if (s.status == "DADOS_INVALIDOS") {
            statusBar()->showMessage("Falha na validação dos dados acadêmicos." + kept_suffix, 6000);
            QMessageBox::warning(this, "Dados Inválidos",
                "Foram encontrados problemas nos dados cadastrados que impedem a otimização.\n\n"
                "Verifique os detalhes na aba '📝 Diagnóstico & Logs'." + kept_modal);
        } else if (s.status == "INFEASIBLE") {
            statusBar()->showMessage("Falha na otimização: Restrições inviáveis." + kept_suffix, 6000);
            QMessageBox::warning(this, "Inviabilidade",
                "O solver não encontrou solução viável que satisfaça todas as restrições obrigatórias.\n\n"
                "Verifique os diagnósticos na aba '📝 Diagnóstico & Logs'." + kept_modal);
        } else {
            statusBar()->showMessage("Otimização encerrada sem encontrar solução viável." + kept_suffix, 6000);
            QMessageBox::warning(this, "Otimização Sem Solução Viável",
                "O solver encerrou a busca antes de encontrar uma solução viável completa "
                "(possível limite de tempo, limite de memória RAM ou erro do modelo).\n\n"
                "Verifique os detalhes e limites na aba '📝 Diagnóstico & Logs'." + kept_modal);
        }
        tab_widget_->setCurrentIndex(6); // Alterna para a aba de logs
    }
}

/**
 * @brief Exibe o diálogo de seleção do algoritmo e parâmetros e executa a otimização em background.
 */
void MainWindow::onOptimize() {
    if (is_optimizing_) {
        onStopOptimization();
        return;
    }

    if (solver_thread_.joinable()) {
        solver_thread_.join();
    }

    auto current_cfg = config_widget_->getConfig();

    // Apresenta a caixa para seleção do algoritmo para resolver o problema
    OptimizeDialog dlg(current_cfg, static_cast<int>(data_.grade().alocacoes().size()), this);
    if (dlg.exec() != QDialog::Accepted) {
        return; // Usuário cancelou a operação
    }

    dlg.updateConfig(current_cfg);
    data_.setConfig(current_cfg);
    config_widget_->loadConfig(current_cfg);

    data_.gerarOcorrenciasAulas(true, model::EscopoSemestreOtimizacao::TODAS_SEM_FILTRO);

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
    txt_logs_->appendPlainText("[OTIMIZAÇÃO] Iniciando resolução em segundo plano: " + QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm:ss"));

    model::InstitutionData data_snapshot = data_;
    data_snapshot.gerarOcorrenciasAulas(true, current_cfg.escopo_semestre);
    const auto solved_scope = current_cfg.escopo_semestre;
    QString algo_q = QString::fromStdString(algo_name);
    active_algo_name_ = algo_q;
    last_solver_msg_ = "Iniciando resolução...";
    solver_.resetStop();
    setOptimizationUiState(true);
    updateOptimizationStatusBar();

    QPointer<MainWindow> self(this);
    solver_thread_ = std::thread([this, self, data_snapshot = std::move(data_snapshot), algo_q, solved_scope]() mutable {
        auto tt = solver_.solve(data_snapshot, [self, algo_q](const std::string& msg, double ratio) {
            if (!self) return;
            QString msg_q = QString::fromStdString(msg);
            QMetaObject::invokeMethod(self, [self, algo_q, msg_q, ratio]() {
                if (self) {
                    self->onOptimizationProgress(algo_q, msg_q, ratio);
                }
            }, Qt::QueuedConnection);
        }, /*reset_stop=*/false);
        bool was_stopped = solver_.isStopRequested();
        if (self) {
            QMetaObject::invokeMethod(self, [self, tt = std::move(tt), was_stopped, solved_scope]() mutable {
                if (self) {
                    self->onOptimizationFinished(std::move(tt), was_stopped, solved_scope);
                }
            }, Qt::QueuedConnection);
        }
    });
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
    const bool is_dark = ModernTheme::isDarkTheme();
    const bool has_grade = !data_.grade().alocacoes().empty();

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

        QString carga_max = (p.cargaMaximaDiariaMinutos() > 0)
            ? QString::number(p.cargaMaximaDiariaMinutos()) + " min"
            : "Sem limite";
        table_professores_->setItem(r, 3, new SmartTableWidgetItem(carga_max));

        const auto carga = data_.calcularCargaProfessor(p.id());

        int total_inters = 0;
        QStringList disp_lines;
        for (auto d : model::allDays()) {
            const auto& inters = p.disponibilidade(d);
            total_inters += static_cast<int>(inters.size());
            if (!inters.empty()) {
                QStringList day_parts;
                for (const auto& inter : inters) {
                    day_parts << QString::fromStdString(inter.toString());
                }
                disp_lines << QString("• <b>%1</b>: %2")
                                  .arg(QString::fromStdString(model::dayToString(d)),
                                       day_parts.join(", "));
            }
        }
        const QString disp_text = (total_inters == 0)
            ? QString("Sem disponibilidade")
            : QString("%1 %2 (%3/sem)")
                  .arg(total_inters)
                  .arg(total_inters == 1 ? "turno" : "turnos",
                       formatHorasMin(carga.minutos_disponiveis));
        auto* item_disp = new SmartTableWidgetItem(disp_text);
        item_disp->setData(SmartTableWidgetItem::SortKeyRole, static_cast<qlonglong>(carga.minutos_disponiveis));
        QString disp_tip = QString("<b>Disponibilidade Semanal:</b> %1 min (%2)")
                               .arg(carga.minutos_disponiveis)
                               .arg(formatHorasMin(carga.minutos_disponiveis));
        if (!disp_lines.isEmpty()) {
            disp_tip += "<br>" + disp_lines.join("<br>");
        }
        item_disp->setToolTip(disp_tip);
        table_professores_->setItem(r, 4, item_disp);

        const QString carga_str = (carga.minutos_s1 == 0 && carga.minutos_s2 == 0)
            ? QString("0h00 (0a)")
            : QString("1º: %1 (%2a) | 2º: %3 (%4a)")
                  .arg(formatHorasMin(carga.minutos_s1))
                  .arg(carga.aulas_s1)
                  .arg(formatHorasMin(carga.minutos_s2))
                  .arg(carga.aulas_s2);
        auto* item_carga = new SmartTableWidgetItem(carga_str);
        item_carga->setData(SmartTableWidgetItem::SortKeyRole,
                            static_cast<qlonglong>(std::max(carga.minutos_s1, carga.minutos_s2)));

        QString tip_carga = QString(
            "<b>Carga Horária Semanal (Disciplinas Marcadas):</b><br>"
            "• <b>1º Semestre:</b> %1 aula(s) — %2 min (%3)<br>"
            "• <b>2º Semestre:</b> %4 aula(s) — %5 min (%6)<br>"
            "• <b>Disponibilidade Semanal:</b> %7 min (%8) em %9 dia(s)")
            .arg(carga.aulas_s1).arg(carga.minutos_s1).arg(formatHorasMin(carga.minutos_s1))
            .arg(carga.aulas_s2).arg(carga.minutos_s2).arg(formatHorasMin(carga.minutos_s2))
            .arg(carga.minutos_disponiveis).arg(formatHorasMin(carga.minutos_disponiveis))
            .arg(carga.dias_com_disponibilidade);

        if (carga.excedeDisponibilidade()) {
            tip_carga += "<br><span style='color:#ef4444;'><b>⚠️ Atenção:</b> A carga semanal em ao menos um semestre excede a disponibilidade cadastrada!</span>";
            item_carga->setForeground(QBrush(QColor(239, 68, 68)));
        } else if (carga.minutos_s1 > 0 || carga.minutos_s2 > 0) {
            item_carga->setForeground(QBrush(is_dark ? QColor(52, 211, 153) : QColor(22, 163, 74)));
            if (p.cargaMaximaDiariaMinutos() > 0 && carga.dias_com_disponibilidade > 0) {
                const int cap_diaria_sem = p.cargaMaximaDiariaMinutos() * carga.dias_com_disponibilidade;
                if (carga.minutos_s1 > cap_diaria_sem || carga.minutos_s2 > cap_diaria_sem) {
                    tip_carga += QString("<br><span style='color:#f59e0b;'><b>⚠️ Aviso:</b> Carga excede o limite diário (%1 min × %2 dias = %3 min).</span>")
                                     .arg(p.cargaMaximaDiariaMinutos())
                                     .arg(carga.dias_com_disponibilidade)
                                     .arg(cap_diaria_sem);
                }
            }
        } else {
            item_carga->setForeground(QBrush(is_dark ? QColor(148, 163, 184) : QColor(100, 116, 139)));
        }

        if (has_grade) {
            tip_carga += QString("<br>• <b>Alocado na Grade Atual:</b> 1º: %1a (%2) | 2º: %3a (%4)")
                             .arg(carga.alocado_aulas_s1).arg(formatHorasMin(carga.alocado_minutos_s1))
                             .arg(carga.alocado_aulas_s2).arg(formatHorasMin(carga.alocado_minutos_s2));
        }

        tip_carga += "<br><br><b>Disciplinas Marcadas:</b><br>" + formatDisciplinasTooltip(carga.disciplinas);
        item_carga->setToolTip(tip_carga);
        table_professores_->setItem(r, 5, item_carga);
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
            tooltip_str += QString("• <b>%1</b>: %2 (%3 slots) [Intervalos: %4]<br>")
                .arg(QString::fromStdString(model::dayToString(d)))
                .arg(QString::fromStdString(jd.jornadaTotal().toString()))
                .arg(jd.slots_letivos.size())
                .arg(ints.isEmpty() ? "Nenhum" : ints.join(", "));
        }

        if (t.temExcecoes()) {
            QStringList diff_parts;
            for (const auto& [d, exc] : t.excecoesPorDia()) {
                diff_parts << QString("%1: %2 (%3 slots)").arg(QString::fromStdString(model::dayToString(d)),
                                                                QString::fromStdString(exc.jornadaTotal().toString()))
                                                           .arg(exc.slots_letivos.size());
            }
            jornada_str = QString("%1 (%2)")
                .arg(QString::fromStdString(t.jornadaPadrao().jornadaTotal().toString()),
                     diff_parts.join("; "));
            intervals_str = "Personalizado por dia (passe o mouse)";
        } else {
            const auto& jp = t.jornadaPadrao();
            jornada_str = QString("%1 (%2 slots)").arg(QString::fromStdString(jp.jornadaTotal().toString())).arg(jp.slots_letivos.size());
            QStringList int_strs;
            for (size_t idx_i = 0; idx_i < jp.intervalosAtivos().size(); ++idx_i) {
                int_strs << QString("Int%1: %2").arg(idx_i + 1).arg(QString::fromStdString(jp.intervalosAtivos()[idx_i].toString()));
            }
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

        // Calcular carga horária semanal da turma por semestre vs disponível
        int min_disp = 0;
        for (auto d : model::allDays()) {
            if (t.funcionaNoDia(d)) {
                min_disp += t.jornadaParaDia(d).minutosLetivos();
            }
        }

        int min_s1 = 0;
        int min_s2 = 0;
        for (const auto& d : data_.disciplinas()) {
            const auto& t_ids = d.turmas();
            if (std::find(t_ids.begin(), t_ids.end(), t.id()) != t_ids.end()) {
                int dur = d.duracaoMinutos() * d.aulasSemanais();
                if (d.isAnual()) {
                    min_s1 += dur;
                    min_s2 += dur;
                } else if (d.semestre() == model::SemestreLetivo::SEMESTRE_1) {
                    min_s1 += dur;
                } else if (d.semestre() == model::SemestreLetivo::SEMESTRE_2) {
                    min_s2 += dur;
                }
            }
        }

        QString carga_str = QString("1º: %1m | 2º: %2m (Disp: %3m)")
            .arg(min_s1).arg(min_s2).arg(min_disp);
        auto* item_carga = new SmartTableWidgetItem(carga_str);
        QString tip_carga = QString("<b>Distribuição de Carga Horária Semanal:</b><br>"
                                    "• <b>1º Semestre:</b> %1 min (%2h%3m)<br>"
                                    "• <b>2º Semestre:</b> %4 min (%5h%6m)<br>"
                                    "• <b>Disponível na Jornada:</b> %7 min (%8h%9m)")
                                    .arg(min_s1).arg(min_s1 / 60).arg(min_s1 % 60)
                                    .arg(min_s2).arg(min_s2 / 60).arg(min_s2 % 60)
                                    .arg(min_disp).arg(min_disp / 60).arg(min_disp % 60);

        if (min_s1 > min_disp || min_s2 > min_disp) {
            tip_carga += "<br><span style='color:#ef4444;'><b>⚠️ Atenção:</b> A carga de um dos semestres excede o tempo semanal disponível nesta turma!</span>";
            item_carga->setForeground(QBrush(QColor(239, 68, 68)));
        } else {
            bool is_dark = ModernTheme::isDarkTheme();
            item_carga->setForeground(QBrush(is_dark ? QColor(52, 211, 153) : QColor(22, 163, 74)));
        }
        item_carga->setToolTip(tip_carga);
        table_turmas_->setItem(r, 6, item_carga);
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
    const bool is_dark = ModernTheme::isDarkTheme();
    const bool has_grade = !data_.grade().alocacoes().empty();

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

        const auto carga = data_.calcularCargaSala(s.id());
        const bool has_fixo = (carga.fixo_minutos_s1 > 0 || carga.fixo_minutos_s2 > 0);
        const bool has_alt = (carga.alt_minutos_s1 > 0 || carga.alt_minutos_s2 > 0);

        QString carga_str;
        if (!has_fixo && !has_alt) {
            carga_str = "0h00 (0a)";
        } else if (has_fixo) {
            carga_str = QString("1º: %1 (%2a) | 2º: %3 (%4a)")
                            .arg(formatHorasMin(carga.fixo_minutos_s1))
                            .arg(carga.fixo_aulas_s1)
                            .arg(formatHorasMin(carga.fixo_minutos_s2))
                            .arg(carga.fixo_aulas_s2);
            if (has_alt) {
                carga_str += " (+alt)";
            }
        } else {
            carga_str = QString("Alt: 1º: %1 (%2a) | 2º: %3 (%4a)")
                            .arg(formatHorasMin(carga.alt_minutos_s1))
                            .arg(carga.alt_aulas_s1)
                            .arg(formatHorasMin(carga.alt_minutos_s2))
                            .arg(carga.alt_aulas_s2);
        }

        auto* item_carga = new SmartTableWidgetItem(carga_str);
        const qlonglong sort_key =
            static_cast<qlonglong>(std::max(carga.fixo_minutos_s1, carga.fixo_minutos_s2)) * 100000LL +
            static_cast<qlonglong>(std::max(carga.alt_minutos_s1, carga.alt_minutos_s2));
        item_carga->setData(SmartTableWidgetItem::SortKeyRole, sort_key);

        if (has_fixo) {
            item_carga->setForeground(QBrush(is_dark ? QColor(52, 211, 153) : QColor(22, 163, 74)));
        } else if (has_alt) {
            item_carga->setForeground(QBrush(is_dark ? QColor(56, 189, 248) : QColor(2, 132, 199)));
        } else {
            item_carga->setForeground(QBrush(is_dark ? QColor(148, 163, 184) : QColor(100, 116, 139)));
        }

        QString tip_carga = QString(
            "<b>Carga Horária Semanal da Sala / Laboratório:</b><br>"
            "• <b>Fixa / Exclusiva — 1º Sem:</b> %1 aula(s) — %2 min (%3)<br>"
            "• <b>Fixa / Exclusiva — 2º Sem:</b> %4 aula(s) — %5 min (%6)")
            .arg(carga.fixo_aulas_s1).arg(carga.fixo_minutos_s1).arg(formatHorasMin(carga.fixo_minutos_s1))
            .arg(carga.fixo_aulas_s2).arg(carga.fixo_minutos_s2).arg(formatHorasMin(carga.fixo_minutos_s2));

        if (has_alt) {
            tip_carga += QString(
                "<br>• <b>Alternativa / Compartilhada — 1º Sem:</b> %1 aula(s) — %2 min (%3)<br>"
                "• <b>Alternativa / Compartilhada — 2º Sem:</b> %4 aula(s) — %5 min (%6)<br>"
                "<i>(Disciplinas em que esta sala é uma entre múltiplas opções admissíveis.)</i>")
                .arg(carga.alt_aulas_s1).arg(carga.alt_minutos_s1).arg(formatHorasMin(carga.alt_minutos_s1))
                .arg(carga.alt_aulas_s2).arg(carga.alt_minutos_s2).arg(formatHorasMin(carga.alt_minutos_s2));
        }

        if (has_grade) {
            tip_carga += QString("<br>• <b>Alocado na Grade Atual:</b> 1º: %1a (%2) | 2º: %3a (%4)")
                             .arg(carga.alocado_aulas_s1).arg(formatHorasMin(carga.alocado_minutos_s1))
                             .arg(carga.alocado_aulas_s2).arg(formatHorasMin(carga.alocado_minutos_s2));
        }

        if (!carga.disciplinas_fixas.empty()) {
            tip_carga += "<br><br><b>Disciplinas Fixas / Exclusivas:</b><br>" +
                         formatDisciplinasTooltip(carga.disciplinas_fixas);
        }
        if (!carga.disciplinas_alternativas.empty()) {
            tip_carga += "<br><br><b>Disciplinas Alternativas / Compartilhadas:</b><br>" +
                         formatDisciplinasTooltip(carga.disciplinas_alternativas);
        }
        if (carga.disciplinas_fixas.empty() && carga.disciplinas_alternativas.empty()) {
            tip_carga += "<br><br><i>Nenhuma disciplina vinculada a esta sala.</i>";
        }

        item_carga->setToolTip(tip_carga);
        table_salas_->setItem(r, 5, item_carga);
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
    if (is_optimizing_) return;
    ProfessorEditorDialog dlg(this, nullptr, data_.nextProfessorId());
    if (dlg.exec() == QDialog::Accepted) {
        data_.addProfessor(dlg.getProfessor());
        refreshProfessoresTable();
        timetable_view_->refreshView();
        updateKpiBanner();
    }
}

/**
 * @brief Abre o diálogo de edição para o professor atualmente selecionado na tabela.
 */
void MainWindow::onEditProfessor() {
    if (is_optimizing_) return;
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
            updateKpiBanner();
        }
    }
}

/**
 * @brief Solicita confirmação e remove o(s) professor(es) selecionado(s) da instituição.
 */
void MainWindow::onRemoveProfessor() {
    if (is_optimizing_) return;
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
        updateKpiBanner();
    }
}

/**
 * @brief Abre o diálogo de cadastro para adicionar uma nova turma.
 */
void MainWindow::onAddTurma() {
    if (is_optimizing_) return;
    TurmaEditorDialog dlg(data_.salas(), this, nullptr, data_.nextTurmaId(), data_.slotTemplates());
    if (dlg.exec() == QDialog::Accepted) {
        data_.addTurma(dlg.getTurma());
        refreshTurmasTable();
        refreshSalasTable();
        timetable_view_->refreshView();
        updateKpiBanner();
    }
}

/**
 * @brief Abre o diálogo de edição para a turma atualmente selecionada.
 */
void MainWindow::onEditTurma() {
    if (is_optimizing_) return;
    int r = table_turmas_->currentRow();
    if (r < 0 || !table_turmas_->item(r, 0)) return;
    std::string id = table_turmas_->item(r, 0)->text().toStdString();
    auto* t = data_.findTurma(id);
    if (t) {
        TurmaEditorDialog dlg(data_.salas(), this, t, "", data_.slotTemplates());
        if (dlg.exec() == QDialog::Accepted) {
            data_.addTurma(dlg.getTurma());
            refreshTurmasTable();
            refreshSalasTable();
            timetable_view_->refreshView();
            updateKpiBanner();
        }
    }
}

/**
 * @brief Solicita confirmação e remove a(s) turma(s) selecionada(s).
 */
void MainWindow::onRemoveTurma() {
    if (is_optimizing_) return;
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
        refreshSalasTable();
        refreshDisciplinasTable();
        timetable_view_->refreshView();
        updateKpiBanner();
    }
}

/**
 * @brief Abre o diálogo de cadastro para adicionar uma nova sala ou laboratório.
 */
void MainWindow::onAddSala() {
    if (is_optimizing_) return;
    RoomEditorDialog dlg(this, nullptr, data_.nextSalaId());
    if (dlg.exec() == QDialog::Accepted) {
        data_.addSala(dlg.getRoom());
        refreshSalasTable();
        timetable_view_->refreshView();
        updateKpiBanner();
    }
}

/**
 * @brief Abre o diálogo de edição para a sala atualmente selecionada.
 */
void MainWindow::onEditSala() {
    if (is_optimizing_) return;
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
            updateKpiBanner();
        }
    }
}

/**
 * @brief Solicita confirmação e remove a(s) sala(s) selecionada(s).
 */
void MainWindow::onRemoveSala() {
    if (is_optimizing_) return;
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
        refreshTurmasTable();
        refreshDisciplinasTable();
        timetable_view_->refreshView();
        updateKpiBanner();
    }
}

/**
 * @brief Abre o diálogo de cadastro para adicionar uma nova disciplina curricular.
 */
void MainWindow::onAddDisciplina() {
    if (is_optimizing_) return;
    LessonEditorDialog dlg(data_.professores(), data_.turmas(), data_.salas(), this, nullptr, data_.nextDisciplinaId());
    if (dlg.exec() == QDialog::Accepted) {
        data_.addDisciplina(dlg.getDisciplina());
        data_.gerarOcorrenciasAulas(true, model::EscopoSemestreOtimizacao::TODAS_SEM_FILTRO);
        refreshAllTables();
    }
}

/**
 * @brief Abre o diálogo de edição para a disciplina atualmente selecionada.
 */
void MainWindow::onEditDisciplina() {
    if (is_optimizing_) return;
    int r = table_disciplinas_->currentRow();
    if (r < 0 || !table_disciplinas_->item(r, 0)) return;
    std::string id = table_disciplinas_->item(r, 0)->text().toStdString();
    auto* d = data_.findDisciplina(id);
    if (d) {
        LessonEditorDialog dlg(data_.professores(), data_.turmas(), data_.salas(), this, d);
        if (dlg.exec() == QDialog::Accepted) {
            data_.addDisciplina(dlg.getDisciplina());
            data_.gerarOcorrenciasAulas(true, model::EscopoSemestreOtimizacao::TODAS_SEM_FILTRO);
            refreshAllTables();
        }
    }
}

/**
 * @brief Solicita confirmação e remove a(s) disciplina(s) selecionada(s).
 */
void MainWindow::onRemoveDisciplina() {
    if (is_optimizing_) return;
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
        data_.gerarOcorrenciasAulas(true, model::EscopoSemestreOtimizacao::TODAS_SEM_FILTRO);
        refreshAllTables();
        timetable_view_->refreshView();
        updateKpiBanner();
    }
}

} // namespace scheduler::ui
