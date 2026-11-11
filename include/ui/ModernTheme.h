/**
 * @file ModernTheme.h
 * @brief Gerenciador de temas visuais e estilos da aplicação Qt.
 */

#pragma once

#include <QString>
#include <QApplication>
#include <QIcon>

namespace scheduler::ui {

/**
 * @class ModernTheme
 * @brief Gerenciador responsável por aplicar temas, estilos e folhas de estilo (QSS).
 *
 * Suporta temas nativos do KDE/Sistema, Breeze, Fusion e um tema ModernDark customizado.
 */
class ModernTheme {
public:
    /**
     * @enum ThemeMode
     * @brief Modos de tema visual suportados pela interface.
     */
    enum class ThemeMode {
        SystemKDE = 0,  ///< Padrão nativo do sistema/KDE Plasma.
        Breeze = 1,     ///< Estilo Breeze do KDE.
        Fusion = 2,     ///< Estilo Fusion multiplataforma do Qt.
        ModernDark = 3  ///< Tema escuro moderno com visual personalizado.
    };

    /**
     * @brief Aplica o tema padrão inicial à aplicação.
     * @param app Referência para a instância de QApplication.
     */
    static void applyTheme(QApplication& app);

    /**
     * @brief Define e aplica o modo de tema selecionado à aplicação.
     * @param app Referência para a instância de QApplication.
     * @param mode Modo de tema a ser aplicado (ThemeMode).
     */
    static void setThemeMode(QApplication& app, ThemeMode mode);

    /**
     * @brief Obtém o modo de tema atualmente ativo.
     * @return Modo de tema corrente (ThemeMode).
     */
    static ThemeMode currentThemeMode();

    /**
     * @brief Retorna a folha de estilo (CSS/QSS) correspondente ao modo de tema.
     * @param mode Modo de tema desejado (padrão: ThemeMode::SystemKDE).
     * @return QString contendo a folha de estilo QSS.
     */
    static QString getStyleSheet(ThemeMode mode = ThemeMode::SystemKDE);

    /**
     * @brief Verifica se o tema corrente é escuro (Dark Mode).
     * @return true se o tema atual for escuro; false se for claro.
     */
    static bool isDarkTheme();

    /**
     * @brief Resolve o caminho do arquivo de imagem na pasta img/ do projeto.
     * @param filename Nome do arquivo na pasta img/ (ex: "calendar.svg" ou "calendar").
     * @return Caminho encontrado para o arquivo na pasta img/.
     */
    static QString iconPath(const QString& filename);

    /**
     * @brief Carrega um QIcon a partir da pasta img/ do projeto.
     * @param name Nome do ícone na pasta img/ (ex: "calendar" ou "calendar.svg").
     * @return QIcon carregado.
     */
    static QIcon icon(const QString& name);
};

} // namespace scheduler::ui

