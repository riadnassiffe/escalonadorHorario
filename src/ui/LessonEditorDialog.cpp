/**
 * @file LessonEditorDialog.cpp
 * @brief Implementação da janela de cadastro e edição de disciplinas e suas regras de alocação.
 */

#include "ui/LessonEditorDialog.h"
#include "ui/ModernTheme.h"
#include "ui/ResponsiveUi.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QSignalBlocker>
#include <algorithm>

namespace scheduler::ui {

namespace {
QString normalizeSearchText(const QString& s) {
    QString decomp = s.normalized(QString::NormalizationForm_KD);
    QString clean;
    clean.reserve(decomp.size());
    for (QChar ch : decomp) {
        if (ch.category() != QChar::Mark_NonSpacing) {
            clean.append(ch);
        }
    }
    return clean.toLower();
}
} // namespace

/**
 * @brief Construtor da janela de edição/criação de disciplina.
 * @param profs Lista de professores cadastrados.
 * @param turmas Lista de turmas cadastradas.
 * @param salas Lista de salas disponíveis.
 * @param parent Widget pai na hierarquia Qt.
 * @param disc Ponteiro opcional para dados de disciplina existente.
 * @param default_id ID sugerido se for novo cadastro de disciplina.
 */
LessonEditorDialog::LessonEditorDialog(const std::vector<model::Professor>& profs,
                                       const std::vector<model::Turma>& turmas,
                                       const std::vector<model::Room>& salas,
                                       QWidget* parent,
                                       const model::Disciplina* disc,
                                       const std::string& default_id)
    : QDialog(parent), profs_(profs), turmas_(turmas), salas_(salas) {
    if (disc) {
        id_ = disc->id();
    } else if (!default_id.empty()) {
        id_ = default_id;
    } else {
        id_ = "D01";
    }

    setupUi();
    if (disc) {
        loadDisciplina(*disc);
        setWindowTitle("Editar Disciplina - " + QString::fromStdString(disc->nome()));
    } else {
        setWindowTitle("Nova Disciplina");
    }
}

/**
 * @brief Constrói os campos do formulário, controles de regime e seleção de salas.
 */
void LessonEditorDialog::setupUi() {
    setWindowIcon(ModernTheme::icon("disciplina"));
    setMinimumSize(440, 360);
    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(12, 12, 12, 12);
    root_layout->setSpacing(10);

    auto* body_widget = new QWidget(this);
    auto* main_layout = new QVBoxLayout(body_widget);
    main_layout->setContentsMargins(2, 2, 2, 2);
    main_layout->setSpacing(12);

    auto* form_layout = new QFormLayout();
    form_layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    txt_nome_ = new QLineEdit(body_widget);
    txt_nome_->setPlaceholderText("Ex: Matemática, Física Experimental");
    form_layout->addRow("Nome da Disciplina:", txt_nome_);

    combo_regime_ = new QComboBox(body_widget);
    combo_regime_->addItem(ModernTheme::icon("calendar"), "Anual (Ano Letivo Completo)", "ANUAL");
    combo_regime_->addItem(ModernTheme::icon("calendar"), "Semestral", "SEMESTRAL");
    makeComboResponsive(combo_regime_, 18, 180);
    form_layout->addRow("Regime:", combo_regime_);

    combo_semestre_ = new QComboBox(body_widget);
    combo_semestre_->addItem("Ambos os Semestres (Ano Todo)", "AMBOS");
    combo_semestre_->addItem("1º Semestre", "1");
    combo_semestre_->addItem("2º Semestre", "2");
    combo_semestre_->setEnabled(false); // Anual por padrão
    makeComboResponsive(combo_semestre_, 18, 180);
    form_layout->addRow("Semestre de Oferta:", combo_semestre_);

    combo_tipo_ = new QComboBox(body_widget);
    combo_tipo_->addItem("Teórica (Usa a sala fixa da turma)", "TEORICA");
    combo_tipo_->addItem("Laboratório (Usa salas especiais/laboratórios)", "LABORATORIO");
    combo_tipo_->addItem("Atividade Especial (Quadra/Oficina)", "ESPECIAL");
    makeComboResponsive(combo_tipo_, 20, 180);
    form_layout->addRow("Tipo de Atividade:", combo_tipo_);

    combo_duracao_ = new QComboBox(body_widget);
    combo_duracao_->addItem("45 minutos (9 slots de 5 min)", 45);
    combo_duracao_->addItem("50 minutos (10 slots de 5 min)", 50);
    combo_duracao_->addItem("90 minutos (18 slots de 5 min)", 90);
    combo_duracao_->addItem("100 minutos (20 slots de 5 min)", 100);
    combo_duracao_->setCurrentIndex(1); // 50 min padrão
    makeComboResponsive(combo_duracao_, 18, 180);
    form_layout->addRow("Duração por Aula (Seção 1.1):", combo_duracao_);

    spin_aulas_semanais_ = new QSpinBox(body_widget);
    spin_aulas_semanais_->setRange(1, 30);
    spin_aulas_semanais_->setValue(4);
    spin_aulas_semanais_->setSuffix(" aulas por semana");
    form_layout->addRow("Carga Semanal:", spin_aulas_semanais_);

    auto* widget_profs = new QWidget(body_widget);
    auto* lay_profs = new QVBoxLayout(widget_profs);
    lay_profs->setContentsMargins(0, 0, 0, 0);
    lay_profs->setSpacing(4);

    lbl_profs_resumo_ = new QLabel(widget_profs);
    lbl_profs_resumo_->setWordWrap(true);
    lbl_profs_resumo_->setStyleSheet("font-size: 11px; color: palette(highlight); font-weight: 500;");
    lay_profs->addWidget(lbl_profs_resumo_);

    txt_search_prof_ = new QLineEdit(widget_profs);
    txt_search_prof_->setPlaceholderText("Filtrar professores por nome ou ID...");
    txt_search_prof_->setClearButtonEnabled(true);
    lay_profs->addWidget(txt_search_prof_);

    list_professores_ = new QListWidget(widget_profs);
    list_professores_->setMinimumHeight(100);
    list_professores_->setMaximumHeight(150);
    for (const auto& p : profs_) {
        auto* item = new QListWidgetItem(
            QString::fromStdString(p.nome() + " (" + p.id() + ")"),
            list_professores_);
        item->setData(Qt::UserRole, QString::fromStdString(p.id()));
        item->setData(Qt::UserRole + 1, QString::fromStdString(p.nome()));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    lay_profs->addWidget(list_professores_);

    connect(txt_search_prof_, &QLineEdit::textChanged, this, &LessonEditorDialog::filterProfessores);
    connect(list_professores_, &QListWidget::itemChanged, this, [this](QListWidgetItem*) {
        updateProfessoresSummary();
    });
    updateProfessoresSummary();
    form_layout->addRow("Professores Responsáveis:", widget_profs);

    combo_turma_ = new QComboBox(body_widget);
    for (const auto& t : turmas_) {
        combo_turma_->addItem(QString::fromStdString(t.nome()),
                              QString::fromStdString(t.id()));
    }
    makeComboResponsive(combo_turma_, 20, 180);
    form_layout->addRow("Turma:", combo_turma_);

    spin_max_dia_ = new QSpinBox(body_widget);
    spin_max_dia_->setRange(0, 10);
    spin_max_dia_->setSpecialValueText("Sem limite");
    spin_max_dia_->setValue(2);
    spin_max_dia_->setSuffix(" aulas por dia");
    spin_max_dia_->setToolTip("Limite máximo de aulas desta disciplina no mesmo dia para a turma (0 = sem limite).");
    form_layout->addRow("Máximo por Dia:", spin_max_dia_);

    check_consecutivas_ = new QCheckBox("Aulas devem ser ministradas em blocos consecutivos", body_widget);
    form_layout->addRow("", check_consecutivas_);

    main_layout->addLayout(form_layout);

    // Painel de Salas Alternativas para Laboratório / Especial
    widget_salas_alternativas_ = new QWidget(body_widget);
    auto* lay_alt = new QVBoxLayout(widget_salas_alternativas_);
    lay_alt->setContentsMargins(0, 0, 0, 0);
    auto* lbl_alt = new QLabel("Salas Admissíveis para esta Atividade Especial (Selecione 1 ou mais):", widget_salas_alternativas_);
    lbl_alt->setWordWrap(true);
    lay_alt->addWidget(lbl_alt);

    list_salas_alternativas_ = new QListWidget(widget_salas_alternativas_);
    list_salas_alternativas_->setSelectionMode(QAbstractItemView::MultiSelection);
    list_salas_alternativas_->setMinimumHeight(80);
    list_salas_alternativas_->setMaximumHeight(120);
    for (const auto& s : salas_) {
        auto* item = new QListWidgetItem(QString::fromStdString(s.nome() + " (" + s.tipo() + ")"), list_salas_alternativas_);
        item->setData(Qt::UserRole, QString::fromStdString(s.id()));
    }
    lay_alt->addWidget(list_salas_alternativas_);
    main_layout->addWidget(widget_salas_alternativas_);
    main_layout->addStretch();

    root_layout->addWidget(wrapInScrollArea(body_widget, this), 1);

    connect(combo_tipo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LessonEditorDialog::onTipoChanged);
    connect(combo_regime_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LessonEditorDialog::onRegimeChanged);

    onTipoChanged(combo_tipo_->currentIndex());
    onRegimeChanged(combo_regime_->currentIndex());

    // Botões (fixos no rodapé)
    auto* btn_box = new QHBoxLayout();
    btn_box->addStretch();
    auto* btn_cancel = new QPushButton("Cancelar", this);
    btn_cancel->setObjectName("btnSecondary");
    auto* btn_save = new QPushButton(ModernTheme::icon("save_file"), "Salvar Disciplina", this);
    btn_box->addWidget(btn_cancel);
    btn_box->addWidget(btn_save);
    root_layout->addLayout(btn_box);

    fitWindowToScreen(this, QSize(580, 620));

    connect(btn_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btn_save, &QPushButton::clicked, [this]() {
        if (txt_nome_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Atenção", "Preencha o Nome da Disciplina.");
            return;
        }
        int prof_count = 0;
        for (int i = 0; i < list_professores_->count(); ++i) {
            if (list_professores_->item(i)->checkState() == Qt::Checked) {
                prof_count++;
            }
        }
        if (prof_count == 0) {
            QMessageBox::warning(this, "Atenção", "Selecione ao menos um Professor responsável.");
            return;
        }
        if (combo_turma_->currentIndex() < 0) {
            QMessageBox::warning(this, "Atenção", "Selecione a Turma.");
            return;
        }
        if (combo_tipo_->currentData().toString() != "TEORICA" &&
            list_salas_alternativas_->selectedItems().isEmpty()) {
            QMessageBox::warning(this, "Atenção", "Selecione ao menos uma sala admissível para laboratório/especial.");
            return;
        }
        accept();
    });
}

/**
 * @brief Mostra ou oculta a seleção de salas alternativas conforme o tipo de ambiente (teórica vs laboratório).
 * @param index Índice do item selecionado.
 */
void LessonEditorDialog::onTipoChanged(int /*index*/) {
    bool is_teorica = (combo_tipo_->currentData().toString() == "TEORICA");
    widget_salas_alternativas_->setVisible(!is_teorica);
}

/**
 * @brief Habilita ou desabilita o campo de semestre de oferta conforme o regime (anual vs semestral).
 * @param index Índice do regime selecionado.
 */
void LessonEditorDialog::onRegimeChanged(int /*index*/) {
    bool is_anual = (combo_regime_->currentData().toString() == "ANUAL");
    if (is_anual) {
        combo_semestre_->setCurrentIndex(0); // Ambos os semestres
        combo_semestre_->setEnabled(false);
    } else {
        combo_semestre_->setEnabled(true);
        if (combo_semestre_->currentIndex() == 0) {
            combo_semestre_->setCurrentIndex(1); // 1º Semestre por padrão
        }
    }
}

/**
 * @brief Carrega os dados de uma disciplina existente nos campos do diálogo.
 * @param disc Instância de model::Disciplina a ser carregada.
 */
void LessonEditorDialog::loadDisciplina(const model::Disciplina& disc) {
    id_ = disc.id();
    txt_nome_->setText(QString::fromStdString(disc.nome()));

    int idx_regime = combo_regime_->findData(QString::fromStdString(model::regimeToString(disc.regime())));
    if (idx_regime >= 0) combo_regime_->setCurrentIndex(idx_regime);

    int idx_sem = combo_semestre_->findData(QString::fromStdString(model::semestreToString(disc.semestre())));
    if (idx_sem >= 0) combo_semestre_->setCurrentIndex(idx_sem);

    onRegimeChanged(combo_regime_->currentIndex());

    int idx_tipo = combo_tipo_->findData(QString::fromStdString(disc.tipo()));
    if (idx_tipo >= 0) combo_tipo_->setCurrentIndex(idx_tipo);

    int idx_dur = combo_duracao_->findData(disc.duracaoMinutos());
    if (idx_dur >= 0) combo_duracao_->setCurrentIndex(idx_dur);

    spin_aulas_semanais_->setValue(disc.aulasSemanais());
    spin_max_dia_->setValue(disc.maxAulasPorDiaTurma());
    check_consecutivas_->setChecked(disc.exigirAulasConsecutivas());

    {
        const QSignalBlocker blocker(list_professores_);
        std::vector<std::string> matched_ids;
        for (int i = 0; i < list_professores_->count(); ++i) {
            auto* item = list_professores_->item(i);
            std::string p_id = item->data(Qt::UserRole).toString().toStdString();
            bool selected = std::find(disc.professores().begin(), disc.professores().end(), p_id) != disc.professores().end();
            item->setCheckState(selected ? Qt::Checked : Qt::Unchecked);
            if (selected) {
                matched_ids.push_back(p_id);
            }
        }
        for (const auto& p_id : disc.professores()) {
            if (std::find(matched_ids.begin(), matched_ids.end(), p_id) == matched_ids.end()) {
                auto* item = new QListWidgetItem(QString::fromStdString(p_id), list_professores_);
                item->setData(Qt::UserRole, QString::fromStdString(p_id));
                item->setData(Qt::UserRole + 1, QString::fromStdString(p_id));
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Checked);
            }
        }
        int insert_pos = 0;
        for (int i = 0; i < list_professores_->count(); ++i) {
            if (list_professores_->item(i)->checkState() == Qt::Checked) {
                if (i != insert_pos) {
                    auto* taken = list_professores_->takeItem(i);
                    list_professores_->insertItem(insert_pos, taken);
                }
                insert_pos++;
            }
        }
    }
    updateProfessoresSummary();

    if (!disc.turmas().empty()) {
        int idx_t = combo_turma_->findData(QString::fromStdString(disc.turmas().front()));
        if (idx_t >= 0) combo_turma_->setCurrentIndex(idx_t);
    }

    // Salas alternativas
    for (int i = 0; i < list_salas_alternativas_->count(); ++i) {
        auto* item = list_salas_alternativas_->item(i);
        std::string s_id = item->data(Qt::UserRole).toString().toStdString();
        bool found = false;
        for (const auto& combo : disc.salasAlternativas()) {
            if (std::find(combo.begin(), combo.end(), s_id) != combo.end()) {
                found = true;
                break;
            }
        }
        item->setSelected(found);
    }

    onTipoChanged(combo_tipo_->currentIndex());
}

/**
 * @brief Constrói e retorna uma nova model::Disciplina com os valores do formulário.
 * @return model::Disciplina preenchida.
 */
model::Disciplina LessonEditorDialog::getDisciplina() const {
    model::RegimeDisciplina regime = model::stringToRegime(combo_regime_->currentData().toString().toStdString());
    model::SemestreLetivo semestre = model::stringToSemestre(combo_semestre_->currentData().toString().toStdString());

    model::Disciplina d(id_,
                        txt_nome_->text().trimmed().toStdString(),
                        combo_tipo_->currentData().toString().toStdString(),
                        combo_duracao_->currentData().toInt(),
                        spin_aulas_semanais_->value(),
                        regime, semestre);

    for (int i = 0; i < list_professores_->count(); ++i) {
        auto* item = list_professores_->item(i);
        if (item->checkState() == Qt::Checked) {
            d.addProfessor(item->data(Qt::UserRole).toString().toStdString());
        }
    }
    d.addTurma(combo_turma_->currentData().toString().toStdString());

    d.setMaxAulasPorDiaTurma(spin_max_dia_->value());
    d.setExigirAulasConsecutivas(check_consecutivas_->isChecked());

    if (d.tipo() != "TEORICA") {
        for (auto* item : list_salas_alternativas_->selectedItems()) {
            d.addCombinacaoSalas({item->data(Qt::UserRole).toString().toStdString()});
        }
    }

    return d;
}

void LessonEditorDialog::filterProfessores(const QString& query) {
    if (!list_professores_) return;
    const QString needle = normalizeSearchText(query.trimmed());
    for (int i = 0; i < list_professores_->count(); ++i) {
        auto* item = list_professores_->item(i);
        const bool match = needle.isEmpty() || normalizeSearchText(item->text()).contains(needle);
        item->setHidden(!match);
    }
}

void LessonEditorDialog::updateProfessoresSummary() {
    if (!lbl_profs_resumo_ || !list_professores_) return;
    QStringList selected_names;
    for (int i = 0; i < list_professores_->count(); ++i) {
        auto* item = list_professores_->item(i);
        if (item->checkState() == Qt::Checked) {
            QString nome = item->data(Qt::UserRole + 1).toString();
            selected_names << (nome.isEmpty() ? item->text() : nome);
        }
    }
    if (selected_names.isEmpty()) {
        lbl_profs_resumo_->setText("Nenhum professor selecionado");
    } else {
        lbl_profs_resumo_->setText(
            QString("%1 selecionado(s): %2").arg(selected_names.size()).arg(selected_names.join(", ")));
    }
}

} // namespace scheduler::ui
