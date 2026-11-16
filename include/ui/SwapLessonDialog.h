/**
 * @file SwapLessonDialog.h
 * @brief Diálogo para troca direta de horários (Swap) entre duas aulas da grade.
 */

#pragma once

#include "model/InstitutionData.h"
#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QTableWidget>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <string>
#include <vector>

namespace scheduler::ui {

/**
 * @class SwapLessonDialog
 * @brief Diálogo moderno para permuta de horários (Swap) entre duas aulas na grade semanal.
 *
 * Permite ao usuário selecionar uma aula de origem (ou utilizar a pré-selecionada)
 * e escolher qualquer outra aula alocada na grade para permutar os horários,
 * apresentando validação em tempo real de choques de turmas, professores e salas.
 */
class SwapLessonDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Construtor do diálogo de permuta de aulas.
     * @param data Referência para o modelo de dados acadêmicos e grade.
     * @param initial_aula_id ID opcional da aula de origem pré-selecionada.
     * @param parent Widget pai na hierarquia Qt.
     */
    SwapLessonDialog(model::InstitutionData& data,
                     const std::string& initial_aula_id = "",
                     QWidget* parent = nullptr);

    /**
     * @brief Retorna o ID da aula de origem.
     */
    std::string aulaOrigemId() const;

    /**
     * @brief Retorna o ID da aula de destino escolhida para a permuta.
     */
    std::string aulaDestinoId() const;

    /**
     * @brief Indica se o usuário optou por fixar (Pin) as duas aulas após a troca.
     */
    bool isPinned() const;

private slots:
    void onOrigemChanged();
    void onFilterDestinoChanged(const QString& query);
    void onDestinoSelectionChanged();
    void onConfirmSwap();

private:
    void setupUi();
    void populateOrigemCombo();
    void populateDestinoTable();
    void updateValidationStatus();
    std::vector<std::string> verificarConflitosTroca(const std::string& aid_a,
                                                     const std::string& aid_b) const;

    model::InstitutionData& data_;
    std::string initial_aula_id_;
    std::string selected_origem_id_;
    std::string selected_destino_id_;

    QComboBox* combo_origem_{nullptr};
    QLabel* lbl_origem_details_{nullptr};

    QLineEdit* search_destino_{nullptr};
    QTableWidget* table_destino_{nullptr};

    QLabel* lbl_validation_status_{nullptr};
    QCheckBox* chk_pinned_{nullptr};
    QPushButton* btn_confirm_{nullptr};
    QPushButton* btn_cancel_{nullptr};
};

} // namespace scheduler::ui
