#pragma once

#include <QString>
#include <QApplication>

namespace scheduler::ui {

class ModernTheme {
public:
    enum class ThemeMode {
        SystemKDE = 0,
        Breeze = 1,
        Fusion = 2,
        ModernDark = 3
    };

    static void applyTheme(QApplication& app);
    static void setThemeMode(QApplication& app, ThemeMode mode);
    static ThemeMode currentThemeMode();
    static QString getStyleSheet(ThemeMode mode = ThemeMode::SystemKDE);
    static bool isDarkTheme();
};

} // namespace scheduler::ui
