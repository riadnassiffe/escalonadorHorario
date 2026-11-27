/**
 * @file ChangeRoomDialog.cpp
 * @brief Implementação do diálogo de alteração e permuta de salas de aula.
 */

#include "ui/ChangeRoomDialog.h"
#include "ui/ModernTheme.h"
#include "ui/ResponsiveUi.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <algorithm>

namespace scheduler::ui {

/**
 * @brief Construtor do diálogo de alteração de sala.
 * @param data Referência para os dados institucionais.
 * @param aula_id ID da aula alocada.
 * @param parent Widget pai na hierarquia Qt.
 */
ChangeRoomDialog::ChangeRoomDialog(model::InstitutionData& data,
                                   const std::string& aula_id,
                                   QWidget* parent)
    : QDialog(parent), data_(data), aula_id_(aula_id) {
    setupUi();
}

/**
 * @brief Inicializa e posiciona os componentes visuais da janela.
 */
void ChangeRoomDialog::setupUi() {
    setWindowTitle("Alterar Sala de Aula");
    setWindowIcon(ModernTheme::icon("room"));
    setMinimumSize(440, 340);
    setModal(true);

    const auto* a_aloc = data_.grade().findAula(aula_id_);
    if (!a_aloc) return;

    auto* root_lay = new QVBoxLayout(this);
    root_lay->setSpacing(10);
    root_lay->setContentsMargins(12, 12, 12, 12);

    auto* body_widget = new QWidget(this);
    auto* main_lay = new QVBoxLayout(body_widget);
    main_lay->setSpacing(12);
    main_lay->setContentsMargins(2, 2, 2, 2);

    // 1. Card de informações da aula
    auto* card_aula = new QFrame(body_widget);
    card_aula->setObjectName("cardAula");
    bool is_dark = ModernTheme::isDarkTheme();
    card_aula->setStyleSheet(QString(
        "QFrame#cardAula {"
        "  background-color: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 8px;"
        "}"
    ).arg(is_dark ? "#1e293b" : "#f8fafc", is_dark ? "#334155" : "#e2e8f0"));

    auto* card_lay = new QVBoxLayout(card_aula);
    card_lay->setSpacing(6);
    card_lay->setContentsMargins(10, 10, 10, 10);

    // Disciplina e Tipo
    auto* lbl_titulo = new QLabel(QString("<b>%1</b> <span style='color:%2;'>[%3]</span>")
        .arg(QString::fromStdString(a_aloc->disciplina_nome))
        .arg(a_aloc->tipo == "LABORATORIO" ? "#ec4899" : (a_aloc->tipo == "ESPECIAL" ? "#8b5cf6" : "#3b82f6"))
        .arg(QString::fromStdString(a_aloc->tipo)), card_aula);
    lbl_titulo->setWordWrap(true);
    QFont f_title = lbl_titulo->font();
    f_title.setPointSize(12);
    lbl_titulo->setFont(f_title);
    card_lay->addWidget(lbl_titulo);

    // Horário e Dia
    auto* lbl_horario = new QLabel(QString("📅 <b>%1</b> das <b>%2</b> (%3 min)")
        .arg(QString::fromStdString(model::dayToLongString(a_aloc->dia)))
        .arg(QString::fromStdString(a_aloc->horarioFormatado()))
        .arg(a_aloc->intervalo.durationMinutes()), card_aula);
    lbl_horario->setWordWrap(true);
    card_lay->addWidget(lbl_horario);

    // Turmas e Professores
    QString turmas_str, profs_str;
    for (size_t i = 0; i < a_aloc->turmas.size(); ++i) {
        if (i > 0) turmas_str += ", ";
        const auto* t = data_.findTurma(a_aloc->turmas[i]);
        turmas_str += QString::fromStdString(t ? t->nome() : a_aloc->turmas[i]);
    }
    for (size_t i = 0; i < a_aloc->professores.size(); ++i) {
        if (i > 0) profs_str += ", ";
        const auto* p = data_.findProfessor(a_aloc->professores[i]);
        profs_str += QString::fromStdString(p ? p->nome() : a_aloc->professores[i]);
    }
    auto* lbl_turma_prof = new QLabel(QString("👥 <b>Turma:</b> %1 &nbsp;&nbsp;|&nbsp;&nbsp; 👨‍🏫 <b>Prof:</b> %2")
        .arg(turmas_str, profs_str), card_aula);
    lbl_turma_prof->setWordWrap(true);
    card_lay->addWidget(lbl_turma_prof);

    // Sala atual
    std::string sala_atual_desc = "Nenhuma sala alocada";
    if (!a_aloc->salas.empty()) {
        const auto* s = data_.findSala(a_aloc->salas.front());
        if (s) {
            sala_atual_desc = s->nome() + " (" + s->tipo() + ", " + std::to_string(s->capacidade()) + " lugares)";
        } else {
            sala_atual_desc = a_aloc->salas.front();
        }
    }
    auto* lbl_sala_atual = new QLabel(QString("📍 <b>Sala Atual:</b> <span style='color: %1;'>%2</span>")
        .arg(is_dark ? "#38bdf8" : "#0284c7")
        .arg(QString::fromStdString(sala_atual_desc)), card_aula);
    lbl_sala_atual->setWordWrap(true);
    card_lay->addWidget(lbl_sala_atual);

    main_lay->addWidget(card_aula);

    // 2. Seção de Seleção da Nova Sala
    auto* gb_select = new QGroupBox("Selecionar Nova Sala / Laboratório", body_widget);
    auto* gb_lay = new QVBoxLayout(gb_select);
    gb_lay->setSpacing(10);

    combo_salas_ = new QComboBox(gb_select);
    combo_salas_->setMinimumHeight(32);
    makeComboResponsive(combo_salas_, 24, 220);

    // Lambda para testar sobreposição de semestre e tempo
    auto time_overlaps = [](const model::TimeInterval& t1, const model::TimeInterval& t2) {
        return std::max(t1.start_min, t2.start_min) < std::min(t1.end_min, t2.end_min);
    };
    auto semestres_conflitam = [](const model::AllocatedLesson& a1, const model::AllocatedLesson& a2) {
        if (a1.regime == model::RegimeDisciplina::ANUAL || a2.regime == model::RegimeDisciplina::ANUAL) return true;
        int s1 = (a1.semestre == model::SemestreLetivo::SEMESTRE_2 || a1.semestre_alocado == 2) ? 2 : 1;
        int s2 = (a2.semestre == model::SemestreLetivo::SEMESTRE_2 || a2.semestre_alocado == 2) ? 2 : 1;
        return s1 == s2;
    };

    int current_idx = -1;
    int idx_counter = 0;

    for (const auto& sala : data_.salas()) {
        // Verificar se sala está ocupada no mesmo horário
        bool ocupada = false;
        std::string ocupante_desc;

        for (const auto& a : data_.grade().alocacoes()) {
            if (a.aula_id != aula_id_ && a.dia == a_aloc->dia &&
                semestres_conflitam(*a_aloc, a) && time_overlaps(a.intervalo, a_aloc->intervalo)) {
                if (std::find(a.salas.begin(), a.salas.end(), sala.id()) != a.salas.end()) {
                    ocupada = true;
                    ocupante_desc = a.disciplina_nome;
                    break;
                }
            }
        }

        QString item_text;
        if (ocupada) {
            item_text = QString("🔴 %1 (%2, %3 lug.) — [Ocupada por: %4]")
                .arg(QString::fromStdString(sala.nome()))
                .arg(QString::fromStdString(sala.tipo()))
                .arg(sala.capacidade())
                .arg(QString::fromStdString(ocupante_desc));
        } else {
            item_text = QString("🟢 %1 (%2, %3 lug.) — Livre")
                .arg(QString::fromStdString(sala.nome()))
                .arg(QString::fromStdString(sala.tipo()))
                .arg(sala.capacidade());
        }

        combo_salas_->addItem(item_text, QString::fromStdString(sala.id()));

        if (!a_aloc->salas.empty() && a_aloc->salas.front() == sala.id()) {
            current_idx = idx_counter;
        }
        idx_counter++;
    }

    if (current_idx >= 0) {
        combo_salas_->setCurrentIndex(current_idx);
    }

    gb_lay->addWidget(combo_salas_);

    // Status da sala selecionada
    lbl_status_ = new QLabel(gb_select);
    lbl_status_->setWordWrap(true);
    lbl_status_->setMinimumHeight(40);
    lbl_status_->setStyleSheet("padding: 8px; border-radius: 6px; font-size: 11px;");
    gb_lay->addWidget(lbl_status_);

    main_lay->addWidget(gb_select);

    // 3. Opções
    chk_pin_ = new QCheckBox("Fixar esta sala para a aula (preservar em futuras reotimizações)", body_widget);
    chk_pin_->setIcon(ModernTheme::icon("pin"));
    chk_pin_->setChecked(true);
    main_lay->addWidget(chk_pin_);
    main_lay->addStretch();

    root_lay->addWidget(wrapInScrollArea(body_widget, this), 1);

    // 4. Botões de ação (fixos no rodapé)
    auto* btn_lay = new QHBoxLayout();
    btn_lay->addStretch();

    btn_swap_ = new QPushButton(ModernTheme::icon("swap"), "Trocar Salas (Swap)", this);
    btn_swap_->setObjectName("btnSecondary");
    btn_swap_->setVisible(false);
    btn_lay->addWidget(btn_swap_);

    auto* btn_cancel = new QPushButton("Cancelar", this);
    btn_cancel->setObjectName("btnSecondary");
    btn_lay->addWidget(btn_cancel);

    btn_confirm_ = new QPushButton("Confirmar Alteração", this);
    btn_confirm_->setDefault(true);
    btn_lay->addWidget(btn_confirm_);

    root_lay->addLayout(btn_lay);

    connect(combo_salas_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChangeRoomDialog::onRoomChanged);
    connect(btn_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btn_confirm_, &QPushButton::clicked, this, &ChangeRoomDialog::onConfirmClicked);
    connect(btn_swap_, &QPushButton::clicked, this, &ChangeRoomDialog::onSwapClicked);

    updateRoomStatus();
    fitWindowToScreen(this, QSize(520, 460));
}

/**
 * @brief Slot acionado na seleção de outra sala no combo box.
 * @param index Índice da opção selecionada.
 */
void ChangeRoomDialog::onRoomChanged(int) {
    updateRoomStatus();
}

/**
 * @brief Atualiza a análise de disponibilidade da sala selecionada e reflete no painel de status.
 */
void ChangeRoomDialog::updateRoomStatus() {
    conflicting_aula_id_.clear();
    btn_swap_->setVisible(false);

    if (combo_salas_->count() == 0) {
        lbl_status_->setText("Nenhuma sala cadastrada no sistema.");
        btn_confirm_->setEnabled(false);
        return;
    }

    std::string selected_id = selectedRoomId();
    const auto* a_aloc = data_.grade().findAula(aula_id_);
    const auto* s_new = data_.findSala(selected_id);
    if (!a_aloc || !s_new) return;

    // Se for a mesma sala já alocada
    if (!a_aloc->salas.empty() && a_aloc->salas.front() == selected_id) {
        lbl_status_->setText("ℹ️ Esta já é a sala atualmente atribuída a esta aula.");
        lbl_status_->setStyleSheet("background-color: rgba(148, 163, 184, 0.15); color: palette(window-text); border: 1px solid palette(mid);");
        btn_confirm_->setEnabled(true);
        return;
    }

    auto time_overlaps = [](const model::TimeInterval& t1, const model::TimeInterval& t2) {
        return std::max(t1.start_min, t2.start_min) < std::min(t1.end_min, t2.end_min);
    };
    auto semestres_conflitam = [](const model::AllocatedLesson& a1, const model::AllocatedLesson& a2) {
        if (a1.regime == model::RegimeDisciplina::ANUAL || a2.regime == model::RegimeDisciplina::ANUAL) return true;
        int s1 = (a1.semestre == model::SemestreLetivo::SEMESTRE_2 || a1.semestre_alocado == 2) ? 2 : 1;
        int s2 = (a2.semestre == model::SemestreLetivo::SEMESTRE_2 || a2.semestre_alocado == 2) ? 2 : 1;
        return s1 == s2;
    };

    // Verificar se existe conflito
    const model::AllocatedLesson* conflicting_aula = nullptr;
    for (const auto& a : data_.grade().alocacoes()) {
        if (a.aula_id != aula_id_ && a.dia == a_aloc->dia &&
            semestres_conflitam(*a_aloc, a) && time_overlaps(a.intervalo, a_aloc->intervalo)) {
            if (std::find(a.salas.begin(), a.salas.end(), selected_id) != a.salas.end()) {
                conflicting_aula = &a;
                break;
            }
        }
    }

    if (conflicting_aula) {
        conflicting_aula_id_ = conflicting_aula->aula_id;
        btn_swap_->setVisible(true);

        QString conflito_turmas;
        for (size_t i = 0; i < conflicting_aula->turmas.size(); ++i) {
            if (i > 0) conflito_turmas += ", ";
            const auto* t = data_.findTurma(conflicting_aula->turmas[i]);
            conflito_turmas += QString::fromStdString(t ? t->nome() : conflicting_aula->turmas[i]);
        }

        lbl_status_->setText(QString("⚠️ <b>Sala Ocupada:</b> Já alocada para a aula <b>'%1'</b> (Turma: %2).<br>"
                                     "Você pode usar o botão <b>🔄 Trocar Salas</b> para inverter as salas das duas aulas.")
            .arg(QString::fromStdString(conflicting_aula->disciplina_nome), conflito_turmas));
        lbl_status_->setStyleSheet("background-color: rgba(239, 68, 68, 0.15); color: #dc2626; border: 1px solid rgba(239, 68, 68, 0.4);");
        btn_confirm_->setText("Alocar Mesma Sala");
        return;
    }

    btn_confirm_->setText("Confirmar Alteração");

    // Verificar incompatibilidade de tipo (ex: Laboratório em Sala Teórica)
    QString warning_extra;
    if (a_aloc->tipo == "LABORATORIO" && s_new->isTeorica()) {
        warning_extra = "<br>⚠️ <i>Atenção pedagógica: Esta aula requer Laboratório, mas a sala selecionada é Teórica.</i>";
    }

    lbl_status_->setText(QString("✅ <b>Sala Livre</b> para o horário da aula. Capacidade: <b>%1</b> alunos. Tipo: <b>%2</b>.%3")
        .arg(s_new->capacidade())
        .arg(QString::fromStdString(s_new->tipo()))
        .arg(warning_extra));
    lbl_status_->setStyleSheet("background-color: rgba(22, 163, 74, 0.15); color: #16a34a; border: 1px solid rgba(22, 163, 74, 0.4);");
}

/**
 * @brief Retorna o identificador da sala selecionada.
 * @return String com o ID da sala.
 */
std::string ChangeRoomDialog::selectedRoomId() const {
    if (!combo_salas_ || combo_salas_->currentIndex() < 0) return {};
    return combo_salas_->currentData().toString().toStdString();
}

/**
 * @brief Informa se a alocação deve ser fixada permanentemente na grade.
 * @return true se fixado (pinned); false caso contrário.
 */
bool ChangeRoomDialog::isPinned() const {
    return chk_pin_ && chk_pin_->isChecked();
}

/**
 * @brief Valida confirmação de alteração, exibindo aviso se a sala estiver compartilhada/conflitante.
 */
void ChangeRoomDialog::onConfirmClicked() {
    if (!conflicting_aula_id_.empty()) {
        auto resp = QMessageBox::question(this, "Permissão: Conflito de Sala",
            "A sala selecionada já está ocupada por outra aula neste horário.\n\n"
            "⚠️ Ambas as aulas que geraram conflito serão destacadas em cor VERMELHA na grade horária.\n\n"
            "Deseja permissão para alocar ambas as aulas na mesma sala (compartilhamento)?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (resp != QMessageBox::Yes) return;
    }
    swap_requested_ = false;
    accept();
}

/**
 * @brief Executa a troca mútua entre as duas salas conflitantes e fecha o diálogo.
 */
void ChangeRoomDialog::onSwapClicked() {
    swap_requested_ = true;
    accept();
}

} // namespace scheduler::ui
