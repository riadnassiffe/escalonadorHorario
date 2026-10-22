#include "ui/TurmaEditorDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QMessageBox>
#include <algorithm>

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
    }
    validateJornada();
    onSalaModeChanged();
}

void TurmaEditorDialog::setupUi() {
    setMinimumWidth(640);
    setMinimumHeight(670);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(14);

    // Identificação Básica
    auto* form_layout = new QFormLayout();
    txt_id_ = new QLineEdit(this);
    txt_id_->setPlaceholderText("Ex: T01");
    form_layout->addRow("Código (ID):", txt_id_);

    txt_nome_ = new QLineEdit(this);
    txt_nome_->setPlaceholderText("Ex: 1º Ano A - Ensino Médio");
    form_layout->addRow("Nome da Turma:", txt_nome_);

    combo_turno_ = new QComboBox(this);
    combo_turno_->addItem("Manhã (MANHA)", static_cast<int>(model::ShiftType::MANHA));
    combo_turno_->addItem("Tarde (TARDE)", static_cast<int>(model::ShiftType::TARDE));
    combo_turno_->addItem("Noite (NOITE)", static_cast<int>(model::ShiftType::NOITE));
    combo_turno_->addItem("Integral (INTEGRAL)", static_cast<int>(model::ShiftType::INTEGRAL));
    form_layout->addRow("Turno de Funcionamento:", combo_turno_);

    // Dias Letivos
    auto* dias_layout = new QHBoxLayout();
    for (auto d : model::allDays()) {
        auto* chk = new QCheckBox(QString::fromStdString(model::dayToString(d)), this);
        chk->setChecked(true);
        check_dias_.push_back(chk);
        dias_layout->addWidget(chk);
    }
    dias_layout->addStretch();
    form_layout->addRow("Dias Letivos:", dias_layout);

    main_layout->addLayout(form_layout);

    // Grupo: Jornada Diária e Intervalos (Seção 2.4 & 3.5)
    auto* grp_jornada = new QGroupBox("Jornada Diária e Intervalos de Descanso (Seção 2.4)", this);
    auto* layout_j = new QVBoxLayout(grp_jornada);
    layout_j->setSpacing(10);

    auto* row_j = new QHBoxLayout();
    time_inicio_ = new QTimeEdit(QTime(7, 0), grp_jornada);
    time_inicio_->setDisplayFormat("HH:mm");
    time_fim_ = new QTimeEdit(QTime(11, 30), grp_jornada);
    time_fim_->setDisplayFormat("HH:mm");

    row_j->addWidget(new QLabel("Início da Jornada:"));
    row_j->addWidget(time_inicio_);
    row_j->addSpacing(20);
    row_j->addWidget(new QLabel("Fim da Jornada:"));
    row_j->addWidget(time_fim_);
    row_j->addStretch();
    layout_j->addLayout(row_j);

    auto* row_int1 = new QHBoxLayout();
    chk_int1_ = new QCheckBox("Intervalo 1", grp_jornada);
    chk_int1_->setChecked(true);
    chk_int1_->setStyleSheet("font-weight: bold;");
    time_int1_ini_ = new QTimeEdit(QTime(8, 40), grp_jornada);
    time_int1_ini_->setDisplayFormat("HH:mm");
    time_int1_fim_ = new QTimeEdit(QTime(8, 55), grp_jornada);
    time_int1_fim_->setDisplayFormat("HH:mm");

    row_int1->addWidget(chk_int1_);
    row_int1->addSpacing(15);
    row_int1->addWidget(new QLabel("Início:"));
    row_int1->addWidget(time_int1_ini_);
    row_int1->addSpacing(20);
    row_int1->addWidget(new QLabel("Fim:"));
    row_int1->addWidget(time_int1_fim_);
    row_int1->addStretch();
    layout_j->addLayout(row_int1);

    auto* row_int2 = new QHBoxLayout();
    chk_int2_ = new QCheckBox("Intervalo 2", grp_jornada);
    chk_int2_->setChecked(true);
    chk_int2_->setStyleSheet("font-weight: bold;");
    time_int2_ini_ = new QTimeEdit(QTime(10, 25), grp_jornada);
    time_int2_ini_->setDisplayFormat("HH:mm");
    time_int2_fim_ = new QTimeEdit(QTime(10, 40), grp_jornada);
    time_int2_fim_->setDisplayFormat("HH:mm");

    row_int2->addWidget(chk_int2_);
    row_int2->addSpacing(15);
    row_int2->addWidget(new QLabel("Início:"));
    row_int2->addWidget(time_int2_ini_);
    row_int2->addSpacing(20);
    row_int2->addWidget(new QLabel("Fim:"));
    row_int2->addWidget(time_int2_fim_);
    row_int2->addStretch();
    layout_j->addLayout(row_int2);

    auto* row_int3 = new QHBoxLayout();
    chk_int3_ = new QCheckBox("Intervalo 3", grp_jornada);
    chk_int3_->setChecked(false);
    chk_int3_->setStyleSheet("font-weight: bold;");
    time_int3_ini_ = new QTimeEdit(QTime(11, 0), grp_jornada);
    time_int3_ini_->setDisplayFormat("HH:mm");
    time_int3_ini_->setEnabled(false);
    time_int3_fim_ = new QTimeEdit(QTime(11, 15), grp_jornada);
    time_int3_fim_->setDisplayFormat("HH:mm");
    time_int3_fim_->setEnabled(false);

    row_int3->addWidget(chk_int3_);
    row_int3->addSpacing(15);
    row_int3->addWidget(new QLabel("Início:"));
    row_int3->addWidget(time_int3_ini_);
    row_int3->addSpacing(20);
    row_int3->addWidget(new QLabel("Fim:"));
    row_int3->addWidget(time_int3_fim_);
    row_int3->addStretch();
    layout_j->addLayout(row_int3);

    lbl_status_jornada_ = new QLabel(grp_jornada);
    lbl_status_jornada_->setStyleSheet("font-size: 12px; font-weight: bold;");
    layout_j->addWidget(lbl_status_jornada_);

    connect(time_inicio_, &QTimeEdit::timeChanged, this, &TurmaEditorDialog::validateJornada);
    connect(time_fim_, &QTimeEdit::timeChanged, this, &TurmaEditorDialog::validateJornada);

    connect(chk_int1_, &QCheckBox::toggled, this, [this](bool checked) {
        time_int1_ini_->setEnabled(checked);
        time_int1_fim_->setEnabled(checked);
        validateJornada();
    });
    connect(time_int1_ini_, &QTimeEdit::timeChanged, this, &TurmaEditorDialog::validateJornada);
    connect(time_int1_fim_, &QTimeEdit::timeChanged, this, &TurmaEditorDialog::validateJornada);

    connect(chk_int2_, &QCheckBox::toggled, this, [this](bool checked) {
        time_int2_ini_->setEnabled(checked);
        time_int2_fim_->setEnabled(checked);
        validateJornada();
    });
    connect(time_int2_ini_, &QTimeEdit::timeChanged, this, &TurmaEditorDialog::validateJornada);
    connect(time_int2_fim_, &QTimeEdit::timeChanged, this, &TurmaEditorDialog::validateJornada);

    connect(chk_int3_, &QCheckBox::toggled, this, [this](bool checked) {
        time_int3_ini_->setEnabled(checked);
        time_int3_fim_->setEnabled(checked);
        validateJornada();
    });
    connect(time_int3_ini_, &QTimeEdit::timeChanged, this, &TurmaEditorDialog::validateJornada);
    connect(time_int3_fim_, &QTimeEdit::timeChanged, this, &TurmaEditorDialog::validateJornada);

    main_layout->addWidget(grp_jornada);

    // Grupo: Sala Teórica Fixa (Seção 3.6)
    auto* grp_sala = new QGroupBox("Sala Teórica Permanente Durante a Semana (Seção 3.6)", this);
    auto* layout_s = new QVBoxLayout(grp_sala);

    rad_sala_fixa_ = new QRadioButton("Sala Teórica Pré-definida no cadastro:", grp_sala);
    rad_sala_fixa_->setChecked(true);
    layout_s->addWidget(rad_sala_fixa_);

    combo_sala_fixa_ = new QComboBox(grp_sala);
    for (const auto& s : salas_disponiveis_) {
        combo_sala_fixa_->addItem(QString::fromStdString(s.nome() + " [" + s.id() + "]"),
                                  QString::fromStdString(s.id()));
    }
    layout_s->addWidget(combo_sala_fixa_);

    rad_sala_opt_ = new QRadioButton("Otimizador seleciona uma sala fixa permanente dentre as permitidas:", grp_sala);
    layout_s->addWidget(rad_sala_opt_);

    list_salas_permitidas_ = new QListWidget(grp_sala);
    list_salas_permitidas_->setSelectionMode(QAbstractItemView::MultiSelection);
    list_salas_permitidas_->setMaximumHeight(100);
    for (const auto& s : salas_disponiveis_) {
        auto* item = new QListWidgetItem(QString::fromStdString(s.nome() + " [" + s.id() + "]"), list_salas_permitidas_);
        item->setData(Qt::UserRole, QString::fromStdString(s.id()));
    }
    layout_s->addWidget(list_salas_permitidas_);

    connect(rad_sala_fixa_, &QRadioButton::toggled, this, &TurmaEditorDialog::onSalaModeChanged);
    connect(rad_sala_opt_, &QRadioButton::toggled, this, &TurmaEditorDialog::onSalaModeChanged);

    main_layout->addWidget(grp_sala);

    // Botões
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
        bool a1 = chk_int1_->isChecked();
        bool a2 = chk_int2_->isChecked();
        bool a3 = chk_int3_->isChecked();
        std::string s_ini = time_inicio_->time().toString("HH:mm").toStdString();
        std::string s_fim = time_fim_->time().toString("HH:mm").toStdString();
        std::string s_i1 = time_int1_ini_->time().toString("HH:mm").toStdString();
        std::string s_f1 = time_int1_fim_->time().toString("HH:mm").toStdString();
        std::string s_i2 = time_int2_ini_->time().toString("HH:mm").toStdString();
        std::string s_f2 = time_int2_fim_->time().toString("HH:mm").toStdString();
        std::string s_i3 = time_int3_ini_->time().toString("HH:mm").toStdString();
        std::string s_f3 = time_int3_fim_->time().toString("HH:mm").toStdString();

        model::JornadaTurma j(s_ini, s_fim,
                               s_i1, s_f1, a1,
                               s_i2, s_f2, a2,
                               s_i3, s_f3, a3);
        if (!j.isValid()) {
            QMessageBox::critical(this, "Erro na Jornada",
                                  "Os horários da jornada e intervalos são inconsistentes!\n"
                                  "Condição obrigatória: Início < Intervalos ativos em ordem e sem sobreposição < Fim.");
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

void TurmaEditorDialog::validateJornada() {
    bool a1 = chk_int1_ && chk_int1_->isChecked();
    bool a2 = chk_int2_ && chk_int2_->isChecked();
    bool a3 = chk_int3_ && chk_int3_->isChecked();

    std::string s_ini = time_inicio_->time().toString("HH:mm").toStdString();
    std::string s_fim = time_fim_->time().toString("HH:mm").toStdString();
    std::string s_i1 = time_int1_ini_->time().toString("HH:mm").toStdString();
    std::string s_f1 = time_int1_fim_->time().toString("HH:mm").toStdString();
    std::string s_i2 = time_int2_ini_->time().toString("HH:mm").toStdString();
    std::string s_f2 = time_int2_fim_->time().toString("HH:mm").toStdString();
    std::string s_i3 = time_int3_ini_ ? time_int3_ini_->time().toString("HH:mm").toStdString() : "11:00";
    std::string s_f3 = time_int3_fim_ ? time_int3_fim_->time().toString("HH:mm").toStdString() : "11:15";

    try {
        model::JornadaTurma j(s_ini, s_fim,
                               s_i1, s_f1, a1,
                               s_i2, s_f2, a2,
                               s_i3, s_f3, a3);
        if (j.isValid()) {
            auto blocos = j.blocosLetivos();
            QStringList blocos_desc;
            for (const auto& b : blocos) {
                blocos_desc << QString::fromStdString(b.toString());
            }
            int total_int = (a1 ? 1 : 0) + (a2 ? 1 : 0) + (a3 ? 1 : 0);
            QString desc = QString("✅ Jornada Válida | %1 Bloco(s) Letivo(s): %2 (%3 intervalo(s) ativo(s))")
                .arg(blocos.size())
                .arg(blocos_desc.join(", "))
                .arg(total_int);
            lbl_status_jornada_->setText(desc);
            lbl_status_jornada_->setStyleSheet("color: #10b981; font-weight: bold;");
        } else {
            lbl_status_jornada_->setText("❌ Inconsistência: Início < Intervalos ativos em ordem e sem sobreposição < Fim.");
            lbl_status_jornada_->setStyleSheet("color: #ef4444; font-weight: bold;");
        }
    } catch (...) {
        lbl_status_jornada_->setText("❌ Horários inválidos.");
        lbl_status_jornada_->setStyleSheet("color: #ef4444; font-weight: bold;");
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

    for (size_t i = 0; i < check_dias_.size(); ++i) {
        auto d = static_cast<model::DayOfWeek>(i);
        check_dias_[i]->setChecked(turma.funcionaNoDia(d));
    }

    const auto& jp = turma.jornadaPadrao();
    int ini_m = jp.jornada_total.start_slot * model::SLOT_MINUTES;
    int fim_m = jp.jornada_total.end_slot * model::SLOT_MINUTES;
    time_inicio_->setTime(QTime(ini_m / 60, ini_m % 60));
    time_fim_->setTime(QTime(fim_m / 60, fim_m % 60));

    chk_int1_->setChecked(jp.int1_ativo);
    int i1_ini_m = jp.intervalo_1.start_slot * model::SLOT_MINUTES;
    int i1_fim_m = jp.intervalo_1.end_slot * model::SLOT_MINUTES;
    time_int1_ini_->setTime(QTime(i1_ini_m / 60, i1_ini_m % 60));
    time_int1_fim_->setTime(QTime(i1_fim_m / 60, i1_fim_m % 60));
    time_int1_ini_->setEnabled(jp.int1_ativo);
    time_int1_fim_->setEnabled(jp.int1_ativo);

    chk_int2_->setChecked(jp.int2_ativo);
    int i2_ini_m = jp.intervalo_2.start_slot * model::SLOT_MINUTES;
    int i2_fim_m = jp.intervalo_2.end_slot * model::SLOT_MINUTES;
    time_int2_ini_->setTime(QTime(i2_ini_m / 60, i2_ini_m % 60));
    time_int2_fim_->setTime(QTime(i2_fim_m / 60, i2_fim_m % 60));
    time_int2_ini_->setEnabled(jp.int2_ativo);
    time_int2_fim_->setEnabled(jp.int2_ativo);

    chk_int3_->setChecked(jp.int3_ativo);
    int i3_ini_m = jp.intervalo_3.start_slot * model::SLOT_MINUTES;
    int i3_fim_m = jp.intervalo_3.end_slot * model::SLOT_MINUTES;
    if (i3_ini_m > 0 || i3_fim_m > 0) {
        time_int3_ini_->setTime(QTime(i3_ini_m / 60, i3_ini_m % 60));
        time_int3_fim_->setTime(QTime(i3_fim_m / 60, i3_fim_m % 60));
    }
    time_int3_ini_->setEnabled(jp.int3_ativo);
    time_int3_fim_->setEnabled(jp.int3_ativo);

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
}

model::Turma TurmaEditorDialog::getTurma() const {
    auto shift = static_cast<model::ShiftType>(combo_turno_->currentData().toInt());
    model::Turma t(txt_id_->text().trimmed().toStdString(),
                   txt_nome_->text().trimmed().toStdString(),
                   shift);

    std::vector<model::DayOfWeek> dias;
    for (size_t i = 0; i < check_dias_.size(); ++i) {
        if (check_dias_[i]->isChecked()) {
            dias.push_back(static_cast<model::DayOfWeek>(i));
        }
    }
    t.setDiasLetivos(dias);

    bool a1 = chk_int1_->isChecked();
    bool a2 = chk_int2_->isChecked();
    bool a3 = chk_int3_->isChecked();

    std::string s_ini = time_inicio_->time().toString("HH:mm").toStdString();
    std::string s_fim = time_fim_->time().toString("HH:mm").toStdString();
    std::string s_i1 = time_int1_ini_->time().toString("HH:mm").toStdString();
    std::string s_f1 = time_int1_fim_->time().toString("HH:mm").toStdString();
    std::string s_i2 = time_int2_ini_->time().toString("HH:mm").toStdString();
    std::string s_f2 = time_int2_fim_->time().toString("HH:mm").toStdString();
    std::string s_i3 = time_int3_ini_->time().toString("HH:mm").toStdString();
    std::string s_f3 = time_int3_fim_->time().toString("HH:mm").toStdString();

    t.setJornadaPadrao(model::JornadaTurma(s_ini, s_fim,
                                           s_i1, s_f1, a1,
                                           s_i2, s_f2, a2,
                                           s_i3, s_f3, a3));

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
