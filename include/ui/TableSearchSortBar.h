#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QToolButton>
#include <QTableWidget>

namespace scheduler::ui {

/**
 * @brief Componente moderno de busca e ordenação para QTableWidget.
 * Permite filtrar registros em tempo real e ordenar alfabeticamente ou numericamente
 * em ordem crescente (A-Z) ou decrescente (Z-A).
 */
class TableSearchSortBar : public QWidget {
    Q_OBJECT

public:
    explicit TableSearchSortBar(QWidget* parent = nullptr);

    /**
     * @brief Conecta a barra de controle a uma tabela específica.
     * @param table Ponteiro para a QTableWidget
     * @param entitySingular Nome no singular da entidade (ex: "professor")
     * @param entityPlural Nome no plural da entidade (ex: "professores")
     * @param defaultSortColumn Coluna padrão para ordenação (ex: 1 para Nome)
     * @param defaultOrder Ordem padrão (Crescente ou Decrescente)
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

    QString currentQuery() const;
    int currentSortColumn() const;
    Qt::SortOrder currentSortOrder() const;

public slots:
    void applyFilter(const QString& query);
    void applySort();
    void toggleSortOrder();

private slots:
    void onHeaderSectionClicked(int logicalIndex);

private:
    void updateCountLabel(int visibleCount, int totalCount);
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
