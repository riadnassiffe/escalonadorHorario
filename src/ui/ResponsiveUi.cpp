/**
 * @file ResponsiveUi.cpp
 * @brief Implementação dos utilitários de layout responsivo e ajuste de janelas à tela.
 */

#include "ui/ResponsiveUi.h"
#include <QDialog>
#include <QGuiApplication>
#include <QScreen>
#include <algorithm>

namespace scheduler::ui {

FlowLayout::FlowLayout(QWidget* parent, int margin, int hSpacing, int vSpacing)
    : QLayout(parent), hSpace_(hSpacing), vSpace_(vSpacing) {
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout() {
    for (auto* item : itemList_) {
        delete item;
    }
    itemList_.clear();
}

void FlowLayout::addItem(QLayoutItem* item) {
    itemList_.push_back(item);
}

int FlowLayout::horizontalSpacing() const {
    if (hSpace_ >= 0) {
        return hSpace_;
    }
    return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
}

int FlowLayout::verticalSpacing() const {
    if (vSpace_ >= 0) {
        return vSpace_;
    }
    return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
}

int FlowLayout::count() const {
    return static_cast<int>(itemList_.size());
}

QLayoutItem* FlowLayout::itemAt(int index) const {
    if (index < 0 || index >= static_cast<int>(itemList_.size())) {
        return nullptr;
    }
    return itemList_[static_cast<size_t>(index)];
}

QLayoutItem* FlowLayout::takeAt(int index) {
    if (index < 0 || index >= static_cast<int>(itemList_.size())) {
        return nullptr;
    }
    auto* item = itemList_[static_cast<size_t>(index)];
    itemList_.erase(itemList_.begin() + index);
    return item;
}

Qt::Orientations FlowLayout::expandingDirections() const {
    return {};
}

bool FlowLayout::hasHeightForWidth() const {
    return true;
}

int FlowLayout::heightForWidth(int width) const {
    return doLayout(QRect(0, 0, width, 0), true);
}

void FlowLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const {
    QSize hint(0, 0);
    int left = 0, top = 0, right = 0, bottom = 0;
    getContentsMargins(&left, &top, &right, &bottom);
    const int spaceX = std::max(0, horizontalSpacing());
    int totalW = 0;
    int maxH = 0;
    int visibleCount = 0;
    for (const auto* item : itemList_) {
        if (item->widget() && item->widget()->isHidden()) {
            continue;
        }
        const QSize sz = item->sizeHint();
        if (visibleCount > 0) {
            totalW += spaceX;
        }
        totalW += sz.width();
        maxH = std::max(maxH, sz.height());
        visibleCount++;
    }
    hint = QSize(totalW + left + right, maxH + top + bottom);
    return hint.expandedTo(minimumSize());
}

QSize FlowLayout::minimumSize() const {
    QSize size;
    for (const auto* item : itemList_) {
        if (item->widget() && item->widget()->isHidden()) {
            continue;
        }
        size = size.expandedTo(item->minimumSize());
    }
    const QMargins margins = contentsMargins();
    size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    return size;
}

int FlowLayout::doLayout(const QRect& rect, bool testOnly) const {
    int left = 0, top = 0, right = 0, bottom = 0;
    getContentsMargins(&left, &top, &right, &bottom);
    const QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
    int x = effectiveRect.x();
    int y = effectiveRect.y();
    int lineHeight = 0;

    for (auto* item : itemList_) {
        const QWidget* wid = item->widget();
        if (wid && wid->isHidden()) {
            continue;
        }

        int spaceX = horizontalSpacing();
        if (spaceX == -1 && wid) {
            spaceX = wid->style()->layoutSpacing(
                QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Horizontal);
        }
        if (spaceX < 0) spaceX = 6;

        int spaceY = verticalSpacing();
        if (spaceY == -1 && wid) {
            spaceY = wid->style()->layoutSpacing(
                QSizePolicy::PushButton, QSizePolicy::PushButton, Qt::Vertical);
        }
        if (spaceY < 0) spaceY = 6;

        const QSize itemSize = item->sizeHint();
        int nextX = x + itemSize.width() + spaceX;
        if (nextX - spaceX > effectiveRect.right() && lineHeight > 0) {
            x = effectiveRect.x();
            y = y + lineHeight + spaceY;
            nextX = x + itemSize.width() + spaceX;
            lineHeight = 0;
        }

        if (!testOnly) {
            const int targetW = std::min(itemSize.width(), std::max(0, effectiveRect.width()));
            item->setGeometry(QRect(QPoint(x, y), QSize(targetW, itemSize.height())));
        }

        x = nextX;
        lineHeight = std::max(lineHeight, itemSize.height());
    }
    return y + lineHeight - rect.y() + bottom;
}

int FlowLayout::smartSpacing(QStyle::PixelMetric pm) const {
    QObject* parentObj = parent();
    if (!parentObj) {
        return -1;
    }
    if (parentObj->isWidgetType()) {
        auto* pw = static_cast<QWidget*>(parentObj);
        return pw->style()->pixelMetric(pm, nullptr, pw);
    }
    return static_cast<QLayout*>(parentObj)->spacing();
}

void fitWindowToScreen(QWidget* window,
                       const QSize& preferred_size,
                       double width_ratio,
                       double height_ratio) {
    if (!window) return;

    QScreen* scr = window->screen();
    if (!scr && window->parentWidget()) {
        scr = window->parentWidget()->screen();
    }
    if (!scr) {
        scr = QGuiApplication::primaryScreen();
    }
    if (!scr) {
        window->resize(preferred_size);
        return;
    }

    const QRect avail = scr->availableGeometry();
    const int max_w = std::max(320, static_cast<int>(avail.width() * width_ratio));
    const int max_h = std::max(260, static_cast<int>(avail.height() * height_ratio));

    if (window->minimumWidth() > max_w) {
        window->setMinimumWidth(max_w);
    }
    if (window->minimumHeight() > max_h) {
        window->setMinimumHeight(max_h);
    }

    const int target_w = std::clamp(preferred_size.width(), window->minimumWidth(), max_w);
    const int target_h = std::clamp(preferred_size.height(), window->minimumHeight(), max_h);
    window->resize(target_w, target_h);

    if (qobject_cast<QDialog*>(window) || window->parentWidget()) {
        QPoint center = avail.center();
        if (window->parentWidget() && window->parentWidget()->isVisible()) {
            center = window->parentWidget()->frameGeometry().center();
        }
        int x = center.x() - target_w / 2;
        int y = center.y() - target_h / 2;
        x = std::clamp(x, avail.left(), std::max(avail.left(), avail.right() - target_w + 1));
        y = std::clamp(y, avail.top(), std::max(avail.top(), avail.bottom() - target_h + 1));
        window->move(x, y);
    }
}

QScrollArea* wrapInScrollArea(QWidget* content, QWidget* parent) {
    auto* scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    if (content) {
        scroll->setWidget(content);
    }
    return scroll;
}

void makeComboResponsive(QComboBox* combo, int min_chars, int min_width) {
    if (!combo) return;
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(min_chars);
    combo->setMinimumWidth(min_width);
    combo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

} // namespace scheduler::ui
