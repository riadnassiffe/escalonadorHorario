#include "ui/ProfessorEditorDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QHeaderView>
#include <QTimeEdit>
#include <QComboBox>
#include <QMessageBox>
#include <QScrollArea>
#include <QGridLayout>
#include <QTabWidget>

namespace scheduler::ui {

ProfessorEditorDialog::ProfessorEditorDialog(QWidget* parent, const model::Professor* prof)
    : QDialog(parent) {
    setupUi();
    if (prof) {
        loadProfessor(*prof);
        setWindowTitle("Editar Professor - " + QString::fromStdString(prof->nome()));
    } else {
        setWindowTitle("Novo Professor");
        onSetDefaultMorning();
    }
}

void ProfessorEditorDialog::setupUi() {
    setMinimumWidth(750);
    setMinimumHeight(580);
    resize(780, 630);

    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(12, 12, 12, 12);
    root_layout->setSpacing(10);

    // 1. Dados Básicos de Identificação
    auto* form_card = new QWidget(this);
    auto* form_layout = new QGridLayout(form_card);
    form_layout->setContentsMargins(0, 0, 0, 4);
    form_layout->setHorizontalSpacing(14);
    form_layout->setVerticalSpacing(8);

    txt_id_ = new QLineEdit(form_card);
    txt_id_->setPlaceholderText("Ex: P01");
    txt_id_->setMinimumWidth(110);
    txt_id_->setMaximumWidth(150);

    txt_nome_ = new QLineEdit(form_card);
    txt_nome_->setPlaceholderText("Ex: Prof. Carlos Silva (Matemática)");

    spin_carga_max_ = new QSpinBox(form_card);
    spin_carga_max_->setRange(0, 1440);
    spin_carga_max_->setSingleStep(30);
    spin_carga_max_->setSuffix(" minutos (0 = sem limite)");

    auto* lbl_id = new QLabel("Código (ID):", form_card);
    lbl_id->setStyleSheet("font-weight: 600;");
    auto* lbl_carga = new QLabel("Carga Máx. Diária:", form_card);
    lbl_carga->setStyleSheet("font-weight: 600;");
    auto* lbl_nome = new QLabel("Nome Completo:", form_card);
    lbl_nome->setStyleSheet("font-weight: 600;");

    form_layout->addWidget(lbl_id, 0, 0);
    form_layout->addWidget(txt_id_, 0, 1);
    form_layout->addWidget(lbl_carga, 0, 2);
    form_layout->addWidget(spin_carga_max_, 0, 3);

    form_layout->addWidget(lbl_nome, 1, 0);
    form_layout->addWidget(txt_nome_, 1, 1, 1, 3);

    root_layout->addWidget(form_card);

    // 2. Abas Principais
    auto* main_tabs = new QTabWidget(this);

    // =========================================================================
    // ABA 1: DISPONIBILIDADE DE HORÁRIOS (DIAS DA SEMANA E HORAS INÍCIO/FIM)
    // =========================================================================
    auto* tab_disp = new QWidget(main_tabs);
    auto* layout_tab_disp = new QVBoxLayout(tab_disp);
    layout_tab_disp->setContentsMargins(10, 12, 10, 10);
    layout_tab_disp->setSpacing(10);

    auto* lbl_disp_info = new QLabel(
        "Configure os horários em que o professor está disponível para ministrar aulas em cada dia da semana.<br>"
        "Você pode usar os botões rápidos para preencher toda a semana ou ajustar os dias e horários diretamente na tabela.", tab_disp);
    lbl_disp_info->setStyleSheet("color: #94a3b8; font-size: 11px;");
    lbl_disp_info->setWordWrap(true);
    layout_tab_disp->addWidget(lbl_disp_info);

    // Barra de Presets Rápidos
    auto* bar_presets = new QHBoxLayout();
    bar_presets->setSpacing(6);
    auto* lbl_pre_tit = new QLabel("⚡ Preencher:", tab_disp);
    lbl_pre_tit->setStyleSheet("font-weight: bold; color: #38bdf8;");
    bar_presets->addWidget(lbl_pre_tit);

    auto* btn_def_m = new QPushButton("🌅 Manhã", tab_disp);
    btn_def_m->setObjectName("btnSecondary");
    btn_def_m->setToolTip("Preencher toda a semana com horário matutino (07:30 às 12:00)");

    auto* btn_def_t = new QPushButton("☀️ Tarde", tab_disp);
    btn_def_t->setObjectName("btnSecondary");
    btn_def_t->setToolTip("Preencher toda a semana com horário vespertino (13:15 às 18:00)");

    auto* btn_def_int = new QPushButton("🌅+☀️ Integral", tab_disp);
    btn_def_int->setObjectName("btnSecondary");
    btn_def_int->setToolTip("Preencher toda a semana com horário integral (07:30 às 18:00)");

    auto* btn_def_n = new QPushButton("🌙 Noite", tab_disp);
    btn_def_n->setObjectName("btnSecondary");
    btn_def_n->setToolTip("Preencher toda a semana com horário noturno (18:30 às 22:30)");

    auto* btn_def_all = new QPushButton("🌟 Dia Todo", tab_disp);
    btn_def_all->setObjectName("btnSecondary");
    btn_def_all->setToolTip("Preencher toda a semana com dia completo (07:00 às 22:30)");

    bar_presets->addWidget(btn_def_m);
    bar_presets->addWidget(btn_def_t);
    bar_presets->addWidget(btn_def_int);
    bar_presets->addWidget(btn_def_n);
    bar_presets->addWidget(btn_def_all);
    bar_presets->addStretch();
    layout_tab_disp->addLayout(bar_presets);

    connect(btn_def_m, &QPushButton::clicked, this, &ProfessorEditorDialog::onSetDefaultMorning);
    connect(btn_def_t, &QPushButton::clicked, this, &ProfessorEditorDialog::onSetDefaultAfternoon);
    connect(btn_def_int, &QPushButton::clicked, this, &ProfessorEditorDialog::onSetDefaultAllDay);
    connect(btn_def_n, &QPushButton::clicked, this, &ProfessorEditorDialog::onSetDefaultNight);
    connect(btn_def_all, &QPushButton::clicked, this, &ProfessorEditorDialog::onSetDefaultWholeDay);

    // Barra de Ações para Intervalos
    auto* bar_actions = new QHBoxLayout();
    auto* btn_add = new QPushButton("➕ Adicionar Intervalo", tab_disp);
    auto* btn_rem = new QPushButton("➖ Remover Selecionado", tab_disp);
    btn_rem->setObjectName("btnSecondary");
    auto* btn_clear = new QPushButton("🗑️ Limpar Todos", tab_disp);
    btn_clear->setObjectName("btnSecondary");

    bar_actions->addWidget(btn_add);
    bar_actions->addWidget(btn_rem);
    bar_actions->addWidget(btn_clear);
    bar_actions->addStretch();
    layout_tab_disp->addLayout(bar_actions);

    connect(btn_add, &QPushButton::clicked, this, &ProfessorEditorDialog::onAddInterval);
    connect(btn_rem, &QPushButton::clicked, this, &ProfessorEditorDialog::onRemoveInterval);
    connect(btn_clear, &QPushButton::clicked, this, &ProfessorEditorDialog::onClearAllIntervals);

    // Tabela de Disponibilidade (Dias da Semana, Hora Início, Hora Término)
    table_disp_ = new QTableWidget(tab_disp);
    table_disp_->setColumnCount(3);
    table_disp_->setHorizontalHeaderLabels({"Dia da Semana", "Hora Início", "Hora Término"});
    table_disp_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    table_disp_->setColumnWidth(0, 220);
    table_disp_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table_disp_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    table_disp_->verticalHeader()->setDefaultSectionSize(40);
    table_disp_->verticalHeader()->setVisible(false);
    table_disp_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_disp_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_disp_->setAlternatingRowColors(true);
    layout_tab_disp->addWidget(table_disp_);

    main_tabs->addTab(tab_disp, "⏰ Horários de Disponibilidade (Início / Fim por Dia)");

    // =========================================================================
    // ABA 2: TURNOS PRETENDIDOS & PREFERÊNCIAS
    // =========================================================================
    auto* tab_prefs = new QWidget(main_tabs);
    auto* layout_tab_prefs = new QVBoxLayout(tab_prefs);
    layout_tab_prefs->setContentsMargins(0, 0, 0, 0);

    auto* scroll_prefs = new QScrollArea(tab_prefs);
    scroll_prefs->setWidgetResizable(true);
    scroll_prefs->setFrameShape(QFrame::NoFrame);

    auto* container_prefs = new QWidget(scroll_prefs);
    auto* layout_container_prefs = new QVBoxLayout(container_prefs);
    layout_container_prefs->setSpacing(14);
    layout_container_prefs->setContentsMargins(10, 10, 10, 10);

    // 1. Grupo: Aversão a Janelas (Vontade do Professor)
    auto* grp_janela = new QGroupBox("Aversão a Janelas entre Aulas (Vontade do Professor)", container_prefs);
    auto* layout_janela = new QVBoxLayout(grp_janela);
    layout_janela->setSpacing(8);

    auto* lbl_janela_info = new QLabel(
        "Define a tolerância individual do professor a horários ociosos (janelas) entre aulas no mesmo turno.<br>"
        "0 = Indiferente a janelas (sem custo); 10 = Não tolera janelas (máxima penalidade no escalonador).", grp_janela);
    lbl_janela_info->setStyleSheet("color: #94a3b8; font-size: 11px;");
    lbl_janela_info->setWordWrap(true);
    layout_janela->addWidget(lbl_janela_info);

    auto* row_janela = new QHBoxLayout();
    auto* lbl_janela_tit = new QLabel("Aversão a Janelas (0 a 10):", grp_janela);
    lbl_janela_tit->setFixedWidth(190);
    lbl_janela_tit->setStyleSheet("font-weight: 600;");
    row_janela->addWidget(lbl_janela_tit);

    slider_peso_janela_ = new QSlider(Qt::Horizontal, grp_janela);
    slider_peso_janela_->setRange(0, 10);
    slider_peso_janela_->setValue(8);
    slider_peso_janela_->setTickInterval(1);
    slider_peso_janela_->setTickPosition(QSlider::TicksBelow);
    row_janela->addWidget(slider_peso_janela_);

    lbl_val_peso_janela_ = new QLabel("8", grp_janela);
    lbl_val_peso_janela_->setFixedWidth(30);
    lbl_val_peso_janela_->setStyleSheet("font-weight: bold; color: #38bdf8; font-size: 14px;");
    row_janela->addWidget(lbl_val_peso_janela_);

    lbl_desc_peso_janela_ = new QLabel("Baixa tolerância a janelas", grp_janela);
    lbl_desc_peso_janela_->setFixedWidth(230);
    lbl_desc_peso_janela_->setStyleSheet("color: #cbd5e1; font-size: 12px;");
    row_janela->addWidget(lbl_desc_peso_janela_);

    connect(slider_peso_janela_, &QSlider::valueChanged, this, &ProfessorEditorDialog::onSliderJanelaChanged);
    layout_janela->addLayout(row_janela);
    layout_container_prefs->addWidget(grp_janela);

    // 2. Grupo: Turnos Pretendidos por Dia da Semana
    auto* grp_turnos = new QGroupBox("Turnos Pretendidos de Trabalho por Dia da Semana", container_prefs);
    auto* layout_turnos = new QVBoxLayout(grp_turnos);
    layout_turnos->setSpacing(8);

    auto* lbl_turnos_info = new QLabel(
        "Marque os turnos em que o professor pretende trabalhar em cada dia. "
        "O cálculo de janelas é feito separadamente por turno (sem penalizar pausas entre turnos diferentes). "
        "Aulas alocadas em turnos desmarcados recebem penalidade na função objetivo.", grp_turnos);
    lbl_turnos_info->setStyleSheet("color: #94a3b8; font-size: 11px;");
    lbl_turnos_info->setWordWrap(true);
    layout_turnos->addWidget(lbl_turnos_info);

    auto* tool_turnos = new QHBoxLayout();
    auto* btn_all_shifts = new QPushButton("Todos os Turnos", grp_turnos);
    btn_all_shifts->setObjectName("btnSecondary");
    auto* btn_morning_only = new QPushButton("Apenas Manhã", grp_turnos);
    btn_morning_only->setObjectName("btnSecondary");
    auto* btn_morn_aft = new QPushButton("Manhã e Tarde", grp_turnos);
    btn_morn_aft->setObjectName("btnSecondary");

    tool_turnos->addWidget(btn_all_shifts);
    tool_turnos->addWidget(btn_morning_only);
    tool_turnos->addWidget(btn_morn_aft);
    tool_turnos->addStretch();
    layout_turnos->addLayout(tool_turnos);

    connect(btn_all_shifts, &QPushButton::clicked, this, &ProfessorEditorDialog::onSelectAllShifts);
    connect(btn_morning_only, &QPushButton::clicked, this, &ProfessorEditorDialog::onSelectMorningOnly);
    connect(btn_morn_aft, &QPushButton::clicked, this, &ProfessorEditorDialog::onSelectMorningAfternoon);

    auto* grid_turnos = new QGridLayout();
    grid_turnos->setHorizontalSpacing(24);
    grid_turnos->setVerticalSpacing(8);

    grid_turnos->addWidget(new QLabel("<b>Dia da Semana</b>", grp_turnos), 0, 0);
    grid_turnos->addWidget(new QLabel("<b>☀️ Manhã</b>", grp_turnos), 0, 1);
    grid_turnos->addWidget(new QLabel("<b>🌤️ Tarde</b>", grp_turnos), 0, 2);
    grid_turnos->addWidget(new QLabel("<b>🌙 Noite</b>", grp_turnos), 0, 3);

    for (size_t i = 0; i < 5; ++i) {
        auto d = static_cast<model::DayOfWeek>(i);
        auto* lbl_d = new QLabel(QString::fromStdString(model::dayToLongString(d)), grp_turnos);
        lbl_d->setStyleSheet("font-weight: 600; color: #f1f5f9;");
        grid_turnos->addWidget(lbl_d, static_cast<int>(i) + 1, 0);

        for (size_t s = 0; s < 3; ++s) {
            auto* chk = new QCheckBox(grp_turnos);
            chk->setChecked(true);
            check_turnos_dias_[i][s] = chk;
            grid_turnos->addWidget(chk, static_cast<int>(i) + 1, static_cast<int>(s) + 1);
        }
    }
    layout_turnos->addLayout(grid_turnos);
    layout_container_prefs->addWidget(grp_turnos);

    // 3. Grupo: Pesos de Preferência por Dia da Semana (0 a 10)
    auto* grp_pesos = new QGroupBox("Pesos de Preferência por Dia da Semana (0 a 10) - Seção 2.3", container_prefs);
    auto* layout_pesos = new QVBoxLayout(grp_pesos);
    layout_pesos->setSpacing(8);

    auto* lbl_aviso = new QLabel(
        "Convenção: 10 = maior preferência, 0 = dia menos desejado (não indisponível). Padrão neutro: 5.", grp_pesos);
    lbl_aviso->setStyleSheet("color: #94a3b8; font-size: 11px;");
    lbl_aviso->setWordWrap(true);
    layout_pesos->addWidget(lbl_aviso);

    for (size_t i = 0; i < 5; ++i) {
        auto d = static_cast<model::DayOfWeek>(i);
        auto* row_layout = new QHBoxLayout();

        auto* lbl_dia = new QLabel(QString::fromStdString(model::dayToLongString(d)) + ":", grp_pesos);
        lbl_dia->setFixedWidth(110);
        lbl_dia->setStyleSheet("font-weight: 600;");
        row_layout->addWidget(lbl_dia);

        auto* slider = new QSlider(Qt::Horizontal, grp_pesos);
        slider->setRange(0, 10);
        slider->setValue(5);
        slider->setTickInterval(1);
        slider->setTickPosition(QSlider::TicksBelow);
        sliders_dias_[i] = slider;
        row_layout->addWidget(slider);

        auto* lbl_val = new QLabel("5", grp_pesos);
        lbl_val->setFixedWidth(30);
        lbl_val->setStyleSheet("font-weight: bold; color: #38bdf8; font-size: 14px;");
        lbls_valores_dias_[i] = lbl_val;
        row_layout->addWidget(lbl_val);

        auto* lbl_desc = new QLabel("Neutra", grp_pesos);
        lbl_desc->setFixedWidth(160);
        lbl_desc->setStyleSheet("color: #cbd5e1; font-size: 12px;");
        lbls_desc_dias_[i] = lbl_desc;
        row_layout->addWidget(lbl_desc);

        connect(slider, &QSlider::valueChanged, this, &ProfessorEditorDialog::onSliderValueChanged);
        layout_pesos->addLayout(row_layout);
    }
    layout_container_prefs->addWidget(grp_pesos);

    scroll_prefs->setWidget(container_prefs);
    layout_tab_prefs->addWidget(scroll_prefs);

    main_tabs->addTab(tab_prefs, "⭐ Turnos Pretendidos && Preferências");

    root_layout->addWidget(main_tabs);

    // 3. Botões de Ação
    auto* btn_box = new QHBoxLayout();
    btn_box->addStretch();
    auto* btn_cancel = new QPushButton("Cancelar", this);
    btn_cancel->setObjectName("btnSecondary");
    auto* btn_save = new QPushButton("Salvar Professor", this);
    btn_box->addWidget(btn_cancel);
    btn_box->addWidget(btn_save);
    root_layout->addLayout(btn_box);

    connect(btn_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btn_save, &QPushButton::clicked, [this]() {
        if (txt_id_->text().trimmed().isEmpty() || txt_nome_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Atenção", "Preencha o Código e o Nome do Professor.");
            return;
        }
        accept();
    });

    onSliderValueChanged();
    onSliderJanelaChanged();
}

void ProfessorEditorDialog::onSliderValueChanged() {
    for (size_t i = 0; i < 5; ++i) {
        int val = sliders_dias_[i]->value();
        lbls_valores_dias_[i]->setText(QString::number(val));
        if (val == 10) {
            lbls_desc_dias_[i]->setText("⭐ Maior preferência");
            lbls_desc_dias_[i]->setStyleSheet("color: #10b981; font-weight: bold;");
        } else if (val >= 7) {
            lbls_desc_dias_[i]->setText("Alta preferência");
            lbls_desc_dias_[i]->setStyleSheet("color: #38bdf8; font-weight: bold;");
        } else if (val >= 4) {
            lbls_desc_dias_[i]->setText("Preferência neutra");
            lbls_desc_dias_[i]->setStyleSheet("color: #94a3b8;");
        } else if (val >= 1) {
            lbls_desc_dias_[i]->setText("Baixa preferência");
            lbls_desc_dias_[i]->setStyleSheet("color: #f59e0b;");
        } else {
            lbls_desc_dias_[i]->setText("⚠️ Menos desejado (peso 0)");
            lbls_desc_dias_[i]->setStyleSheet("color: #ef4444; font-weight: bold;");
        }
    }
}

void ProfessorEditorDialog::onSliderJanelaChanged() {
    int val = slider_peso_janela_->value();
    lbl_val_peso_janela_->setText(QString::number(val));
    if (val == 10) {
        lbl_desc_peso_janela_->setText("⭐ Máxima aversão (não tolera janelas)");
        lbl_desc_peso_janela_->setStyleSheet("color: #ef4444; font-weight: bold;");
    } else if (val >= 7) {
        lbl_desc_peso_janela_->setText("Baixa tolerância a janelas");
        lbl_desc_peso_janela_->setStyleSheet("color: #f59e0b; font-weight: bold;");
    } else if (val >= 4) {
        lbl_desc_peso_janela_->setText("Tolerância neutra / média");
        lbl_desc_peso_janela_->setStyleSheet("color: #94a3b8;");
    } else if (val >= 1) {
        lbl_desc_peso_janela_->setText("Alta tolerância a janelas");
        lbl_desc_peso_janela_->setStyleSheet("color: #38bdf8;");
    } else {
        lbl_desc_peso_janela_->setText("Indiferente (sem penalidade)");
        lbl_desc_peso_janela_->setStyleSheet("color: #10b981; font-weight: bold;");
    }
}

void ProfessorEditorDialog::onSelectAllShifts() {
    for (size_t i = 0; i < 5; ++i) {
        for (size_t s = 0; s < 3; ++s) {
            check_turnos_dias_[i][s]->setChecked(true);
        }
    }
}

void ProfessorEditorDialog::onSelectMorningOnly() {
    for (size_t i = 0; i < 5; ++i) {
        check_turnos_dias_[i][0]->setChecked(true);
        check_turnos_dias_[i][1]->setChecked(false);
        check_turnos_dias_[i][2]->setChecked(false);
    }
}

void ProfessorEditorDialog::onSelectMorningAfternoon() {
    for (size_t i = 0; i < 5; ++i) {
        check_turnos_dias_[i][0]->setChecked(true);
        check_turnos_dias_[i][1]->setChecked(true);
        check_turnos_dias_[i][2]->setChecked(false);
    }
}

void ProfessorEditorDialog::onAddInterval() {
    int row = table_disp_->rowCount();
    table_disp_->insertRow(row);

    auto* combo_dia = new QComboBox(table_disp_);
    for (auto d : model::allDays()) {
        combo_dia->addItem(QString::fromStdString(model::dayToLongString(d)), static_cast<int>(d));
    }
    table_disp_->setCellWidget(row, 0, combo_dia);

    auto* time_ini = new QTimeEdit(QTime(7, 30), table_disp_);
    time_ini->setDisplayFormat("HH:mm");
    time_ini->setMinimumHeight(28);
    table_disp_->setCellWidget(row, 1, time_ini);

    auto* time_fim = new QTimeEdit(QTime(12, 0), table_disp_);
    time_fim->setDisplayFormat("HH:mm");
    time_fim->setMinimumHeight(28);
    table_disp_->setCellWidget(row, 2, time_fim);
}

void ProfessorEditorDialog::onRemoveInterval() {
    int row = table_disp_->currentRow();
    if (row >= 0) {
        table_disp_->removeRow(row);
    } else if (table_disp_->rowCount() > 0) {
        table_disp_->removeRow(table_disp_->rowCount() - 1);
    }
}

void ProfessorEditorDialog::onClearAllIntervals() {
    table_disp_->setRowCount(0);
}

void ProfessorEditorDialog::onSetDefaultMorning() {
    table_disp_->setRowCount(0);
    for (auto d : model::allDays()) {
        int r = table_disp_->rowCount();
        table_disp_->insertRow(r);

        auto* combo = new QComboBox(table_disp_);
        for (auto day : model::allDays()) {
            combo->addItem(QString::fromStdString(model::dayToLongString(day)), static_cast<int>(day));
        }
        combo->setCurrentIndex(combo->findData(static_cast<int>(d)));
        table_disp_->setCellWidget(r, 0, combo);

        auto* t_ini = new QTimeEdit(QTime(7, 30), table_disp_);
        t_ini->setDisplayFormat("HH:mm");
        t_ini->setMinimumHeight(28);
        table_disp_->setCellWidget(r, 1, t_ini);

        auto* t_fim = new QTimeEdit(QTime(12, 0), table_disp_);
        t_fim->setDisplayFormat("HH:mm");
        t_fim->setMinimumHeight(28);
        table_disp_->setCellWidget(r, 2, t_fim);
    }
}

void ProfessorEditorDialog::onSetDefaultAfternoon() {
    table_disp_->setRowCount(0);
    for (auto d : model::allDays()) {
        int r = table_disp_->rowCount();
        table_disp_->insertRow(r);

        auto* combo = new QComboBox(table_disp_);
        for (auto day : model::allDays()) {
            combo->addItem(QString::fromStdString(model::dayToLongString(day)), static_cast<int>(day));
        }
        combo->setCurrentIndex(combo->findData(static_cast<int>(d)));
        table_disp_->setCellWidget(r, 0, combo);

        auto* t_ini = new QTimeEdit(QTime(13, 15), table_disp_);
        t_ini->setDisplayFormat("HH:mm");
        t_ini->setMinimumHeight(28);
        table_disp_->setCellWidget(r, 1, t_ini);

        auto* t_fim = new QTimeEdit(QTime(17, 30), table_disp_);
        t_fim->setDisplayFormat("HH:mm");
        t_fim->setMinimumHeight(28);
        table_disp_->setCellWidget(r, 2, t_fim);
    }
}

void ProfessorEditorDialog::onSetDefaultNight() {
    table_disp_->setRowCount(0);
    for (auto d : model::allDays()) {
        int r = table_disp_->rowCount();
        table_disp_->insertRow(r);

        auto* combo = new QComboBox(table_disp_);
        for (auto day : model::allDays()) {
            combo->addItem(QString::fromStdString(model::dayToLongString(day)), static_cast<int>(day));
        }
        combo->setCurrentIndex(combo->findData(static_cast<int>(d)));
        table_disp_->setCellWidget(r, 0, combo);

        auto* t_ini = new QTimeEdit(QTime(18, 30), table_disp_);
        t_ini->setDisplayFormat("HH:mm");
        t_ini->setMinimumHeight(28);
        table_disp_->setCellWidget(r, 1, t_ini);

        auto* t_fim = new QTimeEdit(QTime(22, 30), table_disp_);
        t_fim->setDisplayFormat("HH:mm");
        t_fim->setMinimumHeight(28);
        table_disp_->setCellWidget(r, 2, t_fim);
    }
}

void ProfessorEditorDialog::onSetDefaultAllDay() {
    table_disp_->setRowCount(0);
    for (auto d : model::allDays()) {
        int r = table_disp_->rowCount();
        table_disp_->insertRow(r);

        auto* combo = new QComboBox(table_disp_);
        for (auto day : model::allDays()) {
            combo->addItem(QString::fromStdString(model::dayToLongString(day)), static_cast<int>(day));
        }
        combo->setCurrentIndex(combo->findData(static_cast<int>(d)));
        table_disp_->setCellWidget(r, 0, combo);

        auto* t_ini = new QTimeEdit(QTime(7, 30), table_disp_);
        t_ini->setDisplayFormat("HH:mm");
        t_ini->setMinimumHeight(28);
        table_disp_->setCellWidget(r, 1, t_ini);

        auto* t_fim = new QTimeEdit(QTime(18, 0), table_disp_);
        t_fim->setDisplayFormat("HH:mm");
        t_fim->setMinimumHeight(28);
        table_disp_->setCellWidget(r, 2, t_fim);
    }
}

void ProfessorEditorDialog::onSetDefaultWholeDay() {
    table_disp_->setRowCount(0);
    for (auto d : model::allDays()) {
        int r = table_disp_->rowCount();
        table_disp_->insertRow(r);

        auto* combo = new QComboBox(table_disp_);
        for (auto day : model::allDays()) {
            combo->addItem(QString::fromStdString(model::dayToLongString(day)), static_cast<int>(day));
        }
        combo->setCurrentIndex(combo->findData(static_cast<int>(d)));
        table_disp_->setCellWidget(r, 0, combo);

        auto* t_ini = new QTimeEdit(QTime(7, 0), table_disp_);
        t_ini->setDisplayFormat("HH:mm");
        t_ini->setMinimumHeight(28);
        table_disp_->setCellWidget(r, 1, t_ini);

        auto* t_fim = new QTimeEdit(QTime(22, 30), table_disp_);
        t_fim->setDisplayFormat("HH:mm");
        t_fim->setMinimumHeight(28);
        table_disp_->setCellWidget(r, 2, t_fim);
    }
}

void ProfessorEditorDialog::loadProfessor(const model::Professor& prof) {
    txt_id_->setText(QString::fromStdString(prof.id()));
    txt_id_->setReadOnly(true); // Manter ID estável na edição
    txt_nome_->setText(QString::fromStdString(prof.nome()));
    spin_carga_max_->setValue(prof.cargaMaximaDiariaMinutos());

    slider_peso_janela_->setValue(prof.pesoJanela());
    onSliderJanelaChanged();

    for (size_t i = 0; i < 5; ++i) {
        auto d = static_cast<model::DayOfWeek>(i);
        sliders_dias_[i]->setValue(prof.pesoDia(d));
        check_turnos_dias_[i][0]->setChecked(prof.pretendeTurno(d, model::ShiftType::MANHA));
        check_turnos_dias_[i][1]->setChecked(prof.pretendeTurno(d, model::ShiftType::TARDE));
        check_turnos_dias_[i][2]->setChecked(prof.pretendeTurno(d, model::ShiftType::NOITE));
    }

    table_disp_->setRowCount(0);
    for (auto d : model::allDays()) {
        const auto& intervalos = prof.disponibilidade(d);
        for (const auto& inter : intervalos) {
            int r = table_disp_->rowCount();
            table_disp_->insertRow(r);

            auto* combo = new QComboBox(table_disp_);
            for (auto day : model::allDays()) {
                combo->addItem(QString::fromStdString(model::dayToLongString(day)), static_cast<int>(day));
            }
            combo->setCurrentIndex(combo->findData(static_cast<int>(d)));
            table_disp_->setCellWidget(r, 0, combo);

            int start_min = inter.start_slot * model::SLOT_MINUTES;
            auto* t_ini = new QTimeEdit(QTime(start_min / 60, start_min % 60), table_disp_);
            t_ini->setDisplayFormat("HH:mm");
            t_ini->setMinimumHeight(28);
            table_disp_->setCellWidget(r, 1, t_ini);

            int end_min = inter.end_slot * model::SLOT_MINUTES;
            auto* t_fim = new QTimeEdit(QTime(end_min / 60, end_min % 60), table_disp_);
            t_fim->setDisplayFormat("HH:mm");
            t_fim->setMinimumHeight(28);
            table_disp_->setCellWidget(r, 2, t_fim);
        }
    }

    // Se o professor não tiver disponibilidade cadastrada, preenche o padrão de dia útil
    if (table_disp_->rowCount() == 0) {
        onSetDefaultMorning();
    }
}

model::Professor ProfessorEditorDialog::getProfessor() const {
    model::Professor prof(txt_id_->text().trimmed().toStdString(),
                          txt_nome_->text().trimmed().toStdString());
    prof.setCargaMaximaDiariaMinutos(spin_carga_max_->value());
    prof.setPesoJanela(slider_peso_janela_->value());

    for (size_t i = 0; i < 5; ++i) {
        auto d = static_cast<model::DayOfWeek>(i);
        prof.setPesoDia(d, sliders_dias_[i]->value());

        std::vector<model::ShiftType> turnos;
        if (check_turnos_dias_[i][0]->isChecked()) turnos.push_back(model::ShiftType::MANHA);
        if (check_turnos_dias_[i][1]->isChecked()) turnos.push_back(model::ShiftType::TARDE);
        if (check_turnos_dias_[i][2]->isChecked()) turnos.push_back(model::ShiftType::NOITE);
        prof.setTurnosPretendidos(d, std::move(turnos));
    }

    for (int r = 0; r < table_disp_->rowCount(); ++r) {
        auto* combo = qobject_cast<QComboBox*>(table_disp_->cellWidget(r, 0));
        auto* t_ini = qobject_cast<QTimeEdit*>(table_disp_->cellWidget(r, 1));
        auto* t_fim = qobject_cast<QTimeEdit*>(table_disp_->cellWidget(r, 2));
        if (combo && t_ini && t_fim) {
            model::DayOfWeek dia;
            QVariant v = combo->currentData();
            if (v.isValid()) {
                dia = static_cast<model::DayOfWeek>(v.toInt());
            } else {
                dia = model::stringToDay(combo->currentText().toStdString());
            }
            std::string s_ini = t_ini->time().toString("HH:mm").toStdString();
            std::string s_fim = t_fim->time().toString("HH:mm").toStdString();
            if (s_ini < s_fim) {
                prof.addIntervaloDisponivel(dia, model::TimeInterval(s_ini, s_fim));
            }
        }
    }

    return prof;
}

} // namespace scheduler::ui
