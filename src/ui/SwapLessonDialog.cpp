/**
 * @file SwapLessonDialog.cpp
 * @brief Implementação do diálogo de permuta (Swap) de horários entre duas aulas.
 */

#include "ui/SwapLessonDialog.h"
#include "ui/ModernTheme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>
#include <algorithm>

namespace scheduler::ui {

static QString removeAccents(QString str) {
    static const QString src = "ÁÀÃÂÉÈÊÍÌÎÓÒÕÔÚÙÛÇáàãâéèêíìîóòõôúùûç";
    static const QString dst = "AAAAEEEIIIOOOOUUUCAAAAEEEIIIOOOOUUUC";
    for (int i = 0; i < src.size(); ++i) {
        str.replace(src[i], dst[i]);
    }
    return str;
}

SwapLessonDialog::SwapLessonDialog(model::InstitutionData& data,
                                   const std::string& initial_aula_id,
                                   QWidget* parent)
    : QDialog(parent),
      data_(data),
      initial_aula_id_(initial_aula_id),
      selected_origem_id_(initial_aula_id) {

    setWindowTitle("Trocar Horários de Aulas (Swap)");
    setWindowIcon(ModernTheme::icon("swap"));
    resize(780, 620);
    setMinimumSize(680, 520);

    setupUi();
    populateOrigemCombo();
    populateDestinoTable();
    updateValidationStatus();
}

void SwapLessonDialog::setupUi() {
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(12);
    main_layout->setContentsMargins(16, 16, 16, 16);

    // 1. Seção da Aula de Origem
    auto* grp_origem = new QGroupBox("Aula de Origem (A mover para novo horário)", this);
    auto* layout_origem = new QVBoxLayout(grp_origem);
    layout_origem->setSpacing(8);

    combo_origem_ = new QComboBox(grp_origem);
    combo_origem_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout_origem->addWidget(combo_origem_);

    lbl_origem_details_ = new QLabel(grp_origem);
    lbl_origem_details_->setStyleSheet("font-size: 12px; font-weight: 500; padding: 4px;");
    lbl_origem_details_->setWordWrap(true);
    layout_origem->addWidget(lbl_origem_details_);

    main_layout->addWidget(grp_origem);

    // 2. Seção da Aula de Destino
    auto* grp_destino = new QGroupBox("Selecione a Aula de Destino para Permutar Horários", this);
    auto* layout_destino = new QVBoxLayout(grp_destino);
    layout_destino->setSpacing(8);

    auto* filter_bar = new QHBoxLayout();
    search_destino_ = new QLineEdit(grp_destino);
    search_destino_->addAction(ModernTheme::icon("search"), QLineEdit::LeadingPosition);
    search_destino_->setPlaceholderText("Filtrar por disciplina, professor, turma ou dia...");
    search_destino_->setClearButtonEnabled(true);
    filter_bar->addWidget(search_destino_);
    layout_destino->addLayout(filter_bar);

    table_destino_ = new QTableWidget(grp_destino);
    table_destino_->setColumnCount(6);
    table_destino_->setHorizontalHeaderLabels({"Dia", "Horário", "Turma", "Disciplina", "Professor(es)", "Sala(s)"});
    table_destino_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_destino_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_destino_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    table_destino_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    table_destino_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table_destino_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    table_destino_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_destino_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_destino_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_destino_->verticalHeader()->setVisible(false);
    layout_destino->addWidget(table_destino_);

    main_layout->addWidget(grp_destino, 1);

    // 3. Painel de Validação em Tempo Real
    lbl_validation_status_ = new QLabel(this);
    lbl_validation_status_->setWordWrap(true);
    lbl_validation_status_->setStyleSheet("padding: 8px 12px; border-radius: 6px; font-size: 12px; font-weight: 500;");
    main_layout->addWidget(lbl_validation_status_);

    // 4. Rodapé e Botões
    auto* bottom_bar = new QHBoxLayout();
    chk_pinned_ = new QCheckBox("Fixar horários das aulas após a troca (Pin 📌)", this);
    chk_pinned_->setChecked(true);
    chk_pinned_->setToolTip("Bloqueia ambas as aulas para que não sejam alteradas pelo otimizador automático.");
    bottom_bar->addWidget(chk_pinned_);

    bottom_bar->addStretch();

    btn_cancel_ = new QPushButton("Cancelar", this);
    btn_cancel_->setCursor(Qt::PointingHandCursor);
    bottom_bar->addWidget(btn_cancel_);

    btn_confirm_ = new QPushButton(ModernTheme::icon("swap"), "Confirmar Troca (Swap)", this);
    btn_confirm_->setObjectName("btnPrimary");
    btn_confirm_->setCursor(Qt::PointingHandCursor);
    btn_confirm_->setEnabled(false);
    bottom_bar->addWidget(btn_confirm_);

    main_layout->addLayout(bottom_bar);

    // Conexões
    connect(combo_origem_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SwapLessonDialog::onOrigemChanged);
    connect(search_destino_, &QLineEdit::textChanged,
            this, &SwapLessonDialog::onFilterDestinoChanged);
    connect(table_destino_, &QTableWidget::itemSelectionChanged,
            this, &SwapLessonDialog::onDestinoSelectionChanged);
    connect(table_destino_, &QTableWidget::cellDoubleClicked,
            this, [this](int, int) {
                if (btn_confirm_->isEnabled()) {
                    onConfirmSwap();
                }
            });
    connect(btn_cancel_, &QPushButton::clicked, this, &QDialog::reject);
    connect(btn_confirm_, &QPushButton::clicked, this, &SwapLessonDialog::onConfirmSwap);
}

void SwapLessonDialog::populateOrigemCombo() {
    combo_origem_->blockSignals(true);
    combo_origem_->clear();

    int target_idx = 0;
    int idx = 0;
    for (const auto& a : data_.grade().alocacoes()) {
        QString t_name;
        if (!a.turmas.empty()) {
            const auto* t = data_.findTurma(a.turmas.front());
            t_name = QString::fromStdString(t ? t->nome() : a.turmas.front());
        }
        QString p_name;
        if (!a.professores.empty()) {
            const auto* p = data_.findProfessor(a.professores.front());
            p_name = QString::fromStdString(p ? p->nome() : a.professores.front());
        }

        QString label = QString("[%1] %2 — Prof. %3 (%4 às %5)")
            .arg(t_name.isEmpty() ? "Sem Turma" : t_name)
            .arg(QString::fromStdString(a.disciplina_nome))
            .arg(p_name.isEmpty() ? "Sem Prof" : p_name)
            .arg(QString::fromStdString(a.diaString()))
            .arg(QString::fromStdString(a.horarioFormatado()));

        combo_origem_->addItem(label, QString::fromStdString(a.aula_id));
        if (a.aula_id == initial_aula_id_) {
            target_idx = idx;
        }
        idx++;
    }

    if (combo_origem_->count() > 0) {
        combo_origem_->setCurrentIndex(target_idx);
        selected_origem_id_ = combo_origem_->currentData().toString().toStdString();
    }
    combo_origem_->blockSignals(false);
    onOrigemChanged();
}

void SwapLessonDialog::onOrigemChanged() {
    selected_origem_id_ = combo_origem_->currentData().toString().toStdString();
    const auto* a_aloc = data_.grade().findAula(selected_origem_id_);
    if (a_aloc) {
        QString t_names;
        for (const auto& tid : a_aloc->turmas) {
            const auto* t = data_.findTurma(tid);
            t_names += (t_names.isEmpty() ? "" : ", ") + QString::fromStdString(t ? t->nome() : tid);
        }
        QString p_names;
        for (const auto& pid : a_aloc->professores) {
            const auto* p = data_.findProfessor(pid);
            p_names += (p_names.isEmpty() ? "" : ", ") + QString::fromStdString(p ? p->nome() : pid);
        }
        QString s_names;
        for (const auto& sid : a_aloc->salas) {
            const auto* s = data_.findSala(sid);
            s_names += (s_names.isEmpty() ? "" : ", ") + QString::fromStdString(s ? s->nome() : sid);
        }

        lbl_origem_details_->setText(
            QString("<b>Horário Atual:</b> %1 às %2 | <b>Turma(s):</b> %3 | <b>Professor(es):</b> %4 | <b>Sala(s):</b> %5")
                .arg(QString::fromStdString(a_aloc->diaString()),
                     QString::fromStdString(a_aloc->horarioFormatado()),
                     t_names.isEmpty() ? "—" : t_names,
                     p_names.isEmpty() ? "—" : p_names,
                     s_names.isEmpty() ? "—" : s_names));
    } else {
        lbl_origem_details_->setText("Nenhuma aula de origem selecionada.");
    }

    populateDestinoTable();
    updateValidationStatus();
}

void SwapLessonDialog::populateDestinoTable() {
    table_destino_->setRowCount(0);
    selected_destino_id_.clear();

    const auto* a_origem = data_.grade().findAula(selected_origem_id_);
    if (!a_origem) return;

    QString clean = removeAccents(search_destino_->text()).trimmed().toLower();

    int row = 0;
    for (const auto& a : data_.grade().alocacoes()) {
        if (a.aula_id == selected_origem_id_) continue;

        QString dia_str = QString::fromStdString(a.diaString());
        QString hor_str = QString::fromStdString(a.horarioFormatado());
        QString disc_str = QString::fromStdString(a.disciplina_nome);

        QString t_names;
        for (const auto& tid : a.turmas) {
            const auto* t = data_.findTurma(tid);
            t_names += (t_names.isEmpty() ? "" : ", ") + QString::fromStdString(t ? t->nome() : tid);
        }
        QString p_names;
        for (const auto& pid : a.professores) {
            const auto* p = data_.findProfessor(pid);
            p_names += (p_names.isEmpty() ? "" : ", ") + QString::fromStdString(p ? p->nome() : pid);
        }
        QString s_names;
        for (const auto& sid : a.salas) {
            const auto* s = data_.findSala(sid);
            s_names += (s_names.isEmpty() ? "" : ", ") + QString::fromStdString(s ? s->nome() : sid);
        }

        // Filtro de busca
        if (!clean.isEmpty()) {
            QString full_str = removeAccents(QString("%1 %2 %3 %4 %5 %6").arg(dia_str, hor_str, t_names, disc_str, p_names, s_names)).toLower();
            if (!full_str.contains(clean)) continue;
        }

        table_destino_->insertRow(row);

        auto* item_dia = new QTableWidgetItem(dia_str);
        item_dia->setData(Qt::UserRole, QString::fromStdString(a.aula_id));
        table_destino_->setItem(row, 0, item_dia);

        auto* item_hor = new QTableWidgetItem(hor_str);
        item_hor->setData(Qt::UserRole, QString::fromStdString(a.aula_id));
        table_destino_->setItem(row, 1, item_hor);

        auto* item_tur = new QTableWidgetItem(t_names);
        item_tur->setData(Qt::UserRole, QString::fromStdString(a.aula_id));
        table_destino_->setItem(row, 2, item_tur);

        auto* item_disc = new QTableWidgetItem(disc_str);
        item_disc->setData(Qt::UserRole, QString::fromStdString(a.aula_id));
        table_destino_->setItem(row, 3, item_disc);

        auto* item_prof = new QTableWidgetItem(p_names);
        item_prof->setData(Qt::UserRole, QString::fromStdString(a.aula_id));
        table_destino_->setItem(row, 4, item_prof);

        auto* item_sal = new QTableWidgetItem(s_names);
        item_sal->setData(Qt::UserRole, QString::fromStdString(a.aula_id));
        table_destino_->setItem(row, 5, item_sal);

        row++;
    }

    btn_confirm_->setEnabled(false);
}

void SwapLessonDialog::onFilterDestinoChanged(const QString& /*query*/) {
    populateDestinoTable();
}

void SwapLessonDialog::onDestinoSelectionChanged() {
    int cur_row = table_destino_->currentRow();
    if (cur_row >= 0 && cur_row < table_destino_->rowCount()) {
        auto* item = table_destino_->item(cur_row, 0);
        if (item) {
            selected_destino_id_ = item->data(Qt::UserRole).toString().toStdString();
        }
    } else {
        selected_destino_id_.clear();
    }

    updateValidationStatus();
}

std::vector<std::string> SwapLessonDialog::verificarConflitosTroca(
    const std::string& aid_a, const std::string& aid_b) const {

    std::vector<std::string> conflitos;
    const auto* a = data_.grade().findAula(aid_a);
    const auto* b = data_.grade().findAula(aid_b);
    if (!a || !b) return conflitos;

    auto time_overlaps = [](const model::TimeInterval& t1, const model::TimeInterval& t2) {
        return std::max(t1.start_slot, t2.start_slot) < std::min(t1.end_slot, t2.end_slot);
    };

    auto semestres_conflitam = [](const model::AllocatedLesson& l1, const model::AllocatedLesson& l2) {
        if (l1.regime == model::RegimeDisciplina::ANUAL || l2.regime == model::RegimeDisciplina::ANUAL) return true;
        int s1 = (l1.semestre == model::SemestreLetivo::SEMESTRE_2 || l1.semestre_alocado == 2) ? 2 : 1;
        int s2 = (l2.semestre == model::SemestreLetivo::SEMESTRE_2 || l2.semestre_alocado == 2) ? 2 : 1;
        return s1 == s2;
    };

    // A vai para dia_b e intervalo_b
    auto dia_novo_a = b->dia;
    auto inter_novo_a = model::TimeInterval(b->intervalo.start_slot, b->intervalo.start_slot + a->intervalo.durationSlots());

    // B vai para dia_a e intervalo_a
    auto dia_novo_b = a->dia;
    auto inter_novo_b = model::TimeInterval(a->intervalo.start_slot, a->intervalo.start_slot + b->intervalo.durationSlots());

    // 1. Validar A no novo horário (excluindo B)
    for (const auto& t_id : a->turmas) {
        const auto* t = data_.findTurma(t_id);
        if (t && !t->funcionaNoDia(dia_novo_a)) {
            conflitos.push_back("Turma " + t->nome() + " (Aula " + a->disciplina_nome + ") não funciona na " + model::dayToLongString(dia_novo_a) + ".");
        }
        for (const auto& other : data_.grade().alocacoes()) {
            if (other.aula_id != aid_a && other.aula_id != aid_b && other.dia == dia_novo_a &&
                semestres_conflitam(*a, other) && time_overlaps(other.intervalo, inter_novo_a)) {
                if (std::find(other.turmas.begin(), other.turmas.end(), t_id) != other.turmas.end()) {
                    conflitos.push_back("Choque de turma: Turma " + (t ? t->nome() : t_id) + " já tem a aula '" + other.disciplina_nome + "' neste horário.");
                }
            }
        }
    }
    for (const auto& p_id : a->professores) {
        const auto* p = data_.findProfessor(p_id);
        if (p && !p->isDisponivelNoIntervalo(dia_novo_a, inter_novo_a.start_slot, inter_novo_a.end_slot)) {
            conflitos.push_back("Indisponibilidade: Prof. " + (p ? p->nome() : p_id) + " não está disponível na " + model::dayToLongString(dia_novo_a) + " às " + inter_novo_a.toString() + ".");
        }
        for (const auto& other : data_.grade().alocacoes()) {
            if (other.aula_id != aid_a && other.aula_id != aid_b && other.dia == dia_novo_a &&
                semestres_conflitam(*a, other) && time_overlaps(other.intervalo, inter_novo_a)) {
                if (std::find(other.professores.begin(), other.professores.end(), p_id) != other.professores.end()) {
                    conflitos.push_back("Choque de docente: Prof. " + (p ? p->nome() : p_id) + " já tem a aula '" + other.disciplina_nome + "' neste horário.");
                }
            }
        }
    }
    for (const auto& s_id : a->salas) {
        const auto* s = data_.findSala(s_id);
        for (const auto& other : data_.grade().alocacoes()) {
            if (other.aula_id != aid_a && other.aula_id != aid_b && other.dia == dia_novo_a &&
                semestres_conflitam(*a, other) && time_overlaps(other.intervalo, inter_novo_a)) {
                if (std::find(other.salas.begin(), other.salas.end(), s_id) != other.salas.end()) {
                    conflitos.push_back("Choque de sala: Sala " + (s ? s->nome() : s_id) + " já está ocupada pela aula '" + other.disciplina_nome + "' neste horário.");
                }
            }
        }
    }

    // 2. Validar B no novo horário (excluindo A)
    for (const auto& t_id : b->turmas) {
        const auto* t = data_.findTurma(t_id);
        if (t && !t->funcionaNoDia(dia_novo_b)) {
            conflitos.push_back("Turma " + t->nome() + " (Aula " + b->disciplina_nome + ") não funciona na " + model::dayToLongString(dia_novo_b) + ".");
        }
        for (const auto& other : data_.grade().alocacoes()) {
            if (other.aula_id != aid_a && other.aula_id != aid_b && other.dia == dia_novo_b &&
                semestres_conflitam(*b, other) && time_overlaps(other.intervalo, inter_novo_b)) {
                if (std::find(other.turmas.begin(), other.turmas.end(), t_id) != other.turmas.end()) {
                    conflitos.push_back("Choque de turma: Turma " + (t ? t->nome() : t_id) + " já tem a aula '" + other.disciplina_nome + "' neste horário.");
                }
            }
        }
    }
    for (const auto& p_id : b->professores) {
        const auto* p = data_.findProfessor(p_id);
        if (p && !p->isDisponivelNoIntervalo(dia_novo_b, inter_novo_b.start_slot, inter_novo_b.end_slot)) {
            conflitos.push_back("Indisponibilidade: Prof. " + (p ? p->nome() : p_id) + " não está disponível na " + model::dayToLongString(dia_novo_b) + " às " + inter_novo_b.toString() + ".");
        }
        for (const auto& other : data_.grade().alocacoes()) {
            if (other.aula_id != aid_a && other.aula_id != aid_b && other.dia == dia_novo_b &&
                semestres_conflitam(*b, other) && time_overlaps(other.intervalo, inter_novo_b)) {
                if (std::find(other.professores.begin(), other.professores.end(), p_id) != other.professores.end()) {
                    conflitos.push_back("Choque de docente: Prof. " + (p ? p->nome() : p_id) + " já tem a aula '" + other.disciplina_nome + "' neste horário.");
                }
            }
        }
    }
    for (const auto& s_id : b->salas) {
        const auto* s = data_.findSala(s_id);
        for (const auto& other : data_.grade().alocacoes()) {
            if (other.aula_id != aid_a && other.aula_id != aid_b && other.dia == dia_novo_b &&
                semestres_conflitam(*b, other) && time_overlaps(other.intervalo, inter_novo_b)) {
                if (std::find(other.salas.begin(), other.salas.end(), s_id) != other.salas.end()) {
                    conflitos.push_back("Choque de sala: Sala " + (s ? s->nome() : s_id) + " já está ocupada pela aula '" + other.disciplina_nome + "' neste horário.");
                }
            }
        }
    }

    return conflitos;
}

void SwapLessonDialog::updateValidationStatus() {
    if (selected_destino_id_.empty() || selected_origem_id_.empty()) {
        lbl_validation_status_->setText("Selecione uma aula de destino na tabela acima para avaliar a troca de horários.");
        lbl_validation_status_->setStyleSheet("background-color: palette(base); color: palette(window-text); border: 1px solid palette(mid);");
        btn_confirm_->setEnabled(false);
        return;
    }

    auto conflitos = verificarConflitosTroca(selected_origem_id_, selected_destino_id_);
    bool is_dark = ModernTheme::isDarkTheme();

    if (conflitos.empty()) {
        lbl_validation_status_->setText("✅ <b>Troca Perfeita:</b> Não foram detectados conflitos de horários, turmas, professores ou salas!");
        lbl_validation_status_->setStyleSheet(
            is_dark ? "background-color: #064e3b; color: #6ee7b7; border: 1px solid #059669;"
                    : "background-color: #d1fae5; color: #065f46; border: 1px solid #10b981;");
    } else {
        QString msg = QString("⚠️ <b>Atenção:</b> Foram detectados %1 conflito(s) ao permutar as aulas:<br>").arg(conflitos.size());
        for (size_t i = 0; i < std::min<size_t>(conflitos.size(), 3); ++i) {
            msg += "• " + QString::fromStdString(conflitos[i]) + "<br>";
        }
        if (conflitos.size() > 3) {
            msg += QString("... e mais %1 outro(s).").arg(conflitos.size() - 3);
        }
        lbl_validation_status_->setText(msg);
        lbl_validation_status_->setStyleSheet(
            is_dark ? "background-color: #451a03; color: #fde047; border: 1px solid #d97706;"
                    : "background-color: #fef3c7; color: #92400e; border: 1px solid #f59e0b;");
    }

    btn_confirm_->setEnabled(true);
}

void SwapLessonDialog::onConfirmSwap() {
    if (selected_origem_id_.empty() || selected_destino_id_.empty()) return;

    auto conflitos = verificarConflitosTroca(selected_origem_id_, selected_destino_id_);
    if (!conflitos.empty()) {
        QString msg = QString("Foram detectados %1 conflito(s) ao permutar estas duas aulas:\n\n").arg(conflitos.size());
        for (size_t i = 0; i < std::min<size_t>(conflitos.size(), 6); ++i) {
            msg += "• " + QString::fromStdString(conflitos[i]) + "\n";
        }
        if (conflitos.size() > 6) {
            msg += QString("... e mais %1 outro(s).\n").arg(conflitos.size() - 6);
        }
        msg += "\n⚠️ As aulas que gerarem conflito serão destacadas em cor VERMELHA na grade horária.\n\nDeseja confirmar a troca mesmo com os conflitos informados?";
        auto resp = QMessageBox::question(this, "Permissão: Troca com Conflitos", msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (resp != QMessageBox::Yes) return;
    }

    accept();
}

std::string SwapLessonDialog::aulaOrigemId() const {
    return selected_origem_id_;
}

std::string SwapLessonDialog::aulaDestinoId() const {
    return selected_destino_id_;
}

bool SwapLessonDialog::isPinned() const {
    return chk_pinned_ && chk_pinned_->isChecked();
}

} // namespace scheduler::ui
