#pragma once

#include "model/Room.h"
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>

namespace scheduler::ui {

class RoomEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit RoomEditorDialog(QWidget* parent = nullptr,
                              const model::Room* room = nullptr,
                              const std::string& default_id = "");

    model::Room getRoom() const;

private:
    void setupUi();
    void loadRoom(const model::Room& room);

    std::string id_{"S01"};
    QLineEdit* txt_nome_{nullptr};
    QComboBox* combo_tipo_{nullptr};
    QSpinBox* spin_capacidade_{nullptr};
    QLineEdit* txt_recursos_{nullptr};
};

} // namespace scheduler::ui
