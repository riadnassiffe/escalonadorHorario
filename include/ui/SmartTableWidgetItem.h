/**
 * @file SmartTableWidgetItem.h
 * @brief Item de tabela inteligente com ordenação natural e utilitário de remoção de acentos.
 */

#pragma once

#include <QTableWidgetItem>
#include <QCollator>
#include <QString>

namespace scheduler::ui {

/**
 * @brief Remove acentos e diacríticos de uma string para permitir buscas normalizadas.
 *
 * Converte a string usando decomposição canônica (KD) e descarta caracteres
 * combinantes. Exemplo: "Física" -> "Fisica", "Atenção" -> "Atencao".
 *
 * @param str Texto original com potenciais acentos e diacríticos.
 * @return QString Texto resultante sem acentos ou caracteres diacríticos.
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
 * @class SmartTableWidgetItem
 * @brief Especialização de QTableWidgetItem com ordenação natural inteligente (alfabética e numérica).
 *
 * Utiliza QCollator com modo numérico ativado e insensibilidade a maiúsculas/minúsculas,
 * permitindo que "Sala 2" ordene antes de "Sala 10" e strings compostas ordenem de forma intuitiva.
 */
class SmartTableWidgetItem : public QTableWidgetItem {
public:
    /**
     * @brief Constrói o item com o texto especificado.
     * @param text Texto exibido na célula da tabela.
     */
    explicit SmartTableWidgetItem(const QString& text)
        : QTableWidgetItem(text) {}

    /**
     * @brief Constrói o item com o texto e o tipo especificados.
     * @param text Texto exibido na célula da tabela.
     * @param type Tipo do item de tabela Qt.
     */
    SmartTableWidgetItem(const QString& text, int type)
        : QTableWidgetItem(text, type) {}

    /**
     * @brief Sobrecarga do operador menor que (<) para ordenação natural.
     * @param other Outro item de tabela a ser comparado.
     * @return true se este item antecede `other` na ordem natural; false caso contrário.
     */
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

