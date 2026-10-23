#include "ui/LessonEditorDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>

namespace scheduler::ui {

LessonEditorDialog::LessonEditorDialog(const std::vector<model::Professor>& profs,
                                       const std::vector<model::Turma>& turmas,
                                       const std::vector<model::Room>& salas,
                                       QWidget* parent,
                                       const model::Disciplina* disc)
    : QDialog(parent), profs_(profs), turmas_(turmas), salas_(salas) {
    setupUi();
    if (disc) {
        loadDisciplina(*disc);
        setWindowTitle("Editar Disciplina - " + QString::fromStdString(disc->nome()));
    } else {
        setWindowTitle("Nova Disciplina");
    }
}

void LessonEditorDialog::setupUi() {
    setMinimumWidth(550);
    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(14);

    auto* form_layout = new QFormLayout();

    txt_id_ = new QLineEdit(this);
    txt_id_->setPlaceholderText("Ex: MAT_01, FIS_01");
    form_layout->addRow("Código (ID):", txt_id_);

    txt_nome_ = new QLineEdit(this);
    txt_nome_->setPlaceholderText("Ex: Matemática, Física Experimental");
    form_layout->addRow("Nome da Disciplina:", txt_nome_);

    combo_regime_ = new QComboBox(this);
    combo_regime_->addItem("📅 Anual (Ano Letivo Completo)", "ANUAL");
    combo_regime_->addItem("📆 Semestral", "SEMESTRAL");
    form_layout->addRow("Regime:", combo_regime_);

    combo_semestre_ = new QComboBox(this);
    combo_semestre_->addItem("Ambos os Semestres (Ano Todo)", "AMBOS");
    combo_semestre_->addItem("1º Semestre", "1");
    combo_semestre_->addItem("2º Semestre", "2");
    combo_semestre_->setEnabled(false); // Anual por padrão
    form_layout->addRow("Semestre de Oferta:", combo_semestre_);

    combo_tipo_ = new QComboBox(this);
    combo_tipo_->addItem("Teórica (Usa a sala fixa da turma)", "TEORICA");
    combo_tipo_->addItem("Laboratório (Usa salas especiais/laboratórios)", "LABORATORIO");
    combo_tipo_->addItem("Atividade Especial (Quadra/Oficina)", "ESPECIAL");
    form_layout->addRow("Tipo de Atividade:", combo_tipo_);

    combo_duracao_ = new QComboBox(this);
    combo_duracao_->addItem("45 minutos (9 slots de 5 min)", 45);
    combo_duracao_->addItem("50 minutos (10 slots de 5 min)", 50);
    combo_duracao_->addItem("90 minutos (18 slots de 5 min)", 90);
    combo_duracao_->addItem("100 minutos (20 slots de 5 min)", 100);
    combo_duracao_->setCurrentIndex(1); // 50 min padrão
    form_layout->addRow("Duração por Aula (Seção 1.1):", combo_duracao_);

    spin_aulas_semanais_ = new QSpinBox(this);
    spin_aulas_semanais_->setRange(1, 30);
    spin_aulas_semanais_->setValue(4);
    spin_aulas_semanais_->setSuffix(" aulas por semana");
    form_layout->addRow("Carga Semanal:", spin_aulas_semanais_);

    combo_prof_ = new QComboBox(this);
    for (const auto& p : profs_) {
        combo_prof_->addItem(QString::fromStdString(p.nome() + " [" + p.id() + "]"),
                             QString::fromStdString(p.id()));
    }
    form_layout->addRow("Professor Responsável:", combo_prof_);

    combo_turma_ = new QComboBox(this);
    for (const auto& t : turmas_) {
        combo_turma_->addItem(QString::fromStdString(t.nome() + " [" + t.id() + "]"),
                              QString::fromStdString(t.id()));
    }
    form_layout->addRow("Turma:", combo_turma_);

    // Turnos permitidos
    auto* turnos_layout = new QHBoxLayout();
    auto* chk_manha = new QCheckBox("Manhã", this);
    chk_manha->setChecked(true);
    auto* chk_tarde = new QCheckBox("Tarde", this);
    auto* chk_noite = new QCheckBox("Noite", this);
    turnos_layout->addWidget(chk_manha);
    turnos_layout->addWidget(chk_tarde);
    turnos_layout->addWidget(chk_noite);
    turnos_layout->addStretch();
    check_turnos_ = {chk_manha, chk_tarde, chk_noite};
    form_layout->addRow("Turnos Permitidos:", turnos_layout);

    auto updateTurnosForTurma = [this](int idx) {
        if (idx < 0 || idx >= static_cast<int>(turmas_.size())) return;
        const auto& t = turmas_[idx];
        bool tem_manha = false;
        bool tem_tarde = false;
        bool tem_noite = false;

        for (auto d : t.diasLetivos()) {
            auto j = t.jornadaParaDia(d);
            int start_min = j.jornada_total.start_slot * model::SLOT_MINUTES;
            int end_min = j.jornada_total.end_slot * model::SLOT_MINUTES;
            if (start_min < 12 * 60 + 30) tem_manha = true;
            if (end_min > 12 * 60 + 30 && start_min < 18 * 60) tem_tarde = true;
            if (end_min > 18 * 60) tem_noite = true;
        }
        if (t.turno() == model::ShiftType::INTEGRAL) {
            tem_manha = true;
            tem_tarde = true;
        } else if (t.turno() == model::ShiftType::TARDE) {
            tem_tarde = true;
        } else if (t.turno() == model::ShiftType::NOITE) {
            tem_noite = true;
        }

        check_turnos_[0]->setChecked(tem_manha);
        check_turnos_[1]->setChecked(tem_tarde);
        check_turnos_[2]->setChecked(tem_noite);
    };

    connect(combo_turma_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, updateTurnosForTurma);
    if (!turmas_.empty()) {
        updateTurnosForTurma(0);
    }

    spin_max_dia_ = new QSpinBox(this);
    spin_max_dia_->setRange(1, 10);
    spin_max_dia_->setValue(2);
    spin_max_dia_->setSuffix(" aulas por dia");
    form_layout->addRow("Máximo por Dia:", spin_max_dia_);

    check_consecutivas_ = new QCheckBox("Aulas devem ser ministradas em blocos consecutivos", this);
    form_layout->addRow("", check_consecutivas_);

    main_layout->addLayout(form_layout);

    // Painel de Salas Alternativas para Laboratório / Especial
    widget_salas_alternativas_ = new QWidget(this);
    auto* lay_alt = new QVBoxLayout(widget_salas_alternativas_);
    lay_alt->setContentsMargins(0, 0, 0, 0);
    lay_alt->addWidget(new QLabel("Salas Admissíveis para esta Atividade Especial (Selecione 1 ou mais):", this));

    list_salas_alternativas_ = new QListWidget(this);
    list_salas_alternativas_->setSelectionMode(QAbstractItemView::MultiSelection);
    list_salas_alternativas_->setMaximumHeight(90);
    for (const auto& s : salas_) {
        auto* item = new QListWidgetItem(QString::fromStdString(s.nome() + " [" + s.id() + "] (" + s.tipo() + ")"), list_salas_alternativas_);
        item->setData(Qt::UserRole, QString::fromStdString(s.id()));
    }
    lay_alt->addWidget(list_salas_alternativas_);
    main_layout->addWidget(widget_salas_alternativas_);

    connect(combo_tipo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LessonEditorDialog::onTipoChanged);
    connect(combo_regime_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &LessonEditorDialog::onRegimeChanged);

    onTipoChanged(combo_tipo_->currentIndex());
    onRegimeChanged(combo_regime_->currentIndex());

    // Botões
    auto* btn_box = new QHBoxLayout();
    btn_box->addStretch();
    auto* btn_cancel = new QPushButton("Cancelar", this);
    btn_cancel->setObjectName("btnSecondary");
    auto* btn_save = new QPushButton("Salvar Disciplina", this);
    btn_box->addWidget(btn_cancel);
    btn_box->addWidget(btn_save);
    main_layout->addLayout(btn_box);

    connect(btn_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btn_save, &QPushButton::clicked, [this]() {
        if (txt_id_->text().trimmed().isEmpty() || txt_nome_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Atenção", "Preencha o Código e o Nome da Disciplina.");
            return;
        }
        if (combo_prof_->currentIndex() < 0) {
            QMessageBox::warning(this, "Atenção", "Selecione o Professor responsável.");
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

void LessonEditorDialog::onTipoChanged(int /*index*/) {
    bool is_teorica = (combo_tipo_->currentData().toString() == "TEORICA");
    widget_salas_alternativas_->setVisible(!is_teorica);
}

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

void LessonEditorDialog::loadDisciplina(const model::Disciplina& disc) {
    txt_id_->setText(QString::fromStdString(disc.id()));
    txt_id_->setReadOnly(true);
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

    if (!disc.professores().empty()) {
        int idx_p = combo_prof_->findData(QString::fromStdString(disc.professores().front()));
        if (idx_p >= 0) combo_prof_->setCurrentIndex(idx_p);
    }

    if (!disc.turmas().empty()) {
        int idx_t = combo_turma_->findData(QString::fromStdString(disc.turmas().front()));
        if (idx_t >= 0) combo_turma_->setCurrentIndex(idx_t);
    }

    // Turnos
    check_turnos_[0]->setChecked(false);
    check_turnos_[1]->setChecked(false);
    check_turnos_[2]->setChecked(false);
    for (auto st : disc.turnosPermitidos()) {
        if (st == model::ShiftType::MANHA) check_turnos_[0]->setChecked(true);
        if (st == model::ShiftType::TARDE) check_turnos_[1]->setChecked(true);
        if (st == model::ShiftType::NOITE) check_turnos_[2]->setChecked(true);
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

model::Disciplina LessonEditorDialog::getDisciplina() const {
    model::RegimeDisciplina regime = model::stringToRegime(combo_regime_->currentData().toString().toStdString());
    model::SemestreLetivo semestre = model::stringToSemestre(combo_semestre_->currentData().toString().toStdString());

    model::Disciplina d(txt_id_->text().trimmed().toStdString(),
                        txt_nome_->text().trimmed().toStdString(),
                        combo_tipo_->currentData().toString().toStdString(),
                        combo_duracao_->currentData().toInt(),
                        spin_aulas_semanais_->value(),
                        regime, semestre);

    d.addProfessor(combo_prof_->currentData().toString().toStdString());
    d.addTurma(combo_turma_->currentData().toString().toStdString());

    std::vector<model::ShiftType> turnos;
    if (check_turnos_[0]->isChecked()) turnos.push_back(model::ShiftType::MANHA);
    if (check_turnos_[1]->isChecked()) turnos.push_back(model::ShiftType::TARDE);
    if (check_turnos_[2]->isChecked()) turnos.push_back(model::ShiftType::NOITE);
    if (turnos.empty()) turnos.push_back(model::ShiftType::MANHA);
    d.setTurnosPermitidos(turnos);

    d.setMaxAulasPorDiaTurma(spin_max_dia_->value());
    d.setExigirAulasConsecutivas(check_consecutivas_->isChecked());

    if (d.tipo() != "TEORICA") {
        for (auto* item : list_salas_alternativas_->selectedItems()) {
            d.addCombinacaoSalas({item->data(Qt::UserRole).toString().toStdString()});
        }
    }

    return d;
}

} // namespace scheduler::ui
