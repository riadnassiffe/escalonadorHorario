/**
 * @file RoomEditorDialog.cpp
 * @brief Implementação do diálogo de edição e cadastro de salas e laboratórios.
 */

#include "ui/RoomEditorDialog.h"
#include "ui/ModernTheme.h"
#include "ui/ResponsiveUi.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QMessageBox>
#include <sstream>

namespace scheduler::ui {

/**
 * @brief Construtor do diálogo de sala.
 * @param parent Widget pai na interface Qt.
 * @param room Ponteiro opcional para a sala a ser editada.
 * @param default_id ID padrão a sugerir se for inclusão de nova sala.
 */
RoomEditorDialog::RoomEditorDialog(QWidget* parent, const model::Room* room, const std::string& default_id)
    : QDialog(parent) {
    if (room) {
        id_ = room->id();
    } else if (!default_id.empty()) {
        id_ = default_id;
    } else {
        id_ = "S01";
    }

    setupUi();
    if (room) {
        loadRoom(*room);
        setWindowTitle("Editar Sala - " + QString::fromStdString(room->nome()));
    } else {
        setWindowTitle("Nova Sala");
    }
}

/**
 * @brief Monta o layout de campos e botões de confirmação.
 */
void RoomEditorDialog::setupUi() {
    setWindowIcon(ModernTheme::icon("room"));
    setMinimumSize(380, 260);

    auto* root_layout = new QVBoxLayout(this);
    root_layout->setContentsMargins(12, 12, 12, 12);
    root_layout->setSpacing(10);

    auto* body_widget = new QWidget(this);
    auto* body_layout = new QVBoxLayout(body_widget);
    body_layout->setContentsMargins(2, 2, 2, 2);

    auto* form_layout = new QFormLayout();
    form_layout->setSpacing(10);
    form_layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    txt_nome_ = new QLineEdit(body_widget);
    txt_nome_->setPlaceholderText("Ex: Sala Teórica 101");
    form_layout->addRow("Nome:", txt_nome_);

    combo_tipo_ = new QComboBox(body_widget);
    combo_tipo_->addItem("Sala Teórica (TEORICA)", "TEORICA");
    combo_tipo_->addItem("Laboratório (LABORATORIO)", "LABORATORIO");
    combo_tipo_->addItem("Ambiente Especial (ESPECIAL)", "ESPECIAL");
    makeComboResponsive(combo_tipo_, 18, 180);
    form_layout->addRow("Tipo de Ambiente:", combo_tipo_);

    spin_capacidade_ = new QSpinBox(body_widget);
    spin_capacidade_->setRange(1, 500);
    spin_capacidade_->setValue(40);
    spin_capacidade_->setSuffix(" alunos");
    form_layout->addRow("Capacidade:", spin_capacidade_);

    txt_recursos_ = new QLineEdit(body_widget);
    txt_recursos_->setPlaceholderText("Ex: Projetor, Ar Condicionado, Lousa");
    form_layout->addRow("Recursos Adicionais:", txt_recursos_);

    body_layout->addLayout(form_layout);
    body_layout->addStretch();
    root_layout->addWidget(wrapInScrollArea(body_widget, this), 1);

    auto* btn_box = new QHBoxLayout();
    btn_box->addStretch();
    auto* btn_cancel = new QPushButton("Cancelar", this);
    btn_cancel->setObjectName("btnSecondary");
    auto* btn_save = new QPushButton(ModernTheme::icon("save_file"), "Salvar Sala", this);
    btn_box->addWidget(btn_cancel);
    btn_box->addWidget(btn_save);
    root_layout->addLayout(btn_box);

    connect(btn_cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btn_save, &QPushButton::clicked, [this]() {
        if (txt_nome_->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Atenção", "Preencha o Nome da Sala.");
            return;
        }
        accept();
    });

    fitWindowToScreen(this, QSize(460, 320));
}

/**
 * @brief Preenche os componentes com os dados da sala informada.
 * @param room Sala com os dados para preenchimento.
 */
void RoomEditorDialog::loadRoom(const model::Room& room) {
    id_ = room.id();
    txt_nome_->setText(QString::fromStdString(room.nome()));

    int idx = combo_tipo_->findData(QString::fromStdString(room.tipo()));
    if (idx >= 0) combo_tipo_->setCurrentIndex(idx);

    spin_capacidade_->setValue(room.capacidade());

    QString recursos_str;
    for (size_t i = 0; i < room.recursos().size(); ++i) {
        if (i > 0) recursos_str += ", ";
        recursos_str += QString::fromStdString(room.recursos()[i]);
    }
    txt_recursos_->setText(recursos_str);
}

/**
 * @brief Retorna uma nova instância de model::Room com os valores preenchidos.
 * @return Objeto Room configurado.
 */
model::Room RoomEditorDialog::getRoom() const {
    model::Room r(id_,
                  txt_nome_->text().trimmed().toStdString(),
                  combo_tipo_->currentData().toString().toStdString(),
                  spin_capacidade_->value());

    std::string recs = txt_recursos_->text().toStdString();
    std::stringstream ss(recs);
    std::string token;
    while (std::getline(ss, token, ',')) {
        size_t first = token.find_first_not_of(" \t");
        size_t last = token.find_last_not_of(" \t");
        if (first != std::string::npos && last != std::string::npos) {
            r.addRecurso(token.substr(first, last - first + 1));
        }
    }
    return r;
}

} // namespace scheduler::ui

