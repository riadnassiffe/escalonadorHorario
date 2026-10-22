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
    setMinimumWidth(650);
    setMinimumHeight(600);

    auto* main_layout = new QVBoxLayout(this);
    main_layout->setSpacing(14);

    // Dados Básicos
    auto* form_layout = new QFormLayout();
    txt_id_ = new QLineEdit(this);
    txt_id_->setPlaceholderText("Ex: P01");
    form_layout->addRow("Código (ID):", txt_id_);

    txt_nome_ = new QLineEdit(this);
    txt_nome_->setPlaceholderText("Ex: Prof. Carlos Silva (Matemática)");
    form_layout->addRow("Nome Completo:", txt_nome_);

    spin_carga_max_ = new QSpinBox(this);
    spin_carga_max_->setRange(0, 1440);
    spin_carga_max_->setSingleStep(30);
    spin_carga_max_->setSuffix(" minutos (0 = sem limite)");
    form_layout->addRow("Carga Máx. Diária:", spin_carga_max_);

    main_layout->addLayout(form_layout);

    // Grupo: Pesos de Preferência por Dia da Semana (0 a 10) - Seção 2.3
    auto* grp_pesos = new QGroupBox("Pesos de Preferência Semanal (0 a 10) - Seção 2.3", this);
    auto* layout_pesos = new QVBoxLayout(grp_pesos);
    layout_pesos->setSpacing(8);

    auto* lbl_aviso = new QLabel(
        "<i>Convenção: 10 = maior preferência, 0 = dia menos desejado (não indisponível). Padrão neutro: 5.</i>", grp_pesos);
    lbl_aviso->setStyleSheet("color: #94a3b8; font-size: 11px;");
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
    main_layout->addWidget(grp_pesos);

    // Grupo: Disponibilidade de Horários por Dia (Seção 3.8)
    auto* grp_disp = new QGroupBox("Disponibilidade de Horários (Seção 3.8)", this);
    auto* layout_disp = new QVBoxLayout(grp_disp);

    auto* tool_bar = new QHBoxLayout();
    auto* btn_add = new QPushButton("+ Adicionar Intervalo", grp_disp);
    btn_add->setObjectName("btnSecondary");
    auto* btn_rem = new QPushButton("- Remover Selecionado", grp_disp);
    btn_rem->setObjectName("btnSecondary");
    auto* btn_def_m = new QPushButton("Manhã (08:00-12:00)", grp_disp);
    btn_def_m->setObjectName("btnSecondary");
    auto* btn_def_all = new QPushButton("Integral (08:00-18:00)", grp_disp);
    btn_def_all->setObjectName("btnSecondary");

    tool_bar->addWidget(btn_add);
    tool_bar->addWidget(btn_rem);
    tool_bar->addWidget(btn_def_m);
    tool_bar->addWidget(btn_def_all);
    tool_bar->addStretch();
    layout_disp->addLayout(tool_bar);

    table_disp_ = new QTableWidget(grp_disp);
    table_disp_->setColumnCount(3);
    table_disp_->setHorizontalHeaderLabels({"Dia", "Hora Início", "Hora Término"});
    table_disp_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_disp_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_disp_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout_disp->addWidget(table_disp_);

    connect(btn_add, &QPushButton::clicked, this, &ProfessorEditorDialog::onAddInterval);
    connect(btn_rem, &QPushButton::clicked, this, &ProfessorEditorDialog::onRemoveInterval);
    connect(btn_def_m, &QPushButton::clicked, this, &ProfessorEditorDialog::onSetDefaultMorning);
    connect(btn_def_all, &QPushButton::clicked, this, &ProfessorEditorDialog::onSetDefaultAllDay);

    main_layout->addWidget(grp_disp);

    // Botões de Ação
    auto* btn_box = new QHBoxLayout();
    btn_box->addStretch();
    auto* btn_cancel = new QPushButton("Cancelar", this);
    btn_cancel->setObjectName("btnSecondary");
    auto* btn_save = new QPushButton("Salvar Professor", this);
    btn_box->addWidget(btn_cancel);
    btn_box->addWidget(btn_save);
    main_layout->addLayout(btn_box);

    connect(btn_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btn_save, &QPushButton::clicked, [this]() {
        if (txt_id_->text().trimmed().isEmpty() || txt_nome_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Atenção", "Preencha o Código e o Nome do Professor.");
            return;
        }
        accept();
    });

    onSliderValueChanged();
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
            lbls_desc_dias_[i]->setText("Preferencia média/neutra");
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

void ProfessorEditorDialog::onAddInterval() {
    int row = table_disp_->rowCount();
    table_disp_->insertRow(row);

    auto* combo_dia = new QComboBox(table_disp_);
    for (auto d : model::allDays()) {
        combo_dia->addItem(QString::fromStdString(model::dayToString(d)));
    }
    table_disp_->setCellWidget(row, 0, combo_dia);

    auto* time_ini = new QTimeEdit(QTime(7, 0), table_disp_);
    time_ini->setDisplayFormat("HH:mm");
    table_disp_->setCellWidget(row, 1, time_ini);

    auto* time_fim = new QTimeEdit(QTime(12, 0), table_disp_);
    time_fim->setDisplayFormat("HH:mm");
    table_disp_->setCellWidget(row, 2, time_fim);
}

void ProfessorEditorDialog::onRemoveInterval() {
    int row = table_disp_->currentRow();
    if (row >= 0) {
        table_disp_->removeRow(row);
    }
}

void ProfessorEditorDialog::onSetDefaultMorning() {
    table_disp_->setRowCount(0);
    for (auto d : model::allDays()) {
        int r = table_disp_->rowCount();
        table_disp_->insertRow(r);

        auto* combo = new QComboBox(table_disp_);
        for (auto day : model::allDays()) combo->addItem(QString::fromStdString(model::dayToString(day)));
        combo->setCurrentText(QString::fromStdString(model::dayToString(d)));
        table_disp_->setCellWidget(r, 0, combo);

        auto* t_ini = new QTimeEdit(QTime(7, 0), table_disp_);
        t_ini->setDisplayFormat("HH:mm");
        table_disp_->setCellWidget(r, 1, t_ini);

        auto* t_fim = new QTimeEdit(QTime(12, 0), table_disp_);
        t_fim->setDisplayFormat("HH:mm");
        table_disp_->setCellWidget(r, 2, t_fim);
    }
}

void ProfessorEditorDialog::onSetDefaultAllDay() {
    table_disp_->setRowCount(0);
    for (auto d : model::allDays()) {
        int r = table_disp_->rowCount();
        table_disp_->insertRow(r);

        auto* combo = new QComboBox(table_disp_);
        for (auto day : model::allDays()) combo->addItem(QString::fromStdString(model::dayToString(day)));
        combo->setCurrentText(QString::fromStdString(model::dayToString(d)));
        table_disp_->setCellWidget(r, 0, combo);

        auto* t_ini = new QTimeEdit(QTime(7, 0), table_disp_);
        t_ini->setDisplayFormat("HH:mm");
        table_disp_->setCellWidget(r, 1, t_ini);

        auto* t_fim = new QTimeEdit(QTime(18, 0), table_disp_);
        t_fim->setDisplayFormat("HH:mm");
        table_disp_->setCellWidget(r, 2, t_fim);
    }
}

void ProfessorEditorDialog::loadProfessor(const model::Professor& prof) {
    txt_id_->setText(QString::fromStdString(prof.id()));
    txt_id_->setReadOnly(true); // Manter ID estável na edição
    txt_nome_->setText(QString::fromStdString(prof.nome()));
    spin_carga_max_->setValue(prof.cargaMaximaDiariaMinutos());

    for (size_t i = 0; i < 5; ++i) {
        auto d = static_cast<model::DayOfWeek>(i);
        sliders_dias_[i]->setValue(prof.pesoDia(d));
    }

    table_disp_->setRowCount(0);
    for (const auto& [dia, intervalos] : prof.todasDisponibilidades()) {
        for (const auto& inter : intervalos) {
            int r = table_disp_->rowCount();
            table_disp_->insertRow(r);

            auto* combo = new QComboBox(table_disp_);
            for (auto day : model::allDays()) combo->addItem(QString::fromStdString(model::dayToString(day)));
            combo->setCurrentText(QString::fromStdString(model::dayToString(dia)));
            table_disp_->setCellWidget(r, 0, combo);

            int start_min = inter.start_slot * model::SLOT_MINUTES;
            auto* t_ini = new QTimeEdit(QTime(start_min / 60, start_min % 60), table_disp_);
            t_ini->setDisplayFormat("HH:mm");
            table_disp_->setCellWidget(r, 1, t_ini);

            int end_min = inter.end_slot * model::SLOT_MINUTES;
            auto* t_fim = new QTimeEdit(QTime(end_min / 60, end_min % 60), table_disp_);
            t_fim->setDisplayFormat("HH:mm");
            table_disp_->setCellWidget(r, 2, t_fim);
        }
    }
}

model::Professor ProfessorEditorDialog::getProfessor() const {
    model::Professor prof(txt_id_->text().trimmed().toStdString(),
                          txt_nome_->text().trimmed().toStdString());
    prof.setCargaMaximaDiariaMinutos(spin_carga_max_->value());

    for (size_t i = 0; i < 5; ++i) {
        auto d = static_cast<model::DayOfWeek>(i);
        prof.setPesoDia(d, sliders_dias_[i]->value());
    }

    for (int r = 0; r < table_disp_->rowCount(); ++r) {
        auto* combo = qobject_cast<QComboBox*>(table_disp_->cellWidget(r, 0));
        auto* t_ini = qobject_cast<QTimeEdit*>(table_disp_->cellWidget(r, 1));
        auto* t_fim = qobject_cast<QTimeEdit*>(table_disp_->cellWidget(r, 2));
        if (combo && t_ini && t_fim) {
            auto dia = model::stringToDay(combo->currentText().toStdString());
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
