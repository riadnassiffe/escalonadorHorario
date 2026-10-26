/**
 * @file TimetableGridWidget.h
 * @brief Widget de grade semanal especializado com suporte a arrastar e soltar (Drag and Drop).
 */

#pragma once

#include <QTableWidget>
#include <QPoint>

namespace scheduler::ui {

/**
 * @class TimetableGridWidget
 * @brief Tabela especializada para exibição da grade semanal com suporte a Drag & Drop nativo.
 *
 * Permite ao usuário clicar e arrastar uma aula alocada para qualquer outro slot/dia da grade,
 * emitindo sinal para verificação de viabilidade e confirmação de reposicionamento ou troca.
 */
class TimetableGridWidget : public QTableWidget {
    Q_OBJECT

public:
    /**
     * @brief Construtor da grade horária interativa.
     * @param parent Widget pai na hierarquia Qt.
     */
    explicit TimetableGridWidget(QWidget* parent = nullptr);

signals:
    /**
     * @brief Sinal emitido quando uma aula é arrastada e solta em uma célula da grade.
     * @param aula_id ID da aula que foi movida.
     * @param target_row Linha da célula de destino.
     * @param target_col Coluna da célula de destino.
     */
    void lessonDropRequested(const QString& aula_id, int target_row, int target_col);

protected:
    /**
     * @brief Trata o clique inicial do mouse para registrar o ponto de início de um potencial arrasto.
     * @param event Evento do mouse.
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * @brief Trata a movimentação do mouse e dispara a operação QDrag com miniatura gráfica.
     * @param event Evento de movimento do mouse.
     */
    void mouseMoveEvent(QMouseEvent* event) override;

    /**
     * @brief Valida se os dados arrastados sobre a tabela contêm o tipo mime de aula.
     * @param event Evento de entrada de arrasto.
     */
    void dragEnterEvent(QDragEnterEvent* event) override;

    /**
     * @brief Controla se a célula sob o cursor é um destino aceitável (bloqueia intervalos e dias inativos).
     * @param event Evento de movimentação durante arrasto.
     */
    void dragMoveEvent(QDragMoveEvent* event) override;

    /**
     * @brief Processa o evento de soltar o item e emite lessonDropRequested com a posição de destino.
     * @param event Evento de soltura (drop).
     */
    void dropEvent(QDropEvent* event) override;

private:
    QPoint drag_start_pos_;
};

} // namespace scheduler::ui

