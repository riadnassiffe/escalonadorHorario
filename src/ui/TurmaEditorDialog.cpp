#include "ui/TurmaEditorDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QMessageBox>
#include <QScrollArea>
#include <algorithm>
#include <map>

namespace scheduler::ui {

TurmaEditorDialog::TurmaEditorDialog(const std::vector<model::Room>& salas_disponiveis,
                                     QWidget* parent,
                                     const model::Turma* turma)
    : QDialog(parent), salas_disponiveis_(salas_disponiveis) {
    setupUi();
    if (turma) {
        loadTurma(*turma);
        setWindowTitle("Editar Turma - " + QString::fromStdString(turma->nome()));
    } else {
        setWindowTitle("Nova Turma");
        // Predefinição padrão: toda a semana pela manhã (07:30 - 11:45)
        applyPresetToAllDays(model::ShiftType::MANHA);
    }
    validateAllDays();
    onSalaModeChanged();
}

void TurmaEditorDialog::setupUi() {
    setMinimumWidth(750);
    setMinimumHeight(580);
    resize(790, 630);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(10);

    // 1. Identificação Básica
    auto* form_card = new QWidget(this);
    auto* form_layout = new QGridLayout(form_card);
    form_layout->setContentsMargins(0, 0, 0, 4);
    form_layout->setHorizontalSpacing(14);
    form_layout->setVerticalSpacing(8);

    txt_id_ = new QLineEdit(form_card);
    txt_id_->setPlaceholderText("Ex: T01");
    txt_id_->setMinimumWidth(110);
    txt_id_->setMaximumWidth(150);

    txt_nome_ = new QLineEdit(form_card);
    txt_nome_->setPlaceholderText("Ex: 1º Ano A - Ensino Médio");

    combo_turno_ = new QComboBox(form_card);
    combo_turno_->addItem("Manhã (MANHA)", static_cast<int>(model::ShiftType::MANHA));
    combo_turno_->addItem("Tarde (TARDE)", static_cast<int>(model::ShiftType::TARDE));
    combo_turno_->addItem("Noite (NOITE)", static_cast<int>(model::ShiftType::NOITE));
    combo_turno_->addItem("Integral / Misto (INTEGRAL)", static_cast<int>(model::ShiftType::INTEGRAL));

    auto* lbl_id = new QLabel("Código (ID):", form_card);
    lbl_id->setStyleSheet("font-weight: 600;");
    auto* lbl_turno = new QLabel("Turno Geral:", form_card);
    lbl_turno->setStyleSheet("font-weight: 600;");
    auto* lbl_nome = new QLabel("Nome da Turma:", form_card);
    lbl_nome->setStyleSheet("font-weight: 600;");

    form_layout->addWidget(lbl_id, 0, 0);
    form_layout->addWidget(txt_id_, 0, 1);
    form_layout->addWidget(lbl_turno, 0, 2);
    form_layout->addWidget(combo_turno_, 0, 3);

    form_layout->addWidget(lbl_nome, 1, 0);
    form_layout->addWidget(txt_nome_, 1, 1, 1, 3);

    main_layout->addWidget(form_card);

    // 2. Abas Principais
    auto* main_tabs = new QTabWidget(this);

    // =========================================================================
    // ABA 1: JORNADAS E HORÁRIOS POR DIA DA SEMANA
    // =========================================================================
    auto* tab_jornadas = new QWidget(main_tabs);
    auto* layout_tab_j = new QVBoxLayout(tab_jornadas);
    layout_tab_j->setContentsMargins(10, 12, 10, 10);
    layout_tab_j->setSpacing(8);

    // Barra de Ações Rápidas para toda a semana
    auto* bar_semana = new QHBoxLayout();
    bar_semana->setSpacing(6);
    auto* lbl_rapido = new QLabel("⚡ Aplicar em toda a semana:", tab_jornadas);
    lbl_rapido->setStyleSheet("font-weight: bold; color: #38bdf8;");
    bar_semana->addWidget(lbl_rapido);

    auto* btn_all_manha = new QPushButton("🌅 Toda Manhã", tab_jornadas);
    btn_all_manha->setObjectName("btnSecondary");
    btn_all_manha->setToolTip("Aplica horário matutino (07:30 - 11:45) para todos os dias");

    auto* btn_all_tarde = new QPushButton("☀️ Toda Tarde", tab_jornadas);
    btn_all_tarde->setObjectName("btnSecondary");
    btn_all_tarde->setToolTip("Aplica horário vespertino (13:15 - 17:30) para todos os dias");

    auto* btn_all_integral = new QPushButton("🌅+☀️ Toda Integral", tab_jornadas);
    btn_all_integral->setObjectName("btnSecondary");
    btn_all_integral->setToolTip("Aplica horário de manhã e tarde com almoço (07:30 - 17:30) para todos os dias");

    auto* btn_all_noite = new QPushButton("🌙 Toda Noite", tab_jornadas);
    btn_all_noite->setObjectName("btnSecondary");
    btn_all_noite->setToolTip("Aplica horário noturno (18:30 - 22:30) para todos os dias");

    bar_semana->addWidget(btn_all_manha);
    bar_semana->addWidget(btn_all_tarde);
    bar_semana->addWidget(btn_all_integral);
    bar_semana->addWidget(btn_all_noite);
    bar_semana->addStretch();
    layout_tab_j->addLayout(bar_semana);

    connect(btn_all_manha, &QPushButton::clicked, this, [this]() {
        applyPresetToAllDays(model::ShiftType::MANHA);
        combo_turno_->setCurrentIndex(combo_turno_->findData(static_cast<int>(model::ShiftType::MANHA)));
    });
    connect(btn_all_tarde, &QPushButton::clicked, this, [this]() {
        applyPresetToAllDays(model::ShiftType::TARDE);
        combo_turno_->setCurrentIndex(combo_turno_->findData(static_cast<int>(model::ShiftType::TARDE)));
    });
    connect(btn_all_integral, &QPushButton::clicked, this, [this]() {
        applyPresetToAllDays(model::ShiftType::INTEGRAL);
        combo_turno_->setCurrentIndex(combo_turno_->findData(static_cast<int>(model::ShiftType::INTEGRAL)));
    });
    connect(btn_all_noite, &QPushButton::clicked, this, [this]() {
        applyPresetToAllDays(model::ShiftType::NOITE);
        combo_turno_->setCurrentIndex(combo_turno_->findData(static_cast<int>(model::ShiftType::NOITE)));
    });

    // Abas de cada dia da semana (Segunda a Sexta)
    tab_dias_ = new QTabWidget(tab_jornadas);
    for (int i = 0; i < 5; ++i) {
        auto d = static_cast<model::DayOfWeek>(i);
        QString tab_title = QString::fromStdString(model::dayToString(d));
        tab_dias_->addTab(createDayTab(i), tab_title);
    }
    layout_tab_j->addWidget(tab_dias_);

    // Resumo semanal
    lbl_resumo_semanal_ = new QLabel(tab_jornadas);
    lbl_resumo_semanal_->setStyleSheet("font-size: 11px; padding: 6px; background-color: rgba(30, 41, 59, 0.7); "
                                       "border: 1px solid #334155; border-radius: 4px; color: #cbd5e1;");
    lbl_resumo_semanal_->setWordWrap(true);
    layout_tab_j->addWidget(lbl_resumo_semanal_);

    main_tabs->addTab(tab_jornadas, "📅 Horários && Jornadas por Dia");

    // =========================================================================
    // ABA 2: SALA TEÓRICA PERMANENTE (SEÇÃO 3.6)
    // =========================================================================
    auto* tab_sala = new QWidget(main_tabs);
    auto* layout_tab_sala = new QVBoxLayout(tab_sala);
    layout_tab_sala->setContentsMargins(12, 14, 12, 12);
    layout_tab_sala->setSpacing(12);

    auto* lbl_sala_info = new QLabel(
        "A Seção 3.6 do modelo acadêmico define a política de sala permanente para aulas teóricas da turma durante a semana.<br>"
        "Você pode definir uma sala fixa específica no cadastro ou permitir que o otimizador escolha a melhor sala fixa permanente.", tab_sala);
    lbl_sala_info->setStyleSheet("color: #94a3b8; font-size: 11px;");
    lbl_sala_info->setWordWrap(true);
    layout_tab_sala->addWidget(lbl_sala_info);

    rad_sala_fixa_ = new QRadioButton("🏛️ Sala Teórica Pré-definida no cadastro:", tab_sala);
    rad_sala_fixa_->setStyleSheet("font-weight: bold; font-size: 13px;");
    rad_sala_fixa_->setChecked(true);
    layout_tab_sala->addWidget(rad_sala_fixa_);

    combo_sala_fixa_ = new QComboBox(tab_sala);
    for (const auto& s : salas_disponiveis_) {
        combo_sala_fixa_->addItem(QString::fromStdString(s.nome() + " [" + s.id() + "]"),
                                  QString::fromStdString(s.id()));
    }
    layout_tab_sala->addWidget(combo_sala_fixa_);

    rad_sala_opt_ = new QRadioButton("🎯 Otimizador seleciona uma sala fixa permanente dentre as permitidas:", tab_sala);
    rad_sala_opt_->setStyleSheet("font-weight: bold; font-size: 13px;");
    layout_tab_sala->addWidget(rad_sala_opt_);

    auto* bar_sel_salas = new QHBoxLayout();
    auto* btn_sel_all = new QPushButton("Selecionar Todas", tab_sala);
    btn_sel_all->setObjectName("btnSecondary");
    auto* btn_desel_all = new QPushButton("Desmarcar Todas", tab_sala);
    btn_desel_all->setObjectName("btnSecondary");
    bar_sel_salas->addWidget(new QLabel("Salas permitidas para escolha:", tab_sala));
    bar_sel_salas->addStretch();
    bar_sel_salas->addWidget(btn_sel_all);
    bar_sel_salas->addWidget(btn_desel_all);
    layout_tab_sala->addLayout(bar_sel_salas);

    list_salas_permitidas_ = new QListWidget(tab_sala);
    list_salas_permitidas_->setSelectionMode(QAbstractItemView::MultiSelection);
    list_salas_permitidas_->setMinimumHeight(180);
    for (const auto& s : salas_disponiveis_) {
        auto* item = new QListWidgetItem(QString::fromStdString(s.nome() + " [" + s.id() + "]"), list_salas_permitidas_);
        item->setData(Qt::UserRole, QString::fromStdString(s.id()));
    }
    layout_tab_sala->addWidget(list_salas_permitidas_);

    connect(btn_sel_all, &QPushButton::clicked, this, [this]() {
        list_salas_permitidas_->selectAll();
    });
    connect(btn_desel_all, &QPushButton::clicked, this, [this]() {
        list_salas_permitidas_->clearSelection();
    });

    connect(rad_sala_fixa_, &QRadioButton::toggled, this, &TurmaEditorDialog::onSalaModeChanged);
    connect(rad_sala_opt_, &QRadioButton::toggled, this, &TurmaEditorDialog::onSalaModeChanged);

    layout_tab_sala->addStretch();

    main_tabs->addTab(tab_sala, "🏛️ Sala Teórica Permanente");

    main_layout->addWidget(main_tabs);

    // 4. Botões de Ação
    auto* btn_box = new QHBoxLayout();
    btn_box->addStretch();
    auto* btn_cancel = new QPushButton("Cancelar", this);
    btn_cancel->setObjectName("btnSecondary");
    auto* btn_save = new QPushButton("Salvar Turma", this);
    btn_box->addWidget(btn_cancel);
    btn_box->addWidget(btn_save);
    main_layout->addLayout(btn_box);

    connect(btn_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btn_save, &QPushButton::clicked, [this]() {
        if (txt_id_->text().trimmed().isEmpty() || txt_nome_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Atenção", "Preencha o Código e o Nome da Turma.");
            return;
        }

        bool has_active_day = false;
        for (int i = 0; i < 5; ++i) {
            if (days_widgets_[i].chk_dia_ativo->isChecked()) {
                has_active_day = true;
                auto j = getJornadaFromWidgets(days_widgets_[i]);
                if (!j.isValid()) {
                    auto d = static_cast<model::DayOfWeek>(i);
                    QMessageBox::critical(this, "Erro no Horário",
                                          QString("O horário de %1 é inconsistente!\n"
                                                  "Condição obrigatória: Início < Intervalos ativos em ordem e sem sobreposição < Fim.")
                                              .arg(QString::fromStdString(model::dayToLongString(d))));
                    tab_dias_->setCurrentIndex(i);
                    return;
                }
            }
        }

        if (!has_active_day) {
            QMessageBox::warning(this, "Atenção", "A turma deve funcionar em ao menos um dia da semana.");
            return;
        }

        if (rad_sala_opt_->isChecked()) {
            if (list_salas_permitidas_->selectedItems().isEmpty()) {
                QMessageBox::warning(this, "Atenção", "Selecione ao menos uma sala permitida para a escolha do otimizador.");
                return;
            }
        }

        accept();
    });
}

QWidget* TurmaEditorDialog::createDayTab(int day_idx) {
    auto* tab_page = new QWidget(this);
    auto* tab_layout = new QVBoxLayout(tab_page);
    tab_layout->setSpacing(8);
    tab_layout->setContentsMargins(10, 10, 10, 10);

    auto& w = days_widgets_[day_idx];
    auto d = static_cast<model::DayOfWeek>(day_idx);

    // Linha Superior: Checkbox do Dia e Botão Copiar
    auto* top_row = new QHBoxLayout();
    w.chk_dia_ativo = new QCheckBox(QString("Turma possui aulas na %1").arg(QString::fromStdString(model::dayToLongString(d))), tab_page);
    w.chk_dia_ativo->setStyleSheet("font-weight: bold; font-size: 13px; color: #f8fafc;");
    w.chk_dia_ativo->setChecked(true);
    top_row->addWidget(w.chk_dia_ativo);
    top_row->addStretch();

    auto* btn_copy = new QPushButton(QString("📋 Copiar para outros dias..."), tab_page);
    btn_copy->setObjectName("btnSecondary");
    btn_copy->setToolTip(QString("Copia o horário e intervalos configurados para %1 para outros dias da semana")
                         .arg(QString::fromStdString(model::dayToLongString(d))));
    top_row->addWidget(btn_copy);
    tab_layout->addLayout(top_row);

    connect(btn_copy, &QPushButton::clicked, this, [this, day_idx]() {
        copyDayScheduleToOthers(day_idx);
    });

    // Painel de Conteúdo dos Horários (habilitado/desabilitado se o dia for letivo)
    w.content_panel = new QWidget(tab_page);
    auto* panel_layout = new QVBoxLayout(w.content_panel);
    panel_layout->setContentsMargins(0, 0, 0, 0);
    panel_layout->setSpacing(8);

    // Barra de Presets do Dia
    auto* presets_row = new QHBoxLayout();
    auto* lbl_pre_dia = new QLabel("Predefinir este dia:", w.content_panel);
    lbl_pre_dia->setStyleSheet("color: #94a3b8;");
    presets_row->addWidget(lbl_pre_dia);

    auto* btn_p_manha = new QPushButton("🌅 Manhã", w.content_panel);
    btn_p_manha->setObjectName("btnSecondary");
    btn_p_manha->setMinimumHeight(28);

    auto* btn_p_tarde = new QPushButton("☀️ Tarde", w.content_panel);
    btn_p_tarde->setObjectName("btnSecondary");
    btn_p_tarde->setMinimumHeight(28);

    auto* btn_p_integral = new QPushButton("🌅+☀️ Manhã e Tarde (Integral)", w.content_panel);
    btn_p_integral->setStyleSheet("font-weight: bold; color: #38bdf8;");
    btn_p_integral->setMinimumHeight(28);

    auto* btn_p_noite = new QPushButton("🌙 Noite", w.content_panel);
    btn_p_noite->setObjectName("btnSecondary");
    btn_p_noite->setMinimumHeight(28);

    presets_row->addWidget(btn_p_manha);
    presets_row->addWidget(btn_p_tarde);
    presets_row->addWidget(btn_p_integral);
    presets_row->addWidget(btn_p_noite);
    presets_row->addStretch();
    panel_layout->addLayout(presets_row);

    connect(btn_p_manha, &QPushButton::clicked, this, [this, day_idx]() {
        applyPresetToDay(day_idx, model::ShiftType::MANHA);
    });
    connect(btn_p_tarde, &QPushButton::clicked, this, [this, day_idx]() {
        applyPresetToDay(day_idx, model::ShiftType::TARDE);
    });
    connect(btn_p_integral, &QPushButton::clicked, this, [this, day_idx]() {
        applyPresetToDay(day_idx, model::ShiftType::INTEGRAL);
    });
    connect(btn_p_noite, &QPushButton::clicked, this, [this, day_idx]() {
        applyPresetToDay(day_idx, model::ShiftType::NOITE);
    });

    // Jornada Total
    auto* row_jornada = new QHBoxLayout();
    w.time_inicio = new QTimeEdit(QTime(7, 30), w.content_panel);
    w.time_inicio->setDisplayFormat("HH:mm");
    w.time_inicio->setMinimumHeight(28);

    w.time_fim = new QTimeEdit(QTime(11, 45), w.content_panel);
    w.time_fim->setDisplayFormat("HH:mm");
    w.time_fim->setMinimumHeight(28);

    auto* lbl_j_ini = new QLabel("⏰ Início da Jornada:", w.content_panel);
    lbl_j_ini->setStyleSheet("font-weight: 600;");
    auto* lbl_j_fim = new QLabel("⏰ Fim da Jornada:", w.content_panel);
    lbl_j_fim->setStyleSheet("font-weight: 600;");

    row_jornada->addWidget(lbl_j_ini);
    row_jornada->addWidget(w.time_inicio);
    row_jornada->addSpacing(20);
    row_jornada->addWidget(lbl_j_fim);
    row_jornada->addWidget(w.time_fim);
    row_jornada->addStretch();
    panel_layout->addLayout(row_jornada);

    // Intervalo 1 (Recreio Manhã)
    auto* row_int1 = new QHBoxLayout();
    w.chk_int1 = new QCheckBox("Intervalo 1 (ex: Manhã)", w.content_panel);
    w.chk_int1->setChecked(true);
    w.chk_int1->setStyleSheet("font-weight: bold;");
    w.time_int1_ini = new QTimeEdit(QTime(9, 40), w.content_panel);
    w.time_int1_ini->setDisplayFormat("HH:mm");
    w.time_int1_ini->setMinimumHeight(28);
    w.time_int1_fim = new QTimeEdit(QTime(9, 55), w.content_panel);
    w.time_int1_fim->setDisplayFormat("HH:mm");
    w.time_int1_fim->setMinimumHeight(28);

    row_int1->addWidget(w.chk_int1);
    row_int1->addSpacing(15);
    row_int1->addWidget(new QLabel("Início:"));
    row_int1->addWidget(w.time_int1_ini);
    row_int1->addSpacing(15);
    row_int1->addWidget(new QLabel("Fim:"));
    row_int1->addWidget(w.time_int1_fim);
    row_int1->addStretch();
    panel_layout->addLayout(row_int1);

    // Intervalo 2 (Almoço / Transição)
    auto* row_int2 = new QHBoxLayout();
    w.chk_int2 = new QCheckBox("Intervalo 2 (ex: Almoço)", w.content_panel);
    w.chk_int2->setChecked(false);
    w.chk_int2->setStyleSheet("font-weight: bold;");
    w.time_int2_ini = new QTimeEdit(QTime(10, 30), w.content_panel);
    w.time_int2_ini->setDisplayFormat("HH:mm");
    w.time_int2_ini->setMinimumHeight(28);
    w.time_int2_ini->setEnabled(false);
    w.time_int2_fim = new QTimeEdit(QTime(10, 45), w.content_panel);
    w.time_int2_fim->setDisplayFormat("HH:mm");
    w.time_int2_fim->setMinimumHeight(28);
    w.time_int2_fim->setEnabled(false);

    row_int2->addWidget(w.chk_int2);
    row_int2->addSpacing(15);
    row_int2->addWidget(new QLabel("Início:"));
    row_int2->addWidget(w.time_int2_ini);
    row_int2->addSpacing(15);
    row_int2->addWidget(new QLabel("Fim:"));
    row_int2->addWidget(w.time_int2_fim);
    row_int2->addStretch();
    panel_layout->addLayout(row_int2);

    // Intervalo 3 (Recreio Tarde)
    auto* row_int3 = new QHBoxLayout();
    w.chk_int3 = new QCheckBox("Intervalo 3 (ex: Tarde)", w.content_panel);
    w.chk_int3->setChecked(false);
    w.chk_int3->setStyleSheet("font-weight: bold;");
    w.time_int3_ini = new QTimeEdit(QTime(11, 0), w.content_panel);
    w.time_int3_ini->setDisplayFormat("HH:mm");
    w.time_int3_ini->setMinimumHeight(28);
    w.time_int3_ini->setEnabled(false);
    w.time_int3_fim = new QTimeEdit(QTime(11, 15), w.content_panel);
    w.time_int3_fim->setDisplayFormat("HH:mm");
    w.time_int3_fim->setMinimumHeight(28);
    w.time_int3_fim->setEnabled(false);

    row_int3->addWidget(w.chk_int3);
    row_int3->addSpacing(15);
    row_int3->addWidget(new QLabel("Início:"));
    row_int3->addWidget(w.time_int3_ini);
    row_int3->addSpacing(15);
    row_int3->addWidget(new QLabel("Fim:"));
    row_int3->addWidget(w.time_int3_fim);
    row_int3->addStretch();
    panel_layout->addLayout(row_int3);

    // Status da jornada do dia
    w.lbl_status = new QLabel(w.content_panel);
    w.lbl_status->setStyleSheet("font-size: 12px; font-weight: bold;");
    w.lbl_status->setWordWrap(true);
    panel_layout->addWidget(w.lbl_status);

    w.content_panel->setMinimumHeight(230);
    tab_layout->addWidget(w.content_panel);
    tab_layout->addStretch();

    // Conexões de validação e ativação
    connect(w.chk_dia_ativo, &QCheckBox::toggled, this, [this, day_idx](bool checked) {
        days_widgets_[day_idx].content_panel->setEnabled(checked);
        validateAllDays();
    });

    connect(w.time_inicio, &QTimeEdit::timeChanged, this, &TurmaEditorDialog::validateAllDays);
    connect(w.time_fim, &QTimeEdit::timeChanged, this, &TurmaEditorDialog::validateAllDays);

    connect(w.chk_int1, &QCheckBox::toggled, this, [this, day_idx](bool checked) {
        days_widgets_[day_idx].time_int1_ini->setEnabled(checked);
        days_widgets_[day_idx].time_int1_fim->setEnabled(checked);
        validateAllDays();
    });
    connect(w.time_int1_ini, &QTimeEdit::timeChanged, this, &TurmaEditorDialog::validateAllDays);
    connect(w.time_int1_fim, &QTimeEdit::timeChanged, this, &TurmaEditorDialog::validateAllDays);

    connect(w.chk_int2, &QCheckBox::toggled, this, [this, day_idx](bool checked) {
        days_widgets_[day_idx].time_int2_ini->setEnabled(checked);
        days_widgets_[day_idx].time_int2_fim->setEnabled(checked);
        validateAllDays();
    });
    connect(w.time_int2_ini, &QTimeEdit::timeChanged, this, &TurmaEditorDialog::validateAllDays);
    connect(w.time_int2_fim, &QTimeEdit::timeChanged, this, &TurmaEditorDialog::validateAllDays);

    connect(w.chk_int3, &QCheckBox::toggled, this, [this, day_idx](bool checked) {
        days_widgets_[day_idx].time_int3_ini->setEnabled(checked);
        days_widgets_[day_idx].time_int3_fim->setEnabled(checked);
        validateAllDays();
    });
    connect(w.time_int3_ini, &QTimeEdit::timeChanged, this, &TurmaEditorDialog::validateAllDays);
    connect(w.time_int3_fim, &QTimeEdit::timeChanged, this, &TurmaEditorDialog::validateAllDays);

    return tab_page;
}

void TurmaEditorDialog::applyPresetToDay(int day_idx, model::ShiftType preset_type) {
    if (day_idx < 0 || day_idx >= 5) return;
    auto& w = days_widgets_[day_idx];

    // Bloquear sinais temporariamente para evitar avaliações intermediárias inconsistentes
    const QSignalBlocker b_ativo(w.chk_dia_ativo);
    const QSignalBlocker b_ini(w.time_inicio);
    const QSignalBlocker b_fim(w.time_fim);
    const QSignalBlocker b_c1(w.chk_int1);
    const QSignalBlocker b_i1(w.time_int1_ini);
    const QSignalBlocker b_f1(w.time_int1_fim);
    const QSignalBlocker b_c2(w.chk_int2);
    const QSignalBlocker b_i2(w.time_int2_ini);
    const QSignalBlocker b_f2(w.time_int2_fim);
    const QSignalBlocker b_c3(w.chk_int3);
    const QSignalBlocker b_i3(w.time_int3_ini);
    const QSignalBlocker b_f3(w.time_int3_fim);

    w.chk_dia_ativo->setChecked(true);
    w.content_panel->setEnabled(true);

    switch (preset_type) {
        case model::ShiftType::MANHA:
            w.time_inicio->setTime(QTime(7, 30));
            w.time_fim->setTime(QTime(11, 45));

            w.chk_int1->setChecked(true);
            w.time_int1_ini->setTime(QTime(9, 40));
            w.time_int1_fim->setTime(QTime(9, 55));
            w.time_int1_ini->setEnabled(true);
            w.time_int1_fim->setEnabled(true);

            w.chk_int2->setChecked(false);
            w.time_int2_ini->setTime(QTime(10, 30));
            w.time_int2_fim->setTime(QTime(10, 45));
            w.time_int2_ini->setEnabled(false);
            w.time_int2_fim->setEnabled(false);

            w.chk_int3->setChecked(false);
            w.time_int3_ini->setTime(QTime(11, 0));
            w.time_int3_fim->setTime(QTime(11, 15));
            w.time_int3_ini->setEnabled(false);
            w.time_int3_fim->setEnabled(false);
            break;

        case model::ShiftType::TARDE:
            w.time_inicio->setTime(QTime(13, 15));
            w.time_fim->setTime(QTime(17, 30));

            w.chk_int1->setChecked(true);
            w.time_int1_ini->setTime(QTime(15, 15));
            w.time_int1_fim->setTime(QTime(15, 30));
            w.time_int1_ini->setEnabled(true);
            w.time_int1_fim->setEnabled(true);

            w.chk_int2->setChecked(false);
            w.time_int2_ini->setTime(QTime(16, 0));
            w.time_int2_fim->setTime(QTime(16, 15));
            w.time_int2_ini->setEnabled(false);
            w.time_int2_fim->setEnabled(false);

            w.chk_int3->setChecked(false);
            w.time_int3_ini->setTime(QTime(16, 30));
            w.time_int3_fim->setTime(QTime(16, 45));
            w.time_int3_ini->setEnabled(false);
            w.time_int3_fim->setEnabled(false);
            break;

        case model::ShiftType::NOITE:
            w.time_inicio->setTime(QTime(18, 30));
            w.time_fim->setTime(QTime(22, 30));

            w.chk_int1->setChecked(true);
            w.time_int1_ini->setTime(QTime(20, 30));
            w.time_int1_fim->setTime(QTime(20, 45));
            w.time_int1_ini->setEnabled(true);
            w.time_int1_fim->setEnabled(true);

            w.chk_int2->setChecked(false);
            w.time_int2_ini->setTime(QTime(21, 0));
            w.time_int2_fim->setTime(QTime(21, 15));
            w.time_int2_ini->setEnabled(false);
            w.time_int2_fim->setEnabled(false);

            w.chk_int3->setChecked(false);
            w.time_int3_ini->setTime(QTime(21, 30));
            w.time_int3_fim->setTime(QTime(21, 45));
            w.time_int3_ini->setEnabled(false);
            w.time_int3_fim->setEnabled(false);
            break;

        case model::ShiftType::INTEGRAL:
            // Manhã e Tarde com pausa para almoço
            w.time_inicio->setTime(QTime(7, 30));
            w.time_fim->setTime(QTime(17, 30));

            w.chk_int1->setChecked(true);
            w.time_int1_ini->setTime(QTime(9, 45));
            w.time_int1_fim->setTime(QTime(10, 0));
            w.time_int1_ini->setEnabled(true);
            w.time_int1_fim->setEnabled(true);

            w.chk_int2->setChecked(true);
            w.time_int2_ini->setTime(QTime(12, 0));
            w.time_int2_fim->setTime(QTime(13, 15)); // Almoço
            w.time_int2_ini->setEnabled(true);
            w.time_int2_fim->setEnabled(true);

            w.chk_int3->setChecked(true);
            w.time_int3_ini->setTime(QTime(15, 30));
            w.time_int3_fim->setTime(QTime(15, 45));
            w.time_int3_ini->setEnabled(true);
            w.time_int3_fim->setEnabled(true);
            break;
    }

    validateAllDays();
}

void TurmaEditorDialog::applyPresetToAllDays(model::ShiftType preset_type) {
    for (int i = 0; i < 5; ++i) {
        applyPresetToDay(i, preset_type);
    }
}

void TurmaEditorDialog::copyDayScheduleToOthers(int source_day_idx) {
    if (source_day_idx < 0 || source_day_idx >= 5) return;
    std::string src_name = model::dayToLongString(static_cast<model::DayOfWeek>(source_day_idx));

    QDialog dlg(this);
    dlg.setWindowTitle("Copiar Horário de " + QString::fromStdString(src_name));
    dlg.setMinimumWidth(380);

    auto* lay = new QVBoxLayout(&dlg);
    lay->setSpacing(10);
    lay->addWidget(new QLabel(QString("Selecione os dias que receberão a mesma jornada e intervalos de <b>%1</b>:")
                              .arg(QString::fromStdString(src_name)), &dlg));

    std::vector<std::pair<int, QCheckBox*>> checks;
    for (int i = 0; i < 5; ++i) {
        if (i == source_day_idx) continue;
        auto* chk = new QCheckBox(QString::fromStdString(model::dayToLongString(static_cast<model::DayOfWeek>(i))), &dlg);
        chk->setChecked(true);
        checks.push_back({i, chk});
        lay->addWidget(chk);
    }

    auto* btn_box = new QHBoxLayout();
    btn_box->addStretch();
    auto* btn_cancel = new QPushButton("Cancelar", &dlg);
    btn_cancel->setObjectName("btnSecondary");
    auto* btn_ok = new QPushButton("Copiar Horários", &dlg);
    btn_box->addWidget(btn_cancel);
    btn_box->addWidget(btn_ok);
    lay->addLayout(btn_box);

    connect(btn_cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(btn_ok, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() == QDialog::Accepted) {
        auto src_jornada = getJornadaFromWidgets(days_widgets_[source_day_idx]);
        bool src_ativo = days_widgets_[source_day_idx].chk_dia_ativo->isChecked();

        for (const auto& [idx, chk] : checks) {
            if (chk->isChecked()) {
                days_widgets_[idx].chk_dia_ativo->setChecked(src_ativo);
                setWidgetsFromJornada(days_widgets_[idx], src_jornada);
            }
        }
        validateAllDays();
    }
}

model::JornadaTurma TurmaEditorDialog::getJornadaFromWidgets(const DayScheduleWidgets& w) {
    try {
        std::string s_ini = w.time_inicio->time().toString("HH:mm").toStdString();
        std::string s_fim = w.time_fim->time().toString("HH:mm").toStdString();
        std::string s_i1 = w.time_int1_ini->time().toString("HH:mm").toStdString();
        std::string s_f1 = w.time_int1_fim->time().toString("HH:mm").toStdString();
        bool a1 = w.chk_int1->isChecked();
        std::string s_i2 = w.time_int2_ini->time().toString("HH:mm").toStdString();
        std::string s_f2 = w.time_int2_fim->time().toString("HH:mm").toStdString();
        bool a2 = w.chk_int2->isChecked();
        std::string s_i3 = w.time_int3_ini->time().toString("HH:mm").toStdString();
        std::string s_f3 = w.time_int3_fim->time().toString("HH:mm").toStdString();
        bool a3 = w.chk_int3->isChecked();

        return model::JornadaTurma(s_ini, s_fim, s_i1, s_f1, a1, s_i2, s_f2, a2, s_i3, s_f3, a3);
    } catch (...) {
        return model::JornadaTurma();
    }
}

void TurmaEditorDialog::setWidgetsFromJornada(DayScheduleWidgets& w, const model::JornadaTurma& j) {
    const QSignalBlocker b_ini(w.time_inicio);
    const QSignalBlocker b_fim(w.time_fim);
    const QSignalBlocker b_c1(w.chk_int1);
    const QSignalBlocker b_i1(w.time_int1_ini);
    const QSignalBlocker b_f1(w.time_int1_fim);
    const QSignalBlocker b_c2(w.chk_int2);
    const QSignalBlocker b_i2(w.time_int2_ini);
    const QSignalBlocker b_f2(w.time_int2_fim);
    const QSignalBlocker b_c3(w.chk_int3);
    const QSignalBlocker b_i3(w.time_int3_ini);
    const QSignalBlocker b_f3(w.time_int3_fim);

    int ini_m = j.jornada_total.start_slot * model::SLOT_MINUTES;
    int fim_m = j.jornada_total.end_slot * model::SLOT_MINUTES;
    w.time_inicio->setTime(QTime(ini_m / 60, ini_m % 60));
    w.time_fim->setTime(QTime(fim_m / 60, fim_m % 60));

    w.chk_int1->setChecked(j.int1_ativo);
    int i1_ini_m = j.intervalo_1.start_slot * model::SLOT_MINUTES;
    int i1_fim_m = j.intervalo_1.end_slot * model::SLOT_MINUTES;
    w.time_int1_ini->setTime(QTime(i1_ini_m / 60, i1_ini_m % 60));
    w.time_int1_fim->setTime(QTime(i1_fim_m / 60, i1_fim_m % 60));
    w.time_int1_ini->setEnabled(j.int1_ativo);
    w.time_int1_fim->setEnabled(j.int1_ativo);

    w.chk_int2->setChecked(j.int2_ativo);
    int i2_ini_m = j.intervalo_2.start_slot * model::SLOT_MINUTES;
    int i2_fim_m = j.intervalo_2.end_slot * model::SLOT_MINUTES;
    w.time_int2_ini->setTime(QTime(i2_ini_m / 60, i2_ini_m % 60));
    w.time_int2_fim->setTime(QTime(i2_fim_m / 60, i2_fim_m % 60));
    w.time_int2_ini->setEnabled(j.int2_ativo);
    w.time_int2_fim->setEnabled(j.int2_ativo);

    w.chk_int3->setChecked(j.int3_ativo);
    int i3_ini_m = j.intervalo_3.start_slot * model::SLOT_MINUTES;
    int i3_fim_m = j.intervalo_3.end_slot * model::SLOT_MINUTES;
    if (i3_ini_m > 0 || i3_fim_m > 0) {
        w.time_int3_ini->setTime(QTime(i3_ini_m / 60, i3_ini_m % 60));
        w.time_int3_fim->setTime(QTime(i3_fim_m / 60, i3_fim_m % 60));
    } else {
        w.time_int3_ini->setTime(QTime(15, 30));
        w.time_int3_fim->setTime(QTime(15, 45));
    }
    w.time_int3_ini->setEnabled(j.int3_ativo);
    w.time_int3_fim->setEnabled(j.int3_ativo);
}

void TurmaEditorDialog::validateAllDays() {
    int total_semana_min = 0;
    QStringList resumo_partes;
    bool has_morning = false;
    bool has_afternoon = false;
    bool has_night = false;

    for (int i = 0; i < 5; ++i) {
        auto& w = days_widgets_[i];
        auto d = static_cast<model::DayOfWeek>(i);
        QString day_abbr = QString::fromStdString(model::dayToString(d));

        if (!w.chk_dia_ativo->isChecked()) {
            tab_dias_->setTabText(i, QString("%1 ⚪").arg(day_abbr));
            tab_dias_->setTabToolTip(i, QString("%1: Sem aulas (dia não letivo)").arg(QString::fromStdString(model::dayToLongString(d))));
            w.lbl_status->setText("⚠️ Turma não tem aula neste dia (dia não letivo).");
            w.lbl_status->setStyleSheet("color: #94a3b8; font-style: italic;");
            resumo_partes << QString("<b>%1</b>: Folga").arg(day_abbr);
            continue;
        }

        model::JornadaTurma j;
        try {
            j = getJornadaFromWidgets(w);
        } catch (...) {
            j = model::JornadaTurma();
        }
        if (j.isValid()) {
            auto blocos = j.blocosLetivos();
            int dia_min = 0;
            QStringList blocos_desc;
            for (const auto& b : blocos) {
                dia_min += b.durationMinutes();
                blocos_desc << QString::fromStdString(b.toString());
            }
            total_semana_min += dia_min;

            // Classificar turno do dia
            int ini_min = j.jornada_total.start_slot * model::SLOT_MINUTES;
            int fim_min = j.jornada_total.end_slot * model::SLOT_MINUTES;
            QString badge;
            QString icon_badge;
            if (ini_min < 12 * 60 + 30 && fim_min > 13 * 60) {
                badge = "Integral";
                icon_badge = "🌅+☀️";
                has_morning = true;
                has_afternoon = true;
            } else if (fim_min <= 13 * 60) {
                badge = "Manhã";
                icon_badge = "🌅";
                has_morning = true;
            } else if (ini_min >= 12 * 60 && fim_min <= 18 * 60 + 30) {
                badge = "Tarde";
                icon_badge = "☀️";
                has_afternoon = true;
            } else {
                badge = "Noite";
                icon_badge = "🌙";
                has_night = true;
            }

            tab_dias_->setTabText(i, QString("%1 %2").arg(day_abbr, icon_badge));
            tab_dias_->setTabToolTip(i, QString("%1 - %2: %3 min letivos").arg(QString::fromStdString(model::dayToLongString(d)), badge, QString::number(dia_min)));
            w.lbl_status->setText(QString("✅ Jornada Válida | %1 Bloco(s) Letivo(s): %2 (%3 min totais)")
                                  .arg(blocos.size())
                                  .arg(blocos_desc.join(", "))
                                  .arg(dia_min));
            w.lbl_status->setStyleSheet("color: #10b981; font-weight: bold;");

            resumo_partes << QString("<b>%1</b>: %2 %3 (%4m)").arg(day_abbr, icon_badge, badge, QString::number(dia_min));
        } else {
            tab_dias_->setTabText(i, QString("%1 ❌").arg(day_abbr));
            tab_dias_->setTabToolTip(i, QString("%1: Horário inconsistente").arg(QString::fromStdString(model::dayToLongString(d))));
            w.lbl_status->setText("❌ Inconsistência: Início < Intervalos ativos em ordem e sem sobreposição < Fim.");
            w.lbl_status->setStyleSheet("color: #ef4444; font-weight: bold;");
            resumo_partes << QString("<b>%1</b>: ❌ Inválido").arg(day_abbr);
        }
    }

    // Atualizar banner de resumo
    lbl_resumo_semanal_->setText(QString("📊 <b>Resumo da Semana:</b> %1 | <b>Total Semanal:</b> %2 minutos (%3 horas)")
                                 .arg(resumo_partes.join(" • "))
                                 .arg(total_semana_min)
                                 .arg(QString::number(total_semana_min / 60.0, 'f', 1)));

    // Auto-ajustar combo_turno se misto (ex: segunda integral, outros dias manhã)
    if (has_morning && has_afternoon) {
        int idx = combo_turno_->findData(static_cast<int>(model::ShiftType::INTEGRAL));
        if (idx >= 0 && combo_turno_->currentIndex() != idx) {
            combo_turno_->setCurrentIndex(idx);
        }
    }
}

void TurmaEditorDialog::onSalaModeChanged() {
    bool is_fixa = rad_sala_fixa_->isChecked();
    combo_sala_fixa_->setEnabled(is_fixa);
    list_salas_permitidas_->setEnabled(!is_fixa);
}

void TurmaEditorDialog::loadTurma(const model::Turma& turma) {
    txt_id_->setText(QString::fromStdString(turma.id()));
    txt_id_->setReadOnly(true);
    txt_nome_->setText(QString::fromStdString(turma.nome()));
    combo_turno_->setCurrentIndex(combo_turno_->findData(static_cast<int>(turma.turno())));

    for (size_t i = 0; i < 5; ++i) {
        auto d = static_cast<model::DayOfWeek>(i);
        bool ativo = turma.funcionaNoDia(d);
        days_widgets_[i].chk_dia_ativo->setChecked(ativo);
        auto j = turma.jornadaParaDia(d);
        setWidgetsFromJornada(days_widgets_[i], j);
    }

    if (turma.temSalaTeoricaPredefinida()) {
        rad_sala_fixa_->setChecked(true);
        int idx = combo_sala_fixa_->findData(QString::fromStdString(turma.salaTeoricaId()));
        if (idx >= 0) combo_sala_fixa_->setCurrentIndex(idx);
    } else {
        rad_sala_opt_->setChecked(true);
        for (int i = 0; i < list_salas_permitidas_->count(); ++i) {
            auto* item = list_salas_permitidas_->item(i);
            std::string s_id = item->data(Qt::UserRole).toString().toStdString();
            if (std::find(turma.salasTeoricasPermitidas().begin(),
                          turma.salasTeoricasPermitidas().end(), s_id) != turma.salasTeoricasPermitidas().end()) {
                item->setSelected(true);
            }
        }
    }

    validateAllDays();
}

model::Turma TurmaEditorDialog::getTurma() const {
    auto shift = static_cast<model::ShiftType>(combo_turno_->currentData().toInt());
    model::Turma t(txt_id_->text().trimmed().toStdString(),
                   txt_nome_->text().trimmed().toStdString(),
                   shift);

    std::vector<model::DayOfWeek> dias;
    std::vector<std::pair<model::DayOfWeek, model::JornadaTurma>> jornadas_ativas;

    for (size_t i = 0; i < 5; ++i) {
        auto d = static_cast<model::DayOfWeek>(i);
        if (days_widgets_[i].chk_dia_ativo->isChecked()) {
            dias.push_back(d);
            jornadas_ativas.push_back({d, getJornadaFromWidgets(days_widgets_[i])});
        }
    }
    t.setDiasLetivos(dias);

    if (jornadas_ativas.empty()) {
        model::JornadaTurma def_j("08:00", "12:30", "09:40", "09:55", true, "11:25", "11:40", false, "12:00", "12:15", false);
        t.setJornadaPadrao(def_j);
    } else {
        // Encontrar a jornada mais frequente para ser a jornada_padrao
        std::vector<model::JornadaTurma> unique_jornadas;
        std::vector<int> freq;
        std::vector<size_t> day_jornada_map;

        for (const auto& [d, j] : jornadas_ativas) {
            size_t u_idx = unique_jornadas.size();
            for (size_t u = 0; u < unique_jornadas.size(); ++u) {
                if (unique_jornadas[u] == j) {
                    u_idx = u;
                    break;
                }
            }
            if (u_idx == unique_jornadas.size()) {
                unique_jornadas.push_back(j);
                freq.push_back(1);
            } else {
                freq[u_idx]++;
            }
            day_jornada_map.push_back(u_idx);
        }

        size_t best_u = 0;
        int max_cnt = -1;
        for (size_t u = 0; u < freq.size(); ++u) {
            if (freq[u] > max_cnt) {
                max_cnt = freq[u];
                best_u = u;
            }
        }

        t.setJornadaPadrao(unique_jornadas[best_u]);
        t.clearExcecoes();

        for (size_t k = 0; k < jornadas_ativas.size(); ++k) {
            if (day_jornada_map[k] != best_u) {
                t.setExcecaoDia(jornadas_ativas[k].first, jornadas_ativas[k].second);
            }
        }
    }

    if (rad_sala_fixa_->isChecked()) {
        t.setSalaTeoricaId(combo_sala_fixa_->currentData().toString().toStdString());
        t.setSalasTeoricasPermitidas({t.salaTeoricaId()});
    } else {
        t.setSalaTeoricaId("");
        std::vector<std::string> permitidas;
        for (auto* item : list_salas_permitidas_->selectedItems()) {
            permitidas.push_back(item->data(Qt::UserRole).toString().toStdString());
        }
        t.setSalasTeoricasPermitidas(permitidas);
    }

    return t;
}

} // namespace scheduler::ui
