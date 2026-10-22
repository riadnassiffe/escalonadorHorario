#pragma once

#include <QString>
#include <QApplication>

namespace scheduler::ui {

class ModernTheme {
public:
    static void applyTheme(QApplication& app);
    static QString getStyleSheet();
};

} // namespace scheduler::ui
