#include "ui/MainWindow.h"
#include "ui/SmartTableWidgetItem.h"
#include "ui/ProfessorEditorDialog.h"
#include "ui/TurmaEditorDialog.h"
#include "ui/RoomEditorDialog.h"
#include "ui/LessonEditorDialog.h"
#include "ui/OptimizeDialog.h"
#include "ui/ScheduleReportExporter.h"
#include "serialization/JsonSerialization.h"
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

namespace scheduler::ui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setupUi();
    onLoadDefaultExample();
}

void MainWindow::setupUi() {
    setWindowTitle("Escalonador Inteligente de Horários Escolares");
    resize(1300, 850);

    setupToolBar();

    auto* central_widget = new QWidget(this);
    auto* main_layout = new QVBoxLayout(central_widget);
    main_layout->setContentsMargins(12, 8, 12, 12);
    main_layout->setSpacing(10);

    setupKpiBanner();
    main_layout->addWidget(lbl_kpi_status_->parentWidget());

    tab_widget_ = new QTabWidget(this);

    timetable_view_ = new TimetableView(data_, this);
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

void MainWindow::setupKpiBanner() {
    auto* banner_card = new QWidget(this);
    banner_card->setStyleSheet(
        "background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1e293b, stop:1 #131c2e);"
        "border: 1px solid #334155; border-radius: 8px; padding: 6px;");

    auto* layout = new QHBoxLayout(banner_card);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(20);

    auto make_kpi = [](const QString& label, QLabel*& val_label, const QString& init_val, const QString& color = "#38bdf8") {
        auto* box = new QVBoxLayout();
        box->setSpacing(2);
        val_label = new QLabel(init_val);
        val_label->setStyleSheet(QString("font-size: 16px; font-weight: bold; color: %1;").arg(color));
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet("font-size: 11px; color: #94a3b8; font-weight: 500;");
        box->addWidget(val_label);
        box->addWidget(lbl);
        return box;
    };

    layout->addLayout(make_kpi("Status do Solver", lbl_kpi_status_, "NÃO EXECUTADO", "#94a3b8"));
    layout->addLayout(make_kpi("Tempo de Resolução", lbl_kpi_tempo_, "0.0 ms"));
    layout->addLayout(make_kpi("Aulas Alocadas", lbl_kpi_aulas_, "0 / 0", "#10b981"));
    layout->addLayout(make_kpi("Janelas Professores (J)", lbl_kpi_janelas_, "0 min", "#f59e0b"));
    layout->addLayout(make_kpi("Dias Trabalhados (D)", lbl_kpi_dias_, "0"));
    layout->addLayout(make_kpi("Penalidade Pref. Dias (W)", lbl_kpi_pref_dias_, "0"));
    layout->addStretch();
}

void MainWindow::updateKpiBanner() {
    const auto& s = data_.grade().estatisticas();
    lbl_kpi_status_->setText(QString::fromStdString(s.status));
    if (s.status == "OPTIMAL") {
        lbl_kpi_status_->setStyleSheet("font-size: 16px; font-weight: bold; color: #10b981;");
    } else if (s.status == "FEASIBLE") {
        lbl_kpi_status_->setStyleSheet("font-size: 16px; font-weight: bold; color: #38bdf8;");
    } else if (s.status == "INFEASIBLE") {
        lbl_kpi_status_->setStyleSheet("font-size: 16px; font-weight: bold; color: #ef4444;");
    } else {
        lbl_kpi_status_->setStyleSheet("font-size: 16px; font-weight: bold; color: #94a3b8;");
    }

    lbl_kpi_tempo_->setText(QString::number(s.tempo_execucao_ms, 'f', 1) + " ms");
    lbl_kpi_aulas_->setText(QString("%1 / %2").arg(s.total_aulas_alocadas).arg(s.total_aulas_esperadas));
    lbl_kpi_janelas_->setText(QString::number(s.penalidade_janelas_J) + " min");
    lbl_kpi_dias_->setText(QString::number(s.penalidade_dias_D));
    lbl_kpi_pref_dias_->setText(QString::number(s.penalidade_preferencia_dias_W));
}

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
    btn_bar->addWidget(btn_add);
    btn_bar->addWidget(btn_edit);
    btn_bar->addWidget(btn_rem);
    btn_bar->addStretch();

    table_professores_ = new QTableWidget(widget);
    table_professores_->setColumnCount(5);
    table_professores_->setHorizontalHeaderLabels({"ID", "Nome", "Pesos Semanais (SEG..SEX)", "Carga Máx. Diária", "Disponibilidade"});
    table_professores_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_professores_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_professores_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(table_professores_);

    search_bar_professores_ = new TableSearchSortBar(widget);
    search_bar_professores_->setupWithTable(table_professores_, "professor", "professores", 1 /* Nome */);
    btn_bar->addWidget(search_bar_professores_);
    layout->insertLayout(0, btn_bar);

    connect(btn_add, &QPushButton::clicked, this, &MainWindow::onAddProfessor);
    connect(btn_edit, &QPushButton::clicked, this, &MainWindow::onEditProfessor);
    connect(btn_rem, &QPushButton::clicked, this, &MainWindow::onRemoveProfessor);
    connect(table_professores_, &QTableWidget::cellDoubleClicked, this, &MainWindow::onEditProfessor);

    return widget;
}

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
    btn_bar->addWidget(btn_add);
    btn_bar->addWidget(btn_edit);
    btn_bar->addWidget(btn_rem);
    btn_bar->addStretch();

    table_turmas_ = new QTableWidget(widget);
    table_turmas_->setColumnCount(6);
    table_turmas_->setHorizontalHeaderLabels({"ID", "Nome", "Turno", "Jornada", "Intervalos Obrigatórios (2)", "Sala Teórica"});
    table_turmas_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_turmas_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_turmas_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(table_turmas_);

    search_bar_turmas_ = new TableSearchSortBar(widget);
    search_bar_turmas_->setupWithTable(table_turmas_, "turma", "turmas", 1 /* Nome */);
    btn_bar->addWidget(search_bar_turmas_);
    layout->insertLayout(0, btn_bar);

    connect(btn_add, &QPushButton::clicked, this, &MainWindow::onAddTurma);
    connect(btn_edit, &QPushButton::clicked, this, &MainWindow::onEditTurma);
    connect(btn_rem, &QPushButton::clicked, this, &MainWindow::onRemoveTurma);
    connect(table_turmas_, &QTableWidget::cellDoubleClicked, this, &MainWindow::onEditTurma);

    return widget;
}

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
    btn_bar->addWidget(btn_add);
    btn_bar->addWidget(btn_edit);
    btn_bar->addWidget(btn_rem);
    btn_bar->addStretch();

    table_salas_ = new QTableWidget(widget);
    table_salas_->setColumnCount(5);
    table_salas_->setHorizontalHeaderLabels({"ID", "Nome", "Tipo", "Capacidade", "Recursos"});
    table_salas_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_salas_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_salas_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(table_salas_);

    search_bar_salas_ = new TableSearchSortBar(widget);
    search_bar_salas_->setupWithTable(table_salas_, "sala", "salas", 1 /* Nome */);
    btn_bar->addWidget(search_bar_salas_);
    layout->insertLayout(0, btn_bar);

    connect(btn_add, &QPushButton::clicked, this, &MainWindow::onAddSala);
    connect(btn_edit, &QPushButton::clicked, this, &MainWindow::onEditSala);
    connect(btn_rem, &QPushButton::clicked, this, &MainWindow::onRemoveSala);
    connect(table_salas_, &QTableWidget::cellDoubleClicked, this, &MainWindow::onEditSala);

    return widget;
}

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
    btn_bar->addWidget(btn_add);
    btn_bar->addWidget(btn_edit);
    btn_bar->addWidget(btn_rem);
    btn_bar->addStretch();

    table_disciplinas_ = new QTableWidget(widget);
    table_disciplinas_->setColumnCount(7);
    table_disciplinas_->setHorizontalHeaderLabels({"ID", "Nome", "Tipo", "Duração", "Aulas/Sem", "Professor", "Turma"});
    table_disciplinas_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_disciplinas_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_disciplinas_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(table_disciplinas_);

    search_bar_disciplinas_ = new TableSearchSortBar(widget);
    search_bar_disciplinas_->setupWithTable(table_disciplinas_, "disciplina", "disciplinas", 1 /* Nome */);
    btn_bar->addWidget(search_bar_disciplinas_);
    layout->insertLayout(0, btn_bar);

    connect(btn_add, &QPushButton::clicked, this, &MainWindow::onAddDisciplina);
    connect(btn_edit, &QPushButton::clicked, this, &MainWindow::onEditDisciplina);
    connect(btn_rem, &QPushButton::clicked, this, &MainWindow::onRemoveDisciplina);
    connect(table_disciplinas_, &QTableWidget::cellDoubleClicked, this, &MainWindow::onEditDisciplina);

    return widget;
}

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

void MainWindow::onLoadDefaultExample() {
    data_ = model::InstitutionData::criarExemploPadrao();
    refreshAllTables();
    config_widget_->loadConfig(data_.config());
    timetable_view_->refreshView();
    updateKpiBanner();
    txt_logs_->appendPlainText("[EXEMPLO] Cenário padrão carregado: 6 Professores, 6 Salas/Labs, 3 Turmas, 19 Disciplinas mistas (45 e 50 min).");
    statusBar()->showMessage("Cenário de exemplo carregado. Clique em '⚡ OTIMIZAR GRADE' para executar.", 5000);
}

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

    data_.gerarOcorrenciasAulas(true); // Preserva aulas fixadas

    std::string algo_name = model::solverAlgorithmDisplayName(current_cfg.algoritmo);
    txt_logs_->appendPlainText("--------------------------------------------------");
    txt_logs_->appendPlainText("[OTIMIZAÇÃO] Algoritmo selecionado: " + QString::fromStdString(algo_name));
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

void MainWindow::refreshAllTables() {
    refreshProfessoresTable();
    refreshTurmasTable();
    refreshSalasTable();
    refreshDisciplinasTable();
}

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

void MainWindow::refreshTurmasTable() {
    table_turmas_->setSortingEnabled(false);
    table_turmas_->setRowCount(0);
    for (const auto& t : data_.turmas()) {
        int r = table_turmas_->rowCount();
        table_turmas_->insertRow(r);

        table_turmas_->setItem(r, 0, new SmartTableWidgetItem(QString::fromStdString(t.id())));
        table_turmas_->setItem(r, 1, new SmartTableWidgetItem(QString::fromStdString(t.nome())));
        table_turmas_->setItem(r, 2, new SmartTableWidgetItem(QString::fromStdString(model::shiftToString(t.turno()))));

        const auto& jp = t.jornadaPadrao();
        table_turmas_->setItem(r, 3, new SmartTableWidgetItem(QString::fromStdString(jp.jornada_total.toString())));

        QStringList int_strs;
        if (jp.int1_ativo) {
            int_strs << QString("Int1: %1").arg(QString::fromStdString(jp.intervalo_1.toString()));
        }
        if (jp.int2_ativo) {
            int_strs << QString("Int2: %1").arg(QString::fromStdString(jp.intervalo_2.toString()));
        }
        if (jp.int3_ativo) {
            int_strs << QString("Int3: %1").arg(QString::fromStdString(jp.intervalo_3.toString()));
        }
        QString intervals = int_strs.isEmpty() ? "Nenhum" : int_strs.join(" | ");
        table_turmas_->setItem(r, 4, new SmartTableWidgetItem(intervals));

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

void MainWindow::refreshDisciplinasTable() {
    table_disciplinas_->setSortingEnabled(false);
    table_disciplinas_->setRowCount(0);
    for (const auto& d : data_.disciplinas()) {
        int r = table_disciplinas_->rowCount();
        table_disciplinas_->insertRow(r);

        table_disciplinas_->setItem(r, 0, new SmartTableWidgetItem(QString::fromStdString(d.id())));
        table_disciplinas_->setItem(r, 1, new SmartTableWidgetItem(QString::fromStdString(d.nome())));
        table_disciplinas_->setItem(r, 2, new SmartTableWidgetItem(QString::fromStdString(d.tipo())));
        table_disciplinas_->setItem(r, 3, new SmartTableWidgetItem(QString::number(d.duracaoMinutos()) + " min"));
        table_disciplinas_->setItem(r, 4, new SmartTableWidgetItem(QString::number(d.aulasSemanais())));

        QString profs;
        for (size_t i = 0; i < d.professores().size(); ++i) {
            if (i > 0) profs += ", ";
            const auto* p = data_.findProfessor(d.professores()[i]);
            profs += QString::fromStdString(p ? p->nome() : d.professores()[i]);
        }
        table_disciplinas_->setItem(r, 5, new SmartTableWidgetItem(profs));

        QString turmas;
        for (size_t i = 0; i < d.turmas().size(); ++i) {
            if (i > 0) turmas += ", ";
            const auto* t = data_.findTurma(d.turmas()[i]);
            turmas += QString::fromStdString(t ? t->nome() : d.turmas()[i]);
        }
        table_disciplinas_->setItem(r, 6, new SmartTableWidgetItem(turmas));
    }
    table_disciplinas_->setSortingEnabled(true);
    if (search_bar_disciplinas_) {
        search_bar_disciplinas_->reapplyFilterAndSort();
    }
}

void MainWindow::onAddProfessor() {
    ProfessorEditorDialog dlg(this, nullptr);
    if (dlg.exec() == QDialog::Accepted) {
        data_.addProfessor(dlg.getProfessor());
        refreshProfessoresTable();
        timetable_view_->refreshView();
    }
}

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

void MainWindow::onRemoveProfessor() {
    int r = table_professores_->currentRow();
    if (r < 0 || !table_professores_->item(r, 0)) return;
    std::string id = table_professores_->item(r, 0)->text().toStdString();
    if (QMessageBox::question(this, "Remover Professor", "Deseja remover o professor " + QString::fromStdString(id) + "?",
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        data_.removeProfessor(id);
        refreshProfessoresTable();
        timetable_view_->refreshView();
    }
}

void MainWindow::onAddTurma() {
    TurmaEditorDialog dlg(data_.salas(), this, nullptr);
    if (dlg.exec() == QDialog::Accepted) {
        data_.addTurma(dlg.getTurma());
        refreshTurmasTable();
        timetable_view_->refreshView();
    }
}

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

void MainWindow::onRemoveTurma() {
    int r = table_turmas_->currentRow();
    if (r < 0 || !table_turmas_->item(r, 0)) return;
    std::string id = table_turmas_->item(r, 0)->text().toStdString();
    if (QMessageBox::question(this, "Remover Turma", "Deseja remover a turma " + QString::fromStdString(id) + "?",
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        data_.removeTurma(id);
        refreshTurmasTable();
        timetable_view_->refreshView();
    }
}

void MainWindow::onAddSala() {
    RoomEditorDialog dlg(this, nullptr);
    if (dlg.exec() == QDialog::Accepted) {
        data_.addSala(dlg.getRoom());
        refreshSalasTable();
        timetable_view_->refreshView();
    }
}

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

void MainWindow::onRemoveSala() {
    int r = table_salas_->currentRow();
    if (r < 0 || !table_salas_->item(r, 0)) return;
    std::string id = table_salas_->item(r, 0)->text().toStdString();
    if (QMessageBox::question(this, "Remover Sala", "Deseja remover a sala " + QString::fromStdString(id) + "?",
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        data_.removeSala(id);
        refreshSalasTable();
        timetable_view_->refreshView();
    }
}

void MainWindow::onAddDisciplina() {
    LessonEditorDialog dlg(data_.professores(), data_.turmas(), data_.salas(), this, nullptr);
    if (dlg.exec() == QDialog::Accepted) {
        data_.addDisciplina(dlg.getDisciplina());
        data_.gerarOcorrenciasAulas(true);
        refreshDisciplinasTable();
    }
}

void MainWindow::onEditDisciplina() {
    int r = table_disciplinas_->currentRow();
    if (r < 0 || !table_disciplinas_->item(r, 0)) return;
    std::string id = table_disciplinas_->item(r, 0)->text().toStdString();
    auto* d = data_.findDisciplina(id);
    if (d) {
        LessonEditorDialog dlg(data_.professores(), data_.turmas(), data_.salas(), this, d);
        if (dlg.exec() == QDialog::Accepted) {
            data_.addDisciplina(dlg.getDisciplina());
            data_.gerarOcorrenciasAulas(true);
            refreshDisciplinasTable();
        }
    }
}

void MainWindow::onRemoveDisciplina() {
    int r = table_disciplinas_->currentRow();
    if (r < 0 || !table_disciplinas_->item(r, 0)) return;
    std::string id = table_disciplinas_->item(r, 0)->text().toStdString();
    if (QMessageBox::question(this, "Remover Disciplina", "Deseja remover a disciplina " + QString::fromStdString(id) + "?",
                              QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        data_.removeDisciplina(id);
        data_.gerarOcorrenciasAulas(true);
        refreshDisciplinasTable();
    }
}

} // namespace scheduler::ui
