#pragma once

#include <QTableWidgetItem>
#include <QCollator>
#include <QString>

namespace scheduler::ui {

/**
 * @brief Remove acentos e diacríticos para busca insensível a acentuação.
 * Exemplo: "Física" -> "Fisica", "Atenção" -> "Atencao".
 */
inline QString removeAccents(const QString& str) {
    QString normalized = str.normalized(QString::NormalizationForm_KD);
    QString result;
    result.reserve(normalized.size());
    for (const QChar& ch : normalized) {
        if (ch.combiningClass() == 0) {
            result.append(ch);
        }
    }
    return result;
}

/**
 * @brief QTableWidgetItem com ordenação natural inteligente (alfabética e numérica).
 * Suporta ordenação A-Z/Z-A com respeito a maiúsculas/minúsculas e números embutidos
 * ("Sala 2" antes de "Sala 10", "45 min" antes de "90 min").
 */
class SmartTableWidgetItem : public QTableWidgetItem {
public:
    explicit SmartTableWidgetItem(const QString& text)
        : QTableWidgetItem(text) {}

    SmartTableWidgetItem(const QString& text, int type)
        : QTableWidgetItem(text, type) {}

    bool operator<(const QTableWidgetItem& other) const override {
        thread_local QCollator collator;
        thread_local bool initialized = false;
        if (!initialized) {
            collator.setNumericMode(true);
            collator.setCaseSensitivity(Qt::CaseInsensitive);
            initialized = true;
        }

        return collator.compare(text().trimmed(), other.text().trimmed()) < 0;
    }
};

} // namespace scheduler::ui
