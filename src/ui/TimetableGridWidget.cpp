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

TimetableGridWidget::TimetableGridWidget(QWidget* parent)
    : QTableWidget(parent) {
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setDragEnabled(false); // Usamos arrasto manual customizado para controle total do visual e dados
}

void TimetableGridWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        drag_start_pos_ = event->pos();
    }
    QTableWidget::mousePressEvent(event);
}

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

    QString aula_id = item->data(Qt::UserRole).toString();
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
    QPixmap pixmap(230, 52);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor bg_color = is_dark ? QColor(30, 58, 138, 235) : QColor(37, 99, 235, 235);
    QColor border_color = is_dark ? QColor("#60a5fa") : QColor("#93c5fd");
    painter.setBrush(bg_color);
    painter.setPen(QPen(border_color, 1.5));
    painter.drawRoundedRect(QRect(1, 1, 228, 50), 6, 6);

    painter.setPen(Qt::white);
    QFont f = painter.font();
    f.setBold(true);
    f.setPointSize(9);
    painter.setFont(f);

    QString title = item->text().section('\n', 0, 0);
    painter.drawText(QRect(10, 6, 210, 18), Qt::AlignLeft | Qt::AlignVCenter, title);

    f.setBold(false);
    f.setPointSize(8);
    painter.setFont(f);
    painter.setPen(QColor("#dbeafe"));

    QString sub = item->text().section('\n', 1, 1);
    painter.drawText(QRect(10, 26, 210, 18), Qt::AlignLeft | Qt::AlignVCenter, sub);
    painter.end();

    drag->setPixmap(pixmap);
    drag->setHotSpot(QPoint(pixmap.width() / 2, pixmap.height() / 2));

    drag->exec(Qt::MoveAction | Qt::CopyAction);
}

void TimetableGridWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat("application/x-aula-id")) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

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
