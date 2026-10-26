/**
 * @file TableSearchSortBar.h
 * @brief Componente de interface para filtragem em tempo real e ordenação de tabelas QTableWidget.
 */

#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QToolButton>
#include <QTableWidget>

namespace scheduler::ui {

/**
 * @class TableSearchSortBar
 * @brief Componente moderno de busca e ordenação para QTableWidget.
 *
 * Permite filtrar registros em tempo real e ordenar alfabeticamente ou numericamente
 * em ordem crescente (A-Z) ou decrescente (Z-A), integrando-se aos cabeçalhos das tabelas.
 */
class TableSearchSortBar : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constrói a barra de busca e ordenação.
     * @param parent Widget pai (opcional).
     */
    explicit TableSearchSortBar(QWidget* parent = nullptr);

    /**
     * @brief Conecta a barra de controle a uma tabela específica.
     * @param table Ponteiro para a QTableWidget alvo.
     * @param entitySingular Nome no singular da entidade (ex: "professor").
     * @param entityPlural Nome no plural da entidade (ex: "professores").
     * @param defaultSortColumn Coluna padrão para ordenação inicial.
     * @param defaultOrder Ordem padrão (Qt::AscendingOrder ou Qt::DescendingOrder).
     */
    void setupWithTable(QTableWidget* table,
                        const QString& entitySingular,
                        const QString& entityPlural,
                        int defaultSortColumn = 1,
                        Qt::SortOrder defaultOrder = Qt::AscendingOrder);

    /**
     * @brief Re-aplica a ordenação e o filtro ativo após atualização dos dados da tabela.
     */
    void reapplyFilterAndSort();

    /**
     * @brief Retorna o texto de busca digitado atualmente.
     * @return QString com a consulta atual.
     */
    QString currentQuery() const;

    /**
     * @brief Retorna o índice lógico da coluna atualmente selecionada para ordenação.
     * @return Índice da coluna na tabela.
     */
    int currentSortColumn() const;

    /**
     * @brief Retorna o sentido de ordenação atual (Ascendente ou Descendente).
     * @return Qt::SortOrder corrente.
     */
    Qt::SortOrder currentSortOrder() const;

public slots:
    /**
     * @brief Aplica filtro sobre as linhas da tabela com base no termo digitado.
     * @param query Texto digitado no campo de busca.
     */
    void applyFilter(const QString& query);

    /**
     * @brief Aplica a ordenação na tabela utilizando a coluna e sentido atuais.
     */
    void applySort();

    /**
     * @brief Alterna o sentido de ordenação entre Crescente e Decrescente.
     */
    void toggleSortOrder();

private slots:
    /**
     * @brief Responde ao clique de cabeçalho de coluna para sincronizar o dropdown e a direção.
     * @param logicalIndex Índice lógico da coluna clicada.
     */
    void onHeaderSectionClicked(int logicalIndex);

private:
    /**
     * @brief Atualiza o badge numérico com a contagem de registros visíveis e totais.
     * @param visibleCount Número de registros que atenderam ao filtro.
     * @param totalCount Número total de linhas da tabela.
     */
    void updateCountLabel(int visibleCount, int totalCount);

    /**
     * @brief Atualiza o texto e o ícone do botão de sentido de ordenação.
     */
    void updateOrderButtonState();

    QTableWidget* table_{nullptr};
    QString entitySingular_{"item"};
    QString entityPlural_{"itens"};

    QLineEdit* search_edit_{nullptr};
    QLabel* lbl_count_{nullptr};
    QComboBox* combo_sort_column_{nullptr};
    QToolButton* btn_sort_order_{nullptr};

    Qt::SortOrder current_sort_order_{Qt::AscendingOrder};
    bool is_syncing_{false};
};

} // namespace scheduler::ui
