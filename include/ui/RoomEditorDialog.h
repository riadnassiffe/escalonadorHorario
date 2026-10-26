/**
 * @file RoomEditorDialog.h
 * @brief Diálogo para cadastro e edição de salas de aula e laboratórios.
 */

#pragma once

#include "model/Room.h"
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>

namespace scheduler::ui {

/**
 * @class RoomEditorDialog
 * @brief Diálogo gráfico para inserção e edição de dados de salas de aula e laboratórios.
 *
 * Permite configurar identificador, nome, tipo de ambiente (teórica, laboratório, especial),
 * capacidade de alunos e lista de recursos adicionais disponíveis.
 */
class RoomEditorDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constrói a janela de diálogo para criação ou edição de sala.
     * @param parent Widget pai na hierarquia Qt.
     * @param room Ponteiro opcional para sala existente (se nullptr, cria nova sala).
     * @param default_id Identificador sugerido caso seja uma nova sala.
     */
    explicit RoomEditorDialog(QWidget* parent = nullptr,
                              const model::Room* room = nullptr,
                              const std::string& default_id = "");

    /**
     * @brief Constrói e retorna o objeto Room com os valores preenchidos no formulário.
     * @return model::Room configurada.
     */
    model::Room getRoom() const;

private:
    /**
     * @brief Inicializa e posiciona os componentes visuais do formulário.
     */
    void setupUi();

    /**
     * @brief Carrega os dados de uma sala existente nos campos do formulário.
     * @param room Objeto Room com os dados a serem carregados.
     */
    void loadRoom(const model::Room& room);

    std::string id_{"S01"};
    QLineEdit* txt_nome_{nullptr};
    QComboBox* combo_tipo_{nullptr};
    QSpinBox* spin_capacidade_{nullptr};
    QLineEdit* txt_recursos_{nullptr};
};

} // namespace scheduler::ui

