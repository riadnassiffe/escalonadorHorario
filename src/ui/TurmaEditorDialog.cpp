/**
 * @file TurmaEditorDialog.cpp
 * @brief Implementação da janela de cadastro e edição de turmas, slots configuráveis e salas associadas.
 */

#include "ui/TurmaEditorDialog.h"
#include "ui/ModernTheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QPushButton>
#include <QMessageBox>
#include <QSpinBox>
#include <QSignalBlocker>
#include <algorithm>
#include <map>

namespace scheduler::ui {

TurmaEditorDialog::TurmaEditorDialog(
    const std::vector<model::Room>& salas_disponiveis,
    QWidget* parent,
    const model::Turma* turma,
    const std::string& default_id,
    const std::map<model::ShiftType, std::vector<model::TimeInterval>>& slot_templates)
    : QDialog(parent),
      salas_disponiveis_(salas_disponiveis),
      slot_templates_(slot_templates) {
    if (turma) {
        id_ = turma->id();
    } else if (!default_id.empty()) {
        id_ = default_id;
    } else {
        id_ = "T01";
    }

    setupUi();
    if (turma) {
        loadTurma(*turma);
        setWindowTitle("Editar Turma - " + QString::fromStdString(turma->nome()));
    } else {
        setWindowTitle("Nova Turma");
        applyPresetToAllDays(model::ShiftType::MANHA);
    }
    validateAllDays();
    onSalaModeChanged();
}

void TurmaEditorDialog::setupUi() {
    setWindowIcon(ModernTheme::icon("turma"));
    setMinimumWidth(780);
    setMinimumHeight(620);
    resize(840, 680);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(12, 12, 12, 12);
    main_layout->setSpacing(10);

    // 1. Identificação Básica
    auto* form_card = new QWidget(this);
    auto* form_layout = new QGridLayout(form_card);
    form_layout->setContentsMargins(0, 0, 0, 4);
    form_layout->setHorizontalSpacing(14);
    form_layout->setVerticalSpacing(8);

    txt_nome_ = new QLineEdit(form_card);
    txt_nome_->setPlaceholderText("Ex: 1º Ano A - Ensino Médio");

    combo_turno_ = new QComboBox(form_card);
    combo_turno_->addItem("Manhã (MANHA)", static_cast<int>(model::ShiftType::MANHA));
    combo_turno_->addItem("Tarde (TARDE)", static_cast<int>(model::ShiftType::TARDE));
    combo_turno_->addItem("Noite (NOITE)", static_cast<int>(model::ShiftType::NOITE));
    combo_turno_->addItem("Integral / Misto (INTEGRAL)", static_cast<int>(model::ShiftType::INTEGRAL));

    auto* lbl_nome = new QLabel("Nome da Turma:", form_card);
    lbl_nome->setStyleSheet("font-weight: 600; font-size: 13px;");
    auto* lbl_turno = new QLabel("Turno Geral:", form_card);
    lbl_turno->setStyleSheet("font-weight: 600; font-size: 13px;");

    form_layout->addWidget(lbl_nome, 0, 0);
    form_layout->addWidget(txt_nome_, 0, 1);
    form_layout->addWidget(lbl_turno, 0, 2);
    form_layout->addWidget(combo_turno_, 0, 3);

    main_layout->addWidget(form_card);

    // 2. Abas Principais
    auto* main_tabs = new QTabWidget(this);

    // =========================================================================
    // ABA 1: SLOTS DE HORÁRIO POR DIA DA SEMANA
    // =========================================================================
    auto* tab_jornadas = new QWidget(main_tabs);
    auto* layout_tab_j = new QVBoxLayout(tab_jornadas);
    layout_tab_j->setContentsMargins(10, 12, 10, 10);
    layout_tab_j->setSpacing(8);

    auto* bar_semana = new QHBoxLayout();
    bar_semana->setSpacing(6);
    auto* lbl_rapido = new QLabel("⚡ Aplicar modelo em toda a semana:", tab_jornadas);
    lbl_rapido->setStyleSheet("font-weight: bold; color: palette(highlight);");
    bar_semana->addWidget(lbl_rapido);

    auto* btn_all_manha = new QPushButton(ModernTheme::icon("morning"), "Modelo Manhã", tab_jornadas);
    btn_all_manha->setObjectName("btnSecondary");
    auto* btn_all_tarde = new QPushButton(ModernTheme::icon("afternoon"), "Modelo Tarde", tab_jornadas);
    btn_all_tarde->setObjectName("btnSecondary");
    auto* btn_all_integral = new QPushButton(ModernTheme::icon("integral"), "Modelo Integral", tab_jornadas);
    btn_all_integral->setObjectName("btnSecondary");
    auto* btn_all_noite = new QPushButton(ModernTheme::icon("evening"), "Modelo Noite", tab_jornadas);
    btn_all_noite->setObjectName("btnSecondary");

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

    tab_dias_ = new QTabWidget(tab_jornadas);
    for (int i = 0; i < 5; ++i) {
        auto d = static_cast<model::DayOfWeek>(i);
        QString tab_title = QString::fromStdString(model::dayToString(d));
        tab_dias_->addTab(createDayTab(i), tab_title);
    }
    layout_tab_j->addWidget(tab_dias_);

    lbl_resumo_semanal_ = new QLabel(tab_jornadas);
    lbl_resumo_semanal_->setStyleSheet("font-size: 11px; padding: 6px; background-color: palette(base); "
                                       "border: 1px solid palette(mid); border-radius: 4px;");
    lbl_resumo_semanal_->setWordWrap(true);
    layout_tab_j->addWidget(lbl_resumo_semanal_);

    main_tabs->addTab(tab_jornadas, ModernTheme::icon("calendar"), "Slots && Horários por Dia");

    // =========================================================================
    // ABA 2: SALA TEÓRICA PERMANENTE
    // =========================================================================
    auto* tab_sala = new QWidget(main_tabs);
    auto* layout_tab_sala = new QVBoxLayout(tab_sala);
    layout_tab_sala->setContentsMargins(12, 14, 12, 12);
    layout_tab_sala->setSpacing(12);

    auto* lbl_sala_info = new QLabel(
        "Defina a política de sala permanente para aulas teóricas da turma durante a semana.<br>"
        "Você pode definir uma sala fixa específica no cadastro ou permitir que o otimizador escolha a melhor sala fixa permanente.", tab_sala);
    lbl_sala_info->setStyleSheet("font-size: 11px;");
    lbl_sala_info->setWordWrap(true);
    layout_tab_sala->addWidget(lbl_sala_info);

    rad_sala_fixa_ = new QRadioButton("🏛️ Sala Teórica Pré-definida no cadastro:", tab_sala);
    rad_sala_fixa_->setStyleSheet("font-weight: bold; font-size: 13px;");
    rad_sala_fixa_->setChecked(true);
    layout_tab_sala->addWidget(rad_sala_fixa_);

    combo_sala_fixa_ = new QComboBox(tab_sala);
    for (const auto& s : salas_disponiveis_) {
        combo_sala_fixa_->addItem(QString::fromStdString(s.nome()),
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
        auto* item = new QListWidgetItem(QString::fromStdString(s.nome()), list_salas_permitidas_);
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

    main_tabs->addTab(tab_sala, ModernTheme::icon("room"), "Sala Teórica Permanente");

    main_layout->addWidget(main_tabs);

    // 4. Botões de Ação
    auto* btn_box = new QHBoxLayout();
    btn_box->addStretch();
    auto* btn_cancel = new QPushButton("Cancelar", this);
    btn_cancel->setObjectName("btnSecondary");
    auto* btn_save = new QPushButton(ModernTheme::icon("save_file"), "Salvar Turma", this);
    btn_box->addWidget(btn_cancel);
    btn_box->addWidget(btn_save);
    main_layout->addLayout(btn_box);

    connect(btn_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btn_save, &QPushButton::clicked, [this]() {
        if (txt_nome_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Atenção", "Preencha o Nome da Turma.");
            return;
        }

        bool has_active_day = false;
        for (int i = 0; i < 5; ++i) {
            if (days_widgets_[i].chk_dia_ativo->isChecked()) {
                has_active_day = true;
                auto j = getJornadaFromWidgets(days_widgets_[i]);
                if (!j.isValid()) {
                    auto d = static_cast<model::DayOfWeek>(i);
                    QMessageBox::critical(this, "Erro nos Slots",
                                          QString("Os slots de %1 são inválidos!\n"
                                                  "Cada slot deve ter Início < Fim, não pode haver sobreposição entre slots e o dia deve ter ao menos 1 slot.")
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

        if (rad_sala_opt_->isChecked() && list_salas_permitidas_->selectedItems().isEmpty()) {
            QMessageBox::warning(this, "Atenção", "Selecione ao menos uma sala permitida para a escolha do otimizador.");
            return;
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

    auto* top_row = new QHBoxLayout();
    w.chk_dia_ativo = new QCheckBox(QString("Turma possui aulas na %1").arg(QString::fromStdString(model::dayToLongString(d))), tab_page);
    w.chk_dia_ativo->setStyleSheet("font-weight: bold; font-size: 13px;");
    w.chk_dia_ativo->setChecked(true);
    top_row->addWidget(w.chk_dia_ativo);
    top_row->addStretch();

    auto* btn_copy = new QPushButton(ModernTheme::icon("copy"), "Copiar para outros dias...", tab_page);
    btn_copy->setObjectName("btnSecondary");
    top_row->addWidget(btn_copy);
    tab_layout->addLayout(top_row);

    connect(btn_copy, &QPushButton::clicked, this, [this, day_idx]() {
        copyDayScheduleToOthers(day_idx);
    });

    w.content_panel = new QWidget(tab_page);
    auto* panel_layout = new QVBoxLayout(w.content_panel);
    panel_layout->setContentsMargins(0, 0, 0, 0);
    panel_layout->setSpacing(8);

    // Barra de Ações do Dia (Presets + Ferramentas de Slots)
    auto* actions_row = new QHBoxLayout();
    actions_row->setSpacing(6);

    auto* btn_add_slot = new QPushButton(ModernTheme::icon("add"), "Adicionar Slot", w.content_panel);
    btn_add_slot->setObjectName("btnSecondary");
    auto* btn_rem_slot = new QPushButton(ModernTheme::icon("delete"), "Remover", w.content_panel);
    btn_rem_slot->setObjectName("btnSecondary");
    auto* btn_gen_slots = new QPushButton("⚡ Gerar em Lote...", w.content_panel);
    btn_gen_slots->setObjectName("btnSecondary");

    actions_row->addWidget(btn_add_slot);
    actions_row->addWidget(btn_rem_slot);
    actions_row->addWidget(btn_gen_slots);
    actions_row->addSpacing(12);

    auto* btn_p_manha = new QPushButton(ModernTheme::icon("morning"), "Manhã", w.content_panel);
    btn_p_manha->setObjectName("btnSecondary");
    auto* btn_p_tarde = new QPushButton(ModernTheme::icon("afternoon"), "Tarde", w.content_panel);
    btn_p_tarde->setObjectName("btnSecondary");
    auto* btn_p_integral = new QPushButton(ModernTheme::icon("integral"), "Integral", w.content_panel);
    btn_p_integral->setObjectName("btnSecondary");
    auto* btn_p_noite = new QPushButton(ModernTheme::icon("evening"), "Noite", w.content_panel);
    btn_p_noite->setObjectName("btnSecondary");

    actions_row->addWidget(btn_p_manha);
    actions_row->addWidget(btn_p_tarde);
    actions_row->addWidget(btn_p_integral);
    actions_row->addWidget(btn_p_noite);
    actions_row->addStretch();
    panel_layout->addLayout(actions_row);

    connect(btn_add_slot, &QPushButton::clicked, this, [this, day_idx]() {
        auto& tbl = days_widgets_[day_idx].table_slots;
        QTime next_ini(8, 0);
        int dur = 50;
        if (tbl->rowCount() > 0) {
            auto* last_end = qobject_cast<QTimeEdit*>(tbl->cellWidget(tbl->rowCount() - 1, 2));
            auto* last_ini = qobject_cast<QTimeEdit*>(tbl->cellWidget(tbl->rowCount() - 1, 1));
            if (last_end) next_ini = last_end->time();
            if (last_ini && last_end) {
                int d_min = last_ini->time().secsTo(last_end->time()) / 60;
                if (d_min > 0) dur = d_min;
            }
        }
        addSlotRow(day_idx, next_ini, next_ini.addSecs(dur * 60));
        validateAllDays();
    });
    connect(btn_rem_slot, &QPushButton::clicked, this, [this, day_idx]() {
        removeSelectedSlotRow(day_idx);
    });
    connect(btn_gen_slots, &QPushButton::clicked, this, [this, day_idx]() {
        openGenerateSlotsDialog(day_idx);
    });

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

    // Tabela de Slots do Dia
    w.table_slots = new QTableWidget(0, 4, w.content_panel);
    w.table_slots->setHorizontalHeaderLabels({"Slot", "Início (HH:mm)", "Fim (HH:mm)", "Duração"});
    w.table_slots->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    w.table_slots->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    w.table_slots->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    w.table_slots->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    w.table_slots->setSelectionBehavior(QAbstractItemView::SelectRows);
    w.table_slots->setSelectionMode(QAbstractItemView::SingleSelection);
    w.table_slots->verticalHeader()->setVisible(false);
    w.table_slots->setMinimumHeight(210);
    panel_layout->addWidget(w.table_slots);

    auto* lbl_hint = new QLabel(
        "💡 Cadastre os horários de início e fim de cada aula. Lacunas entre o término de um slot e o início do próximo são tratadas automaticamente como intervalos/recreios.",
        w.content_panel);
    lbl_hint->setStyleSheet("font-size: 11px; color: palette(dark);");
    lbl_hint->setWordWrap(true);
    panel_layout->addWidget(lbl_hint);

    w.lbl_status = new QLabel(w.content_panel);
    w.lbl_status->setStyleSheet("font-size: 12px; font-weight: bold;");
    w.lbl_status->setWordWrap(true);
    panel_layout->addWidget(w.lbl_status);

    tab_layout->addWidget(w.content_panel);

    connect(w.chk_dia_ativo, &QCheckBox::toggled, this, [this, day_idx](bool checked) {
        days_widgets_[day_idx].content_panel->setEnabled(checked);
        validateAllDays();
    });

    return tab_page;
}

void TurmaEditorDialog::addSlotRow(int day_idx, const QTime& start_t, const QTime& end_t) {
    auto* tbl = days_widgets_[day_idx].table_slots;
    int r = tbl->rowCount();
    tbl->insertRow(r);

    auto* item_idx = new QTableWidgetItem(QString("Slot %1").arg(r + 1));
    item_idx->setTextAlignment(Qt::AlignCenter);
    item_idx->setFlags(item_idx->flags() & ~Qt::ItemIsEditable);
    tbl->setItem(r, 0, item_idx);

    auto* edit_ini = new QTimeEdit(start_t, tbl);
    edit_ini->setDisplayFormat("HH:mm");
    edit_ini->setMinimumHeight(26);
    tbl->setCellWidget(r, 1, edit_ini);

    auto* edit_fim = new QTimeEdit(end_t, tbl);
    edit_fim->setDisplayFormat("HH:mm");
    edit_fim->setMinimumHeight(26);
    tbl->setCellWidget(r, 2, edit_fim);

    int dur_min = start_t.secsTo(end_t) / 60;
    auto* item_dur = new QTableWidgetItem(QString("%1 min").arg(dur_min));
    item_dur->setTextAlignment(Qt::AlignCenter);
    item_dur->setFlags(item_dur->flags() & ~Qt::ItemIsEditable);
    tbl->setItem(r, 3, item_dur);

    tbl->setRowHeight(r, 32);

    connect(edit_ini, &QTimeEdit::timeChanged, this, [this, day_idx]() {
        updateDaySlotDurations(day_idx);
        validateAllDays();
    });
    connect(edit_fim, &QTimeEdit::timeChanged, this, [this, day_idx]() {
        updateDaySlotDurations(day_idx);
        validateAllDays();
    });
}

void TurmaEditorDialog::removeSelectedSlotRow(int day_idx) {
    auto* tbl = days_widgets_[day_idx].table_slots;
    int r = tbl->currentRow();
    if (r < 0 && tbl->rowCount() > 0) {
        r = tbl->rowCount() - 1;
    }
    if (r >= 0) {
        tbl->removeRow(r);
        updateDaySlotDurations(day_idx);
        validateAllDays();
    }
}

void TurmaEditorDialog::updateDaySlotDurations(int day_idx) {
    auto* tbl = days_widgets_[day_idx].table_slots;
    for (int r = 0; r < tbl->rowCount(); ++r) {
        if (auto* idx_item = tbl->item(r, 0)) {
            idx_item->setText(QString("Slot %1").arg(r + 1));
        }
        auto* edit_ini = qobject_cast<QTimeEdit*>(tbl->cellWidget(r, 1));
        auto* edit_fim = qobject_cast<QTimeEdit*>(tbl->cellWidget(r, 2));
        auto* dur_item = tbl->item(r, 3);
        if (edit_ini && edit_fim && dur_item) {
            int dur_min = edit_ini->time().secsTo(edit_fim->time()) / 60;
            dur_item->setText(QString("%1 min").arg(dur_min));
        }
    }
}

void TurmaEditorDialog::openGenerateSlotsDialog(int day_idx) {
    QDialog dlg(this);
    dlg.setWindowTitle("Gerar Slots do Dia em Lote");
    dlg.setMinimumWidth(360);

    auto* lay = new QVBoxLayout(&dlg);
    auto* form = new QFormLayout();

    auto* time_ini = new QTimeEdit(QTime(7, 30), &dlg);
    time_ini->setDisplayFormat("HH:mm");
    auto* spin_qtd = new QSpinBox(&dlg);
    spin_qtd->setRange(1, 20);
    spin_qtd->setValue(5);
    auto* spin_dur = new QSpinBox(&dlg);
    spin_dur->setRange(10, 240);
    spin_dur->setValue(50);
    spin_dur->setSuffix(" min");

    auto* chk_recreio = new QCheckBox("Inserir intervalo após o slot:", &dlg);
    chk_recreio->setChecked(true);
    auto* spin_pos_int = new QSpinBox(&dlg);
    spin_pos_int->setRange(1, 19);
    spin_pos_int->setValue(3);
    auto* spin_dur_int = new QSpinBox(&dlg);
    spin_dur_int->setRange(5, 180);
    spin_dur_int->setValue(15);
    spin_dur_int->setSuffix(" min");

    form->addRow("Horário Inicial:", time_ini);
    form->addRow("Quantidade de Slots:", spin_qtd);
    form->addRow("Duração de Cada Slot:", spin_dur);
    form->addRow(chk_recreio, spin_pos_int);
    form->addRow("Duração do Intervalo:", spin_dur_int);
    lay->addLayout(form);

    auto* btns = new QHBoxLayout();
    btns->addStretch();
    auto* btn_cancel = new QPushButton("Cancelar", &dlg);
    btn_cancel->setObjectName("btnSecondary");
    auto* btn_ok = new QPushButton("Gerar Slots", &dlg);
    btns->addWidget(btn_cancel);
    btns->addWidget(btn_ok);
    lay->addLayout(btns);

    connect(btn_cancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(btn_ok, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() == QDialog::Accepted) {
        std::vector<model::TimeInterval> generated;
        QTime cur = time_ini->time();
        int qtd = spin_qtd->value();
        int dur = spin_dur->value();
        int int_after = chk_recreio->isChecked() ? spin_pos_int->value() : -1;
        int int_dur = spin_dur_int->value();

        for (int i = 1; i <= qtd; ++i) {
            QTime next = cur.addSecs(dur * 60);
            int s_m = cur.hour() * 60 + cur.minute();
            int e_m = next.hour() * 60 + next.minute();
            if (e_m <= s_m) break;
            generated.emplace_back(s_m, e_m);
            cur = next;
            if (i == int_after) {
                cur = cur.addSecs(int_dur * 60);
            }
        }
        setWidgetsFromJornada(day_idx, model::JornadaTurma(std::move(generated)));
        validateAllDays();
    }
}

void TurmaEditorDialog::applyPresetToDay(int day_idx, model::ShiftType preset_type) {
    if (day_idx < 0 || day_idx >= 5) return;
    auto& w = days_widgets_[day_idx];
    const QSignalBlocker b_ativo(w.chk_dia_ativo);
    w.chk_dia_ativo->setChecked(true);
    w.content_panel->setEnabled(true);

    std::vector<model::TimeInterval> slot_list;
    auto it = slot_templates_.find(preset_type);
    if (it != slot_templates_.end() && !it->second.empty()) {
        slot_list = it->second;
    } else {
        slot_list = model::JornadaTurma::defaultSlotsForShift(preset_type);
    }

    setWidgetsFromJornada(day_idx, model::JornadaTurma(std::move(slot_list)));
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
    dlg.setWindowTitle("Copiar Slots de " + QString::fromStdString(src_name));
    dlg.setMinimumWidth(380);

    auto* lay = new QVBoxLayout(&dlg);
    lay->setSpacing(10);
    lay->addWidget(new QLabel(QString("Selecione os dias que receberão os mesmos slots de <b>%1</b>:")
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
    auto* btn_ok = new QPushButton("Copiar Slots", &dlg);
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
                setWidgetsFromJornada(idx, src_jornada);
            }
        }
        validateAllDays();
    }
}

model::JornadaTurma TurmaEditorDialog::getJornadaFromWidgets(const DayScheduleWidgets& w) {
    std::vector<model::TimeInterval> slot_list;
    if (!w.table_slots) return model::JornadaTurma();

    for (int r = 0; r < w.table_slots->rowCount(); ++r) {
        auto* edit_ini = qobject_cast<QTimeEdit*>(w.table_slots->cellWidget(r, 1));
        auto* edit_fim = qobject_cast<QTimeEdit*>(w.table_slots->cellWidget(r, 2));
        if (!edit_ini || !edit_fim) continue;
        int s_min = edit_ini->time().hour() * 60 + edit_ini->time().minute();
        int e_min = edit_fim->time().hour() * 60 + edit_fim->time().minute();
        if (s_min > e_min) {
            return model::JornadaTurma(); // Inválido
        }
        slot_list.emplace_back(s_min, e_min);
    }
    return model::JornadaTurma(std::move(slot_list));
}

void TurmaEditorDialog::setWidgetsFromJornada(int day_idx, const model::JornadaTurma& j) {
    auto* tbl = days_widgets_[day_idx].table_slots;
    tbl->setRowCount(0);
    for (const auto& s : j.slots_letivos) {
        QTime st(s.start_min / 60, s.start_min % 60);
        QTime et(s.end_min / 60, s.end_min % 60);
        addSlotRow(day_idx, st, et);
    }
}

void TurmaEditorDialog::validateAllDays() {
    int total_semana_min = 0;
    int total_semana_slots = 0;
    QStringList resumo_partes;
    bool has_morning = false;
    bool has_afternoon = false;

    for (int i = 0; i < 5; ++i) {
        auto& w = days_widgets_[i];
        auto d = static_cast<model::DayOfWeek>(i);
        QString day_abbr = QString::fromStdString(model::dayToString(d));

        if (!w.chk_dia_ativo->isChecked()) {
            tab_dias_->setTabText(i, QString("%1 ⚪").arg(day_abbr));
            tab_dias_->setTabToolTip(i, QString("%1: Sem aulas (dia não letivo)").arg(QString::fromStdString(model::dayToLongString(d))));
            w.lbl_status->setText("⚠️ Turma não tem aula neste dia (dia não letivo).");
            w.lbl_status->setStyleSheet("font-style: italic;");
            resumo_partes << QString("<b>%1</b>: Folga").arg(day_abbr);
            continue;
        }

        // Checar erros crus na tabela (start >= end ou sobreposição)
        bool raw_valid = (w.table_slots->rowCount() > 0);
        std::vector<model::TimeInterval> raw_slots;
        for (int r = 0; r < w.table_slots->rowCount(); ++r) {
            auto* edit_ini = qobject_cast<QTimeEdit*>(w.table_slots->cellWidget(r, 1));
            auto* edit_fim = qobject_cast<QTimeEdit*>(w.table_slots->cellWidget(r, 2));
            if (!edit_ini || !edit_fim) { raw_valid = false; break; }
            int s_m = edit_ini->time().hour() * 60 + edit_ini->time().minute();
            int e_m = edit_fim->time().hour() * 60 + edit_fim->time().minute();
            if (s_m >= e_m) { raw_valid = false; break; }
            raw_slots.emplace_back(s_m, e_m);
        }

        model::JornadaTurma j = raw_valid ? model::JornadaTurma(raw_slots) : model::JornadaTurma();
        if (raw_valid && j.isValid()) {
            int dia_min = j.minutosLetivos();
            total_semana_min += dia_min;
            total_semana_slots += static_cast<int>(j.slots_letivos.size());

            std::map<int, int> dur_counts;
            for (const auto& s : j.slots_letivos) dur_counts[s.durationMinutes()]++;
            QStringList dur_parts;
            for (const auto& [dur, cnt] : dur_counts) {
                dur_parts << QString("%1×%2m").arg(cnt).arg(dur);
            }

            auto ints = j.intervalosAtivos();
            QStringList int_parts;
            for (const auto& inter : ints) {
                int_parts << QString("%1 (%2m)").arg(QString::fromStdString(inter.toString())).arg(inter.durationMinutes());
            }

            auto j_total = j.jornadaTotal();
            QString badge;
            QString icon_badge;
            if (j_total.start_min < 12 * 60 + 30 && j_total.end_min > 13 * 60) {
                badge = "Integral";
                icon_badge = "🌅+☀️";
                has_morning = true;
                has_afternoon = true;
            } else if (j_total.end_min <= 13 * 60) {
                badge = "Manhã";
                icon_badge = "🌅";
                has_morning = true;
            } else if (j_total.start_min >= 12 * 60 && j_total.end_min <= 18 * 60 + 30) {
                badge = "Tarde";
                icon_badge = "☀️";
                has_afternoon = true;
            } else {
                badge = "Noite";
                icon_badge = "🌙";
            }

            tab_dias_->setTabText(i, QString("%1 %2").arg(day_abbr, icon_badge));
            tab_dias_->setTabToolTip(i, QString("%1 - %2: %3 slots (%4 min)").arg(QString::fromStdString(model::dayToLongString(d)), badge).arg(j.slots_letivos.size()).arg(dia_min));
            w.lbl_status->setText(QString("✅ %1 slot(s) válidos [%2] | Total: %3 min | Intervalos: %4")
                                  .arg(j.slots_letivos.size())
                                  .arg(dur_parts.join(", "))
                                  .arg(dia_min)
                                  .arg(int_parts.isEmpty() ? "Nenhum" : int_parts.join(", ")));
            w.lbl_status->setStyleSheet("color: #16a34a; font-weight: bold;");

            resumo_partes << QString("<b>%1</b>: %2 %3 (%4 slots, %5m)")
                                 .arg(day_abbr, icon_badge, badge)
                                 .arg(j.slots_letivos.size())
                                 .arg(dia_min);
        } else {
            tab_dias_->setTabText(i, QString("%1 ❌").arg(day_abbr));
            tab_dias_->setTabToolTip(i, QString("%1: Slots inconsistentes").arg(QString::fromStdString(model::dayToLongString(d))));
            w.lbl_status->setText("❌ Inconsistência: cada dia ativo precisa ter ao menos 1 slot com Início < Fim e sem sobreposição entre slots.");
            w.lbl_status->setStyleSheet("color: #dc2626; font-weight: bold;");
            resumo_partes << QString("<b>%1</b>: ❌ Inválido").arg(day_abbr);
        }
    }

    lbl_resumo_semanal_->setText(QString("📊 <b>Resumo da Semana:</b> %1 | <b>Total Semanal:</b> %2 slots, %3 min (%4 h)")
                                 .arg(resumo_partes.join(" • "))
                                 .arg(total_semana_slots)
                                 .arg(total_semana_min)
                                 .arg(QString::number(total_semana_min / 60.0, 'f', 1)));

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
    id_ = turma.id();
    txt_nome_->setText(QString::fromStdString(turma.nome()));
    combo_turno_->setCurrentIndex(combo_turno_->findData(static_cast<int>(turma.turno())));

    for (int i = 0; i < 5; ++i) {
        auto d = static_cast<model::DayOfWeek>(i);
        bool ativo = turma.funcionaNoDia(d);
        days_widgets_[i].chk_dia_ativo->setChecked(ativo);
        auto j = turma.jornadaParaDia(d);
        setWidgetsFromJornada(i, j);
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
    model::Turma t(id_,
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
        t.setJornadaPadrao(model::JornadaTurma(model::JornadaTurma::defaultSlotsForShift(shift)));
    } else {
        std::vector<model::JornadaTurma> unique_jornadas;
        std::vector<int> freq;
        std::vector<size_t> day_jornada_map;

        for (const auto& [d, j] : jornadas_ativas) {
            (void)d;
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
