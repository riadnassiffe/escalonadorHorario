#include "ui/ModernTheme.h"
#include <QFont>
#include <QPalette>

namespace scheduler::ui {

void ModernTheme::applyTheme(QApplication& app) {
    QFont default_font("Segoe UI", 10);
    default_font.setStyleHint(QFont::SansSerif);
    app.setFont(default_font);
    app.setStyleSheet(getStyleSheet());
}

QString ModernTheme::getStyleSheet() {
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
            padding: 6px 10px;
        }

        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QTimeEdit:focus {
            border-color: #38bdf8;
            background-color: #131c2e;
        }

        QComboBox::drop-down {
            border: none;
            width: 24px;
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
            width: 10px;
            margin: 0;
        }

        QScrollBar::handle:vertical {
            background: #334155;
            min-height: 20px;
            border-radius: 5px;
        }

        QScrollBar::handle:vertical:hover {
            background: #475569;
        }

        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
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
}

} // namespace scheduler::ui
