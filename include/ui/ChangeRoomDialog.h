/**
 * @file ChangeRoomDialog.h
 * @brief Diálogo para alteração manual ou permuta de salas de aula na grade.
 */

#pragma once

#include "model/InstitutionData.h"
#include <QDialog>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>

namespace scheduler::ui {

/**
 * @class ChangeRoomDialog
 * @brief Diálogo moderno para alterar a sala de aula de uma aula alocada na grade semanal.
 *
 * Permite selecionar entre as salas disponíveis, exibe ocupação em tempo real e suporta
 * troca mútua de salas (Swap) ou fixação (Pin) no horário.
 */
class ChangeRoomDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constrói o diálogo para a aula informada.
     * @param data Referência para o modelo de dados acadêmicos.
     * @param aula_id Identificador da aula cuja sala será alterada.
     * @param parent Widget pai na interface Qt.
     */
    ChangeRoomDialog(model::InstitutionData& data,
                     const std::string& aula_id,
                     QWidget* parent = nullptr);

    /**
     * @brief Retorna o identificador da sala selecionada pelo usuário.
     * @return ID da sala (std::string).
     */
    std::string selectedRoomId() const;

    /**
     * @brief Indica se foi solicitada uma permuta (swap) de salas com outra aula em choque.
     * @return true se o usuário clicou em Trocar Salas; false caso contrário.
     */
    bool isSwapRequested() const { return swap_requested_; }

    /**
     * @brief Informa se a opção de fixar a alocação (Pin) foi marcada.
     * @return true se o checkbox de fixação estiver selecionado.
     */
    bool isPinned() const;

    /**
     * @brief Retorna o ID da aula conflitante em caso de choque na sala selecionada.
     * @return ID da aula em choque, ou string vazia se livre.
     */
    std::string conflictingAulaId() const { return conflicting_aula_id_; }

private slots:
    /**
     * @brief Atualiza as informações de disponibilidade e status quando uma sala é escolhida.
     * @param index Novo índice selecionado no QComboBox.
     */
    void onRoomChanged(int index);

    /**
     * @brief Valida conflitos e confirma a alteração de sala.
     */
    void onConfirmClicked();

    /**
     * @brief Dispara a operação de permuta (swap) entre as duas salas em conflito.
     */
    void onSwapClicked();

private:
    /**
     * @brief Constrói e organiza os elementos gráficos do diálogo.
     */
    void setupUi();

    /**
     * @brief Verifica a ocupação da sala atualmente selecionada e atualiza rótulos e botões.
     */
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
