/**
 * @file ModernTheme.cpp
 * @brief Implementação dos métodos de gerenciamento de temas e folhas de estilo QSS.
 */

#include "ui/ModernTheme.h"
#include <QFont>
#include <QStyleFactory>
#include <QStyle>

namespace scheduler::ui {

static ModernTheme::ThemeMode s_current_theme_mode = ModernTheme::ThemeMode::SystemKDE;

/**
 * @brief Aplica o tema padrão inicial à aplicação.
 * @param app Referência para QApplication.
 */
void ModernTheme::applyTheme(QApplication& app) {
    setThemeMode(app, ThemeMode::SystemKDE);
}

/**
 * @brief Obtém o modo de tema atualmente selecionado.
 * @return Modo de tema ativo.
 */
ModernTheme::ThemeMode ModernTheme::currentThemeMode() {
    return s_current_theme_mode;
}

/**
 * @brief Informa se o tema atual é escuro.
 * @return true se o tema atual for escuro; false caso contrário.
 */
bool ModernTheme::isDarkTheme() {
    if (s_current_theme_mode == ThemeMode::ModernDark) {
        return true;
    }
    if (qApp) {
        return (qApp->palette().color(QPalette::Window).lightness() < 128);
    }
    return false;
}

/**
 * @brief Configura o estilo e a folha de estilo da aplicação conforme o modo escolhido.
 * @param app Instância de QApplication.
 * @param mode Modo de tema a aplicar.
 */
void ModernTheme::setThemeMode(QApplication& app, ThemeMode mode) {
    s_current_theme_mode = mode;

    if (mode == ThemeMode::ModernDark) {
        if (QStyleFactory::keys().contains("Fusion", Qt::CaseInsensitive)) {
            app.setStyle(QStyleFactory::create("Fusion"));
        }
        QFont default_font("Segoe UI", 10);
        default_font.setStyleHint(QFont::SansSerif);
        app.setFont(default_font);
        app.setStyleSheet(getStyleSheet(ThemeMode::ModernDark));
    } else if (mode == ThemeMode::Breeze) {
        if (QStyleFactory::keys().contains("Breeze", Qt::CaseInsensitive)) {
            app.setStyle(QStyleFactory::create("Breeze"));
        }
        app.setFont(QFont()); // restaura fonte padrão do sistema KDE
        app.setStyleSheet(getStyleSheet(ThemeMode::Breeze));
    } else if (mode == ThemeMode::Fusion) {
        if (QStyleFactory::keys().contains("Fusion", Qt::CaseInsensitive)) {
            app.setStyle(QStyleFactory::create("Fusion"));
        }
        app.setFont(QFont());
        app.setStyleSheet(getStyleSheet(ThemeMode::Fusion));
    } else {
        // ThemeMode::SystemKDE (Padrão do Sistema / KDE)
        // No KDE Plasma 6 o estilo nativo ativo costuma ser 'union' ou 'breeze'.
        QString current_style = app.style() ? app.style()->objectName().toLower() : "";
        if (current_style != "union" && current_style != "breeze") {
            if (QStyleFactory::keys().contains("Breeze", Qt::CaseInsensitive)) {
                app.setStyle(QStyleFactory::create("Breeze"));
            }
        }
        app.setFont(QFont()); // respeita a fonte configurada pelo usuário no KDE
        app.setStyleSheet(getStyleSheet(ThemeMode::SystemKDE));
    }
}

/**
 * @brief Gera a folha de estilo QSS completa para o tema escuro moderno.
 * @return QString com o código QSS do tema escuro.
 */
static QString getDarkStyleSheet() {
    return QString(R"(
        QWidget {
            background-color: #0f172a;
            color: #f8fafc;
            font-family: 'Segoe UI', -apple-system, BlinkMacSystemFont, Roboto, sans-serif;
            font-size: 13px;
        }

        QMainWindow {
            background-color: #0b1120;
        }

        QToolBar {
            background-color: #1e293b;
            border-bottom: 1px solid #334155;
            spacing: 8px;
            padding: 6px 12px;
        }

        QToolButton {
            background-color: #334155;
            color: #f8fafc;
            border: 1px solid #475569;
            border-radius: 6px;
            padding: 6px 14px;
            font-weight: 500;
        }

        QToolButton:hover {
            background-color: #475569;
            border-color: #38bdf8;
        }

        QToolButton:pressed {
            background-color: #1e293b;
        }

        QToolButton#btnOptimize {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2563eb, stop:1 #38bdf8);
            color: #ffffff;
            font-weight: bold;
            font-size: 14px;
            border: none;
            padding: 8px 20px;
        }

        QToolButton#btnOptimize:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1d4ed8, stop:1 #0284c7);
        }

        QTabWidget::pane {
            border: 1px solid #334155;
            background-color: #1e293b;
            border-radius: 8px;
            top: -1px;
        }

        QTabBar::tab {
            background-color: #0f172a;
            color: #94a3b8;
            padding: 10px 18px;
            border-top-left-radius: 6px;
            border-top-right-radius: 6px;
            border: 1px solid #334155;
            border-bottom: none;
            margin-right: 4px;
            font-weight: 600;
        }

        QTabBar::tab:selected {
            background-color: #1e293b;
            color: #38bdf8;
            border-top: 3px solid #38bdf8;
        }

        QTabBar::tab:hover:!selected {
            background-color: #1a2538;
            color: #f8fafc;
        }

        QTableWidget, QTableView {
            background-color: #162032;
            alternate-background-color: #1a273e;
            gridline-color: #334155;
            border: 1px solid #334155;
            border-radius: 6px;
            selection-background-color: #2563eb;
            selection-color: #ffffff;
        }

        QHeaderView::section {
            background-color: #1e293b;
            color: #38bdf8;
            padding: 8px 12px;
            font-weight: 600;
            border: 1px solid #334155;
        }

        QHeaderView::section:hover {
            background-color: #24344d;
            color: #7dd3fc;
        }

        QPushButton {
            background-color: #2563eb;
            color: #ffffff;
            border: none;
            border-radius: 6px;
            padding: 8px 16px;
            font-weight: 600;
        }

        QPushButton:hover {
            background-color: #1d4ed8;
        }

        QPushButton:pressed {
            background-color: #1e40af;
        }

        QPushButton#btnSecondary {
            background-color: #334155;
            color: #f8fafc;
            border: 1px solid #475569;
        }

        QPushButton#btnSecondary:hover {
            background-color: #475569;
            border-color: #38bdf8;
        }

        QPushButton#btnDanger {
            background-color: #ef4444;
        }

        QPushButton#btnDanger:hover {
            background-color: #dc2626;
        }

        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTimeEdit {
            background-color: #0f172a;
            color: #f8fafc;
            border: 1px solid #475569;
            border-radius: 6px;
            padding: 5px 8px;
            min-height: 20px;
        }

        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QTimeEdit:focus {
            border-color: #38bdf8;
            background-color: #131c2e;
        }

        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 26px;
            border-left: 1px solid #334155;
            background-color: #1e293b;
            border-top-right-radius: 5px;
            border-bottom-right-radius: 5px;
        }

        QComboBox::down-arrow {
            width: 0;
            height: 0;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #94a3b8;
            margin: auto;
        }

        QComboBox::down-arrow:hover {
            border-top-color: #38bdf8;
        }

        QComboBox QAbstractItemView {
            background-color: #1e293b;
            color: #f8fafc;
            selection-background-color: #2563eb;
            selection-color: #ffffff;
            border: 1px solid #475569;
            border-radius: 6px;
            padding: 4px;
            outline: none;
        }

        QComboBox QAbstractItemView::item {
            padding: 6px 10px;
            border-radius: 4px;
            min-height: 24px;
        }

        QComboBox QAbstractItemView::item:hover {
            background-color: #334155;
        }

        QComboBox QAbstractItemView::item:selected {
            background-color: #2563eb;
        }

        QSpinBox::up-button, QDoubleSpinBox::up-button, QTimeEdit::up-button {
            subcontrol-origin: border;
            subcontrol-position: top right;
            width: 18px;
            border-left: 1px solid #334155;
            border-bottom: 1px solid #334155;
            background-color: #1e293b;
            border-top-right-radius: 5px;
        }

        QSpinBox::down-button, QDoubleSpinBox::down-button, QTimeEdit::down-button {
            subcontrol-origin: border;
            subcontrol-position: bottom right;
            width: 18px;
            border-left: 1px solid #334155;
            background-color: #1e293b;
            border-bottom-right-radius: 5px;
        }

        QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover, QTimeEdit::up-button:hover,
        QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover, QTimeEdit::down-button:hover {
            background-color: #334155;
        }

        QGroupBox {
            border: 1px solid #334155;
            border-radius: 8px;
            margin-top: 18px;
            padding-top: 14px;
            font-weight: 600;
            color: #38bdf8;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 12px;
            padding: 0 6px;
        }

        QSlider::groove:horizontal {
            height: 6px;
            background: #334155;
            border-radius: 3px;
        }

        QSlider::sub-page:horizontal {
            background: #2563eb;
            border-radius: 3px;
        }

        QSlider::handle:horizontal {
            background: #38bdf8;
            border: 2px solid #ffffff;
            width: 16px;
            margin-top: -5px;
            margin-bottom: -5px;
            border-radius: 8px;
        }

        QScrollBar:vertical {
            background: #0f172a;
            width: 12px;
            margin: 0;
            border-radius: 6px;
        }

        QScrollBar::handle:vertical {
            background: #475569;
            min-height: 25px;
            border-radius: 6px;
        }

        QScrollBar::handle:vertical:hover {
            background: #64748b;
        }

        QScrollBar:horizontal {
            background: #0f172a;
            height: 12px;
            margin: 0;
            border-radius: 6px;
        }

        QScrollBar::handle:horizontal {
            background: #475569;
            min-width: 25px;
            border-radius: 6px;
        }

        QScrollBar::handle:horizontal:hover {
            background: #64748b;
        }

        QScrollBar::add-line, QScrollBar::sub-line {
            width: 0;
            height: 0;
        }

        QStatusBar {
            background-color: #0b1120;
            color: #94a3b8;
            border-top: 1px solid #1e293b;
        }

        QTextEdit, QPlainTextEdit {
            background-color: #0a0f1d;
            border: 1px solid #334155;
            color: #e2e8f0;
            font-family: 'Consolas', 'Fira Code', monospace;
            font-size: 12px;
        }
    )");
/**
 * @brief Retorna a folha de estilo QSS correspondente ao modo de tema configurado.
 * @param mode Modo de tema desejado.
 * @return QString contendo a folha de estilo QSS.
 */
QString ModernTheme::getStyleSheet(ThemeMode mode) {
    if (mode == ThemeMode::ModernDark) {
        return getDarkStyleSheet();
    }

    // Estilos complementares semânticos que respeitam a paleta do sistema (QPalette)
    // e preservam a renderização nativa de controles do KDE Plasma / Breeze.
    return QString(R"(
        /* Botão principal de destaque da barra de ferramentas */
        QToolButton#btnOptimize {
            background-color: palette(highlight);
            color: palette(highlighted-text);
            font-weight: bold;
            border-radius: 6px;
            padding: 6px 16px;
        }

        QPushButton#btnDanger {
            background-color: #dc2626;
            color: #ffffff;
            font-weight: bold;
            border-radius: 4px;
            padding: 6px 12px;
        }

        QPushButton#btnDanger:hover {
            background-color: #b91c1c;
        }
    )");
}

} // namespace scheduler::ui
