/**
 * @file MoveLessonDialog.cpp
 * @brief Implementação do diálogo para mover aulas e validar conflitos de grade.
 */

#include "ui/MoveLessonDialog.h"
#include "ui/ModernTheme.h"
#include "ui/ResponsiveUi.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <algorithm>

namespace scheduler::ui {

/**
 * @brief Construtor da janela de movimentação manual de aula.
 * @param data Base de dados da instituição.
 * @param aula_id ID da aula a movimentar.
 * @param available_slots Intervalos horários disponíveis.
 * @param parent Widget pai na hierarquia Qt.
 */
MoveLessonDialog::MoveLessonDialog(model::InstitutionData& data,
                                   const std::string& aula_id,
                                   const std::vector<model::TimeInterval>& available_slots,
                                   QWidget* parent)
    : QDialog(parent), data_(data), aula_id_(aula_id), available_slots_(available_slots) {
    setWindowTitle("Mover Aula Manualmente");
    setWindowIcon(ModernTheme::icon("swap"));
    setMinimumSize(420, 340);
    setupUi();
    updateValidationStatus();
    fitWindowToScreen(this, QSize(520, 460));
}

/**
 * @brief Constrói o layout, cartões informativos e controles de seleção de dia, horário e sala.
 */
void MoveLessonDialog::setupUi() {
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setSpacing(10);
    root_layout->setContentsMargins(12, 12, 12, 12);

    auto* body_widget = new QWidget(this);
    auto* main_layout = new QVBoxLayout(body_widget);
    main_layout->setSpacing(12);
    main_layout->setContentsMargins(2, 2, 2, 2);

    const auto* aula_aloc = data_.grade().findAula(aula_id_);
    const auto* aula_model = data_.findAula(aula_id_);

    // Card informativo da aula
    auto* grp_info = new QGroupBox("Detalhes da Aula Selecionada", body_widget);
    auto* layout_info = new QFormLayout(grp_info);
    layout_info->setSpacing(8);
    layout_info->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    QString disc_title = aula_aloc ? QString::fromStdString(aula_aloc->disciplina_nome) : "Aula Desconhecida";
    if (aula_model) {
        disc_title += QString(" (%1 - %2 min)").arg(QString::fromStdString(aula_model->tipo())).arg(aula_model->duracaoMinutos());
    }
    auto* lbl_disc = new QLabel(disc_title, grp_info);
    lbl_disc->setWordWrap(true);
    lbl_disc->setStyleSheet("font-weight: bold; font-size: 13px; color: palette(highlight);");
    layout_info->addRow("Disciplina:", lbl_disc);

    QString prof_names;
    if (aula_aloc) {
        for (size_t i = 0; i < aula_aloc->professores.size(); ++i) {
            if (i > 0) prof_names += ", ";
            const auto* p = data_.findProfessor(aula_aloc->professores[i]);
            prof_names += QString::fromStdString(p ? p->nome() : aula_aloc->professores[i]);
        }
    }
    auto* lbl_profs = new QLabel(prof_names.isEmpty() ? "Não informado" : prof_names, grp_info);
    lbl_profs->setWordWrap(true);
    layout_info->addRow("Professor(es):", lbl_profs);

    QString turma_names;
    if (aula_aloc) {
        for (size_t i = 0; i < aula_aloc->turmas.size(); ++i) {
            if (i > 0) turma_names += ", ";
            const auto* t = data_.findTurma(aula_aloc->turmas[i]);
            turma_names += QString::fromStdString(t ? t->nome() : aula_aloc->turmas[i]);
        }
    }
    auto* lbl_turmas = new QLabel(turma_names.isEmpty() ? "Não informada" : turma_names, grp_info);
    lbl_turmas->setWordWrap(true);
    layout_info->addRow("Turma(s):", lbl_turmas);

    QString cur_time = aula_aloc ? QString("%1 às %2").arg(QString::fromStdString(model::dayToLongString(aula_aloc->dia)))
                                                          .arg(QString::fromStdString(aula_aloc->intervalo.toString()))
                                   : "Não alocada";
    auto* lbl_cur = new QLabel(cur_time, grp_info);
    lbl_cur->setWordWrap(true);
    lbl_cur->setStyleSheet("font-weight: 500;");
    layout_info->addRow("Horário Atual:", lbl_cur);

    main_layout->addWidget(grp_info);

    // Painel de seleção de novo horário
    auto* grp_destino = new QGroupBox("Novo Posicionamento na Grade", body_widget);
    auto* form_destino = new QFormLayout(grp_destino);
    form_destino->setSpacing(8);
    form_destino->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    combo_dia_ = new QComboBox(grp_destino);
    for (auto d : model::allDays()) {
        combo_dia_->addItem(QString::fromStdString(model::dayToLongString(d)), static_cast<int>(d));
    }
    if (aula_aloc) {
        int day_idx = static_cast<int>(aula_aloc->dia);
        combo_dia_->setCurrentIndex(day_idx);
    }
    makeComboResponsive(combo_dia_, 16, 180);
    form_destino->addRow("Novo Dia da Semana:", combo_dia_);

    combo_horario_ = new QComboBox(grp_destino);
    int selected_slot_idx = 0;
    int dur_aula = aula_model ? aula_model->duracaoMinutos() : (aula_aloc ? aula_aloc->intervalo.durationMinutes() : 0);
    for (size_t i = 0; i < available_slots_.size(); ++i) {
        const auto& slot = available_slots_[i];
        if (dur_aula > 0 && slot.durationMinutes() != dur_aula) {
            continue;
        }
        combo_horario_->addItem(QString("%1 (%2 min)").arg(QString::fromStdString(slot.toString())).arg(slot.durationMinutes()), static_cast<int>(i));
        if (aula_aloc && slot == aula_aloc->intervalo) {
            selected_slot_idx = combo_horario_->count() - 1;
        }
    }
    if (combo_horario_->count() == 0) {
        for (size_t i = 0; i < available_slots_.size(); ++i) {
            const auto& slot = available_slots_[i];
            combo_horario_->addItem(QString::fromStdString(slot.toString()), static_cast<int>(i));
            if (aula_aloc && slot.start_min == aula_aloc->intervalo.start_min) {
                selected_slot_idx = static_cast<int>(i);
            }
        }
    }
    if (combo_horario_->count() > 0) {
        combo_horario_->setCurrentIndex(selected_slot_idx);
    }
    makeComboResponsive(combo_horario_, 18, 180);
    form_destino->addRow("Novo Horário:", combo_horario_);

    combo_sala_ = new QComboBox(grp_destino);
    int selected_room_idx = 0;
    int r_counter = 0;
    for (const auto& room : data_.salas()) {
        combo_sala_->addItem(QString("%1 (%2, %3 lug.)")
            .arg(QString::fromStdString(room.nome()))
            .arg(QString::fromStdString(room.tipo()))
            .arg(room.capacidade()),
            QString::fromStdString(room.id()));
        if (aula_aloc && !aula_aloc->salas.empty() && aula_aloc->salas.front() == room.id()) {
            selected_room_idx = r_counter;
        }
        r_counter++;
    }
    if (combo_sala_->count() > 0) {
        combo_sala_->setCurrentIndex(selected_room_idx);
    }
    makeComboResponsive(combo_sala_, 20, 180);
    form_destino->addRow("Nova Sala / Lab:", combo_sala_);

    main_layout->addWidget(grp_destino);

    // Feedback de status e conflitos
    lbl_conflito_status_ = new QLabel(body_widget);
    lbl_conflito_status_->setWordWrap(true);
    lbl_conflito_status_->setStyleSheet("padding: 8px; border-radius: 6px; font-size: 11px;");
    main_layout->addWidget(lbl_conflito_status_);
    main_layout->addStretch();

    root_layout->addWidget(wrapInScrollArea(body_widget, this), 1);

    // Botões de Ação (fixos no rodapé)
    auto* btn_layout = new QHBoxLayout();
    btn_layout->addStretch();

    auto* btn_cancelar = new QPushButton("Cancelar", this);
    btn_cancelar->setObjectName("btnSecondary");
    connect(btn_cancelar, &QPushButton::clicked, this, &QDialog::reject);
    btn_layout->addWidget(btn_cancelar);

    btn_mover_ = new QPushButton(ModernTheme::icon("save_file"), "Salvar Alterações", this);
    btn_mover_->setStyleSheet("font-weight: bold; padding: 6px 16px;");
    btn_mover_->setDefault(true);
    connect(btn_mover_, &QPushButton::clicked, this, &QDialog::accept);
    btn_layout->addWidget(btn_mover_);

    root_layout->addLayout(btn_layout);

    connect(combo_dia_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MoveLessonDialog::onSelectionChanged);
    connect(combo_horario_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MoveLessonDialog::onSelectionChanged);
    connect(combo_sala_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MoveLessonDialog::onSelectionChanged);
}

/**
 * @brief Retorna o dia da semana selecionado.
 * @return Dia da semana (model::DayOfWeek).
 */
model::DayOfWeek MoveLessonDialog::selectedDay() const {
    return static_cast<model::DayOfWeek>(combo_dia_->currentData().toInt());
}

/**
 * @brief Retorna o intervalo horário selecionado.
 * @return Intervalo de tempo (model::TimeInterval).
 */
model::TimeInterval MoveLessonDialog::selectedInterval() const {
    int idx = combo_horario_->currentData().toInt();
    if (idx >= 0 && idx < static_cast<int>(available_slots_.size())) {
        return available_slots_[idx];
    }
    return model::TimeInterval();
}

/**
 * @brief Retorna a sala de aula selecionada.
 * @return ID da sala selecionada.
 */
std::string MoveLessonDialog::selectedRoomId() const {
    if (!combo_sala_ || combo_sala_->currentIndex() < 0) return {};
    return combo_sala_->currentData().toString().toStdString();
}

/**
 * @brief Slot disparado ao alterar o dia, horário ou sala para revalidar conflitos.
 */
void MoveLessonDialog::onSelectionChanged() {
    updateValidationStatus();
}

/**
 * @brief Executa validações de disponibilidade docente, de turma, de sala e intervalos de descanso.
 */
void MoveLessonDialog::updateValidationStatus() {
    auto target_day = selectedDay();
    auto target_inter = selectedInterval();

    const auto* a_aloc = data_.grade().findAula(aula_id_);
    if (!a_aloc) {
        lbl_conflito_status_->setText("Aula não encontrada na grade.");
        return;
    }

    std::string sel_room = selectedRoomId();
    bool same_day = (a_aloc->dia == target_day);
    bool same_time = (a_aloc->intervalo == target_inter);
    bool same_room = (!a_aloc->salas.empty() && a_aloc->salas.front() == sel_room);

    // Se for exatamente o mesmo horário e mesma sala
    if (same_day && same_time && same_room) {
        lbl_conflito_status_->setText("ℹ️ Horário e sala inalterados.");
        lbl_conflito_status_->setStyleSheet("background-color: rgba(148, 163, 184, 0.15); color: palette(window-text); border: 1px solid palette(mid);");
        return;
    }

    // Verificar compatibilidade exata com slot da(s) turma(s)
    for (const auto& t_id : a_aloc->turmas) {
        const auto* t = data_.findTurma(t_id);
        if (t) {
            auto j = t->jornadaParaDia(target_day);
            for (const auto& inter : j.intervalosAtivos()) {
                if (std::max(inter.start_min, target_inter.start_min) < std::min(inter.end_min, target_inter.end_min)) {
                    lbl_conflito_status_->setText("❌ Conflito: O horário selecionado sobrepõe o intervalo da turma " + QString::fromStdString(t->nome()) + ".");
                    lbl_conflito_status_->setStyleSheet("background-color: rgba(239, 68, 68, 0.15); color: #dc2626; border: 1px solid rgba(239, 68, 68, 0.4);");
                    return;
                }
            }
            if (!j.contemSlotExato(target_inter)) {
                lbl_conflito_status_->setText("❌ Conflito: O horário " + QString::fromStdString(target_inter.toString()) +
                                              " não corresponde a um slot cadastrado na turma " + QString::fromStdString(t->nome()) + ".");
                lbl_conflito_status_->setStyleSheet("background-color: rgba(239, 68, 68, 0.15); color: #dc2626; border: 1px solid rgba(239, 68, 68, 0.4);");
                return;
            }
        }
    }

    // Verificar se já existe outra aula no mesmo horário para a mesma turma
    bool has_target_lesson = false;
    QString target_lesson_name;
    for (const auto& a : data_.grade().alocacoes()) {
        if (a.aula_id != aula_id_ && a.dia == target_day &&
            std::max(a.intervalo.start_min, target_inter.start_min) < std::min(a.intervalo.end_min, target_inter.end_min)) {
            for (const auto& t_id : a_aloc->turmas) {
                if (std::find(a.turmas.begin(), a.turmas.end(), t_id) != a.turmas.end()) {
                    has_target_lesson = true;
                    target_lesson_name = QString::fromStdString(a.disciplina_nome);
                    break;
                }
            }
        }
    }

    if (has_target_lesson) {
        lbl_conflito_status_->setText(QString("ℹ️ O horário já possui a aula '%1'. Ao mover, será oferecida a troca de horários (Swap) entre as duas aulas.").arg(target_lesson_name));
        lbl_conflito_status_->setStyleSheet("background-color: rgba(245, 158, 11, 0.15); color: #d97706; border: 1px solid rgba(245, 158, 11, 0.4);");
        return;
    }

    // Verificar conflitos de agenda docente
    for (const auto& p_id : a_aloc->professores) {
        for (const auto& a : data_.grade().alocacoes()) {
            if (a.aula_id != aula_id_ && a.dia == target_day &&
                std::max(a.intervalo.start_min, target_inter.start_min) < std::min(a.intervalo.end_min, target_inter.end_min)) {
                if (std::find(a.professores.begin(), a.professores.end(), p_id) != a.professores.end()) {
                    const auto* p = data_.findProfessor(p_id);
                    lbl_conflito_status_->setText(QString("⚠️ Choque de Horário: O Prof. %1 já possui a aula '%2' neste mesmo horário.")
                        .arg(QString::fromStdString(p ? p->nome() : p_id))
                        .arg(QString::fromStdString(a.disciplina_nome)));
                    lbl_conflito_status_->setStyleSheet("background-color: rgba(239, 68, 68, 0.15); color: #dc2626; border: 1px solid rgba(239, 68, 68, 0.4);");
                    return;
                }
            }
        }
    }

    // Verificar conflitos de sala/laboratório
    if (!sel_room.empty()) {
        const auto* s = data_.findSala(sel_room);
        for (const auto& a : data_.grade().alocacoes()) {
            if (a.aula_id != aula_id_ && a.dia == target_day &&
                std::max(a.intervalo.start_min, target_inter.start_min) < std::min(a.intervalo.end_min, target_inter.end_min)) {
                if (std::find(a.salas.begin(), a.salas.end(), sel_room) != a.salas.end()) {
                    lbl_conflito_status_->setText(QString("⚠️ Choque de Sala: A sala '%1' já está ocupada pela aula '%2' neste mesmo horário.")
                        .arg(QString::fromStdString(s ? s->nome() : sel_room))
                        .arg(QString::fromStdString(a.disciplina_nome)));
                    lbl_conflito_status_->setStyleSheet("background-color: rgba(239, 68, 68, 0.15); color: #dc2626; border: 1px solid rgba(239, 68, 68, 0.4);");
                    return;
                }
            }
        }

        // Aviso pedagógico se aula for laboratório e sala for teórica
        if (a_aloc->tipo == "LABORATORIO" && s && s->isTeorica()) {
            lbl_conflito_status_->setText("⚠️ Atenção: A aula requer Laboratório, mas a sala selecionada é Teórica.");
            lbl_conflito_status_->setStyleSheet("background-color: rgba(245, 158, 11, 0.15); color: #d97706; border: 1px solid rgba(245, 158, 11, 0.4);");
            return;
        }
    }

    lbl_conflito_status_->setText("✅ Horário e sala livres, sem conflitos detectados para esta aula.");
    lbl_conflito_status_->setStyleSheet("background-color: rgba(22, 163, 74, 0.15); color: #16a34a; border: 1px solid rgba(22, 163, 74, 0.4);");
}

} // namespace scheduler::ui
