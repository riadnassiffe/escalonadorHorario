#pragma once

#include "model/InstitutionData.h"
#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>

namespace scheduler::ui {

/**
 * @brief Diálogo moderno para alterar a sala de aula de uma aula alocada na grade semanal.
 * Permite selecionar entre as salas disponíveis, exibe ocupação em tempo real e suporta troca de salas (Swap).
 */
class ChangeRoomDialog : public QDialog {
    Q_OBJECT

public:
    ChangeRoomDialog(model::InstitutionData& data,
                     const std::string& aula_id,
                     QWidget* parent = nullptr);

    std::string selectedRoomId() const;
    bool isSwapRequested() const { return swap_requested_; }
    bool isPinned() const;
    std::string conflictingAulaId() const { return conflicting_aula_id_; }

private slots:
    void onRoomChanged(int index);
    void onConfirmClicked();
    void onSwapClicked();

private:
    void setupUi();
    void updateRoomStatus();

    model::InstitutionData& data_;
    std::string aula_id_;

    QComboBox* combo_salas_{nullptr};
    QLabel* lbl_status_{nullptr};
    QCheckBox* chk_pin_{nullptr};
    QPushButton* btn_confirm_{nullptr};
    QPushButton* btn_swap_{nullptr};

    std::string conflicting_aula_id_;
    bool swap_requested_{false};
};

} // namespace scheduler::ui
