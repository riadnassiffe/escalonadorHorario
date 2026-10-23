#pragma once

#include <QTableWidget>
#include <QPoint>

namespace scheduler::ui {

/**
 * @brief Tabela especializada para exibição da grade semanal com suporte a Drag & Drop nativo.
 * Permite ao usuário clicar e arrastar uma aula alocada para qualquer outro slot/dia da grade.
 */
class TimetableGridWidget : public QTableWidget {
    Q_OBJECT

public:
    explicit TimetableGridWidget(QWidget* parent = nullptr);

signals:
    void lessonDropRequested(const QString& aula_id, int target_row, int target_col);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    QPoint drag_start_pos_;
};

} // namespace scheduler::ui
