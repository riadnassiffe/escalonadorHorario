/**
 * @file TimetableGridWidget.cpp
 * @brief Implementação dos eventos de arrastar e soltar da grade horária semanal.
 */

#include "ui/TimetableGridWidget.h"
#include "ui/ModernTheme.h"
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDrag>
#include <QMimeData>
#include <QPainter>
#include <QApplication>

namespace scheduler::ui {

/**
 * @brief Construtor da tabela configurando suporte a recepção de arrasto.
 * @param parent Widget pai na hierarquia Qt.
 */
TimetableGridWidget::TimetableGridWidget(QWidget* parent)
    : QTableWidget(parent) {
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setDragEnabled(false); // Usamos arrasto manual customizado para controle total do visual e dados
}

/**
 * @brief Registra a posição inicial do ponteiro quando o botão esquerdo é pressionado.
 * @param event Evento do mouse.
 */
void TimetableGridWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        drag_start_pos_ = event->pos();

        auto* item = itemAt(event->pos());
        if (item) {
            QStringList all_ids = item->data(Qt::UserRole + 3).toStringList();
            QString clicked_id;
            if (all_ids.size() > 1) {
                QRect cell_rect = visualItemRect(item);
                int rel_y = event->pos().y() - cell_rect.top();
                int idx = std::clamp((rel_y * static_cast<int>(all_ids.size())) / std::max(1, cell_rect.height()), 0, static_cast<int>(all_ids.size()) - 1);
                clicked_id = all_ids[idx];
            } else {
                clicked_id = item->data(Qt::UserRole).toString();
            }

            if (!clicked_id.isEmpty()) {
                emit lessonSelected(clicked_id);
            }
        }
    }
    QTableWidget::mousePressEvent(event);
}

/**
 * @brief Inicia o arrasto (QDrag) com visualização em miniatura após deslocamento mínimo.
 * @param event Evento de movimento do mouse.
 */
void TimetableGridWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!(event->buttons() & Qt::LeftButton)) {
        QTableWidget::mouseMoveEvent(event);
        return;
    }
    if ((event->pos() - drag_start_pos_).manhattanLength() < QApplication::startDragDistance()) {
        QTableWidget::mouseMoveEvent(event);
        return;
    }

    auto* item = itemAt(drag_start_pos_);
    if (!item) {
        QTableWidget::mouseMoveEvent(event);
        return;
    }

    QStringList all_ids = item->data(Qt::UserRole + 3).toStringList();
    QString aula_id;
    int aula_idx = 0;
    if (all_ids.size() > 1) {
        QRect cell_rect = visualItemRect(item);
        int rel_y = drag_start_pos_.y() - cell_rect.top();
        aula_idx = std::clamp((rel_y * static_cast<int>(all_ids.size())) / std::max(1, cell_rect.height()), 0, static_cast<int>(all_ids.size()) - 1);
        aula_id = all_ids[aula_idx];
    } else {
        aula_id = item->data(Qt::UserRole).toString();
    }

    if (aula_id.isEmpty()) {
        QTableWidget::mouseMoveEvent(event);
        return;
    }

    auto* drag = new QDrag(this);
    auto* mime = new QMimeData();
    mime->setData("application/x-aula-id", aula_id.toUtf8());
    mime->setText(aula_id);
    drag->setMimeData(mime);

    // Miniatura elegante sob o cursor do mouse
    bool is_dark = ModernTheme::isDarkTheme();
    bool has_conflict = item->text().contains("⚠️");
    QPixmap pixmap(240, 56);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor bg_color = is_dark ? QColor(30, 58, 138, 235) : QColor(37, 99, 235, 235);
    QColor border_color = is_dark ? QColor("#60a5fa") : QColor("#93c5fd");
    if (has_conflict) {
        bg_color = is_dark ? QColor(153, 27, 27, 235) : QColor(220, 38, 38, 235);
        border_color = is_dark ? QColor("#fca5a5") : QColor("#ef4444");
    }
    painter.setBrush(bg_color);
    painter.setPen(QPen(border_color, 1.5));
    painter.drawRoundedRect(QRect(1, 1, 238, 54), 6, 6);

    painter.setPen(Qt::white);
    QFont f = painter.font();
    f.setBold(true);
    f.setPointSize(9);
    painter.setFont(f);

    QString full_text = item->text();
    QStringList blocks = full_text.split("───────────────");
    QString block_text = (aula_idx >= 0 && aula_idx < blocks.size()) ? blocks[aula_idx].trimmed() : full_text;

    QString title = block_text.section('\n', 0, 0).trimmed();
    painter.drawText(QRect(10, 6, 220, 20), Qt::AlignLeft | Qt::AlignVCenter, title);

    f.setBold(false);
    f.setPointSize(8);
    painter.setFont(f);
    painter.setPen(QColor("#dbeafe"));

    QString sub = block_text.section('\n', 1, 1).trimmed();
    painter.drawText(QRect(10, 28, 220, 20), Qt::AlignLeft | Qt::AlignVCenter, sub);
    painter.end();

    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));

    drag->exec(Qt::MoveAction | Qt::CopyAction);
}

/**
 * @brief Aceita o evento de entrada se o mime contiver dados de identificador de aula.
 * @param event Evento de entrada de arrasto.
 */
void TimetableGridWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat("application/x-aula-id")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

/**
 * @brief Valida o movimento de arrasto sobre as células da grade, impedindo soltar sobre intervalos.
 * @param event Evento de movimento de arrasto.
 */
void TimetableGridWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasFormat("application/x-aula-id")) {
        auto* target_item = itemAt(event->position().toPoint());
        if (target_item) {
            QString text = target_item->text();
            // Bloqueia soltar sobre intervalos ou dias não letivos
            if (text.startsWith("☕") || text == "—") {
                event->ignore();
                return;
            }
        }
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

/**
 * @brief Extrai a linha e coluna da célula receptora e emite lessonDropRequested.
 * @param event Evento de soltura de arrasto.
 */
void TimetableGridWidget::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasFormat("application/x-aula-id")) {
        QString aula_id = QString::fromUtf8(event->mimeData()->data("application/x-aula-id"));
        auto pos = event->position().toPoint();
        int target_row = rowAt(pos.y());
        int target_col = columnAt(pos.x());
        if (target_row >= 0 && target_col >= 0) {
            emit lessonDropRequested(aula_id, target_row, target_col);
            event->acceptProposedAction();
            return;
        }
    }
    event->ignore();
}

} // namespace scheduler::ui

