/**
 * @file TableSearchSortBar.cpp
 * @brief Implementação do componente de busca textual e ordenação de tabelas.
 */

#include "ui/TableSearchSortBar.h"
#include "ui/SmartTableWidgetItem.h"
#include <QHBoxLayout>
#include <QHeaderView>

namespace scheduler::ui {

/**
 * @brief Constrói os elementos de interface da barra de busca e ordenação.
 * @param parent Widget pai na hierarquia Qt.
 */
TableSearchSortBar::TableSearchSortBar(QWidget* parent)
    : QWidget(parent) {
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    // Campo de busca
    search_edit_ = new QLineEdit(this);
    search_edit_->setPlaceholderText("🔍 Buscar...");
    search_edit_->setClearButtonEnabled(true);
    search_edit_->setMinimumWidth(180);
    search_edit_->setMaximumWidth(260);
    search_edit_->setToolTip("Digite para filtrar os registros por qualquer campo em tempo real");
    layout->addWidget(search_edit_);

    // Contador de registros encontrados
    lbl_count_ = new QLabel("0 itens", this);
    lbl_count_->setStyleSheet(
        "background-color: palette(base); color: palette(window-text); border: 1px solid palette(mid); "
        "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: 500;");
    layout->addWidget(lbl_count_);

    layout->addSpacing(8);

    // Label de ordenação
    auto* lbl_sort = new QLabel("Ordenar:", this);
    lbl_sort->setStyleSheet("font-weight: 600; font-size: 12px;");
    layout->addWidget(lbl_sort);

    // Dropdown de coluna para ordenação
    combo_sort_column_ = new QComboBox(this);
    combo_sort_column_->setMinimumWidth(120);
    combo_sort_column_->setToolTip("Selecione a coluna desejada para ordenar");
    layout->addWidget(combo_sort_column_);

    // Botão de direção de ordenação (Crescente A-Z / Decrescente Z-A)
    btn_sort_order_ = new QToolButton(this);
    btn_sort_order_->setObjectName("btnSecondary");
    btn_sort_order_->setCursor(Qt::PointingHandCursor);
    btn_sort_order_->setStyleSheet("font-weight: 600; font-size: 11px; padding: 4px 10px;");
    updateOrderButtonState();
    layout->addWidget(btn_sort_order_);

    // Conexões internas
    connect(search_edit_, &QLineEdit::textChanged, this, &TableSearchSortBar::applyFilter);
    connect(combo_sort_column_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TableSearchSortBar::applySort);
    connect(btn_sort_order_, &QToolButton::clicked, this, &TableSearchSortBar::toggleSortOrder);
}

/**
 * @brief Vincula a barra a uma QTableWidget e popula as colunas disponíveis para ordenação.
 * @param table Tabela alvo.
 * @param entitySingular Nome no singular da entidade.
 * @param entityPlural Nome no plural da entidade.
 * @param defaultSortColumn Coluna padrão selecionada.
 * @param defaultOrder Sentido padrão de ordenação.
 */
void TableSearchSortBar::setupWithTable(QTableWidget* table,
                                        const QString& entitySingular,
                                        const QString& entityPlural,
                                        int defaultSortColumn,
                                        Qt::SortOrder defaultOrder) {
    table_ = table;
    entitySingular_ = entitySingular;
    entityPlural_ = entityPlural;
    current_sort_order_ = defaultOrder;

    search_edit_->setPlaceholderText(QString("🔍 Buscar %1...").arg(entityPlural_));

    if (!table_) return;

    table_->setSortingEnabled(true);
    table_->horizontalHeader()->setSortIndicatorShown(true);
    table_->horizontalHeader()->setSectionsClickable(true);

    combo_sort_column_->blockSignals(true);
    combo_sort_column_->clear();
    for (int c = 0; c < table_->columnCount(); ++c) {
        if (table_->isColumnHidden(c)) continue;
        auto* header_item = table_->horizontalHeaderItem(c);
        QString title = header_item ? header_item->text() : QString("Coluna %1").arg(c + 1);
        combo_sort_column_->addItem(title, c);
    }

    int idx_to_select = 0;
    for (int i = 0; i < combo_sort_column_->count(); ++i) {
        if (combo_sort_column_->itemData(i).toInt() == defaultSortColumn) {
            idx_to_select = i;
            break;
        }
    }
    combo_sort_column_->setCurrentIndex(idx_to_select);
    combo_sort_column_->blockSignals(false);

    updateOrderButtonState();

    connect(table_->horizontalHeader(), &QHeaderView::sectionClicked,
            this, &TableSearchSortBar::onHeaderSectionClicked, Qt::UniqueConnection);

    // Aplica a ordenação inicial
    applySort();
    applyFilter(search_edit_->text());
}

/**
 * @brief Re-aplica a ordenação e a busca textual ativa na tabela.
 */
void TableSearchSortBar::reapplyFilterAndSort() {
    if (!table_) return;
    int col = currentSortColumn();
    if (col >= 0) {
        table_->sortByColumn(col, current_sort_order_);
    }
    applyFilter(search_edit_->text());
}

/**
 * @brief Retorna o texto digitado no campo de busca.
 * @return QString com o texto de busca.
 */
QString TableSearchSortBar::currentQuery() const {
    return search_edit_->text();
}

/**
 * @brief Retorna a coluna atualmente selecionada no dropdown para ordenação.
 * @return Índice lógico da coluna.
 */
int TableSearchSortBar::currentSortColumn() const {
    if (!combo_sort_column_ || combo_sort_column_->currentIndex() < 0) return 0;
    return combo_sort_column_->currentData().toInt();
}

/**
 * @brief Retorna o sentido de ordenação atual (Ascendente ou Descendente).
 * @return Qt::SortOrder corrente.
 */
Qt::SortOrder TableSearchSortBar::currentSortOrder() const {
    return current_sort_order_;
}

/**
 * @brief Aplica filtro textual sobre todas as linhas da tabela, ocultando as não correspondentes.
 * @param query Texto digitado pelo usuário.
 */
void TableSearchSortBar::applyFilter(const QString& query) {
    if (!table_) return;

    QString cleanQuery = removeAccents(query).trimmed().toLower();
    int visibleCount = 0;
    int totalCount = table_->rowCount();

    for (int r = 0; r < totalCount; ++r) {
        bool match = cleanQuery.isEmpty();
        if (!match) {
            for (int c = 0; c < table_->columnCount(); ++c) {
                if (table_->isColumnHidden(c)) continue;
                auto* it = table_->item(r, c);
                if (it && removeAccents(it->text()).toLower().contains(cleanQuery)) {
                    match = true;
                    break;
                }
            }
        }
        table_->setRowHidden(r, !match);
        if (match) {
            visibleCount++;
        }
    }

    updateCountLabel(visibleCount, totalCount);
}

/**
 * @brief Aplica ordenação na tabela com base na coluna e direção selecionadas.
 */
void TableSearchSortBar::applySort() {
    if (!table_ || is_syncing_) return;
    is_syncing_ = true;
    int col = currentSortColumn();
    if (col >= 0) {
        table_->sortByColumn(col, current_sort_order_);
    }
    is_syncing_ = false;
}

/**
 * @brief Alterna o sentido de ordenação entre A-Z e Z-A e dispara re-ordenação.
 */
void TableSearchSortBar::toggleSortOrder() {
    current_sort_order_ = (current_sort_order_ == Qt::AscendingOrder)
                              ? Qt::DescendingOrder
                              : Qt::AscendingOrder;
    updateOrderButtonState();
    applySort();
}

/**
 * @brief Sincroniza a barra quando uma seção do cabeçalho da tabela for clicada.
 * @param logicalIndex Índice lógico da seção clicada.
 */
void TableSearchSortBar::onHeaderSectionClicked(int logicalIndex) {
    if (!table_ || is_syncing_) return;
    is_syncing_ = true;

    current_sort_order_ = table_->horizontalHeader()->sortIndicatorOrder();

    for (int i = 0; i < combo_sort_column_->count(); ++i) {
        if (combo_sort_column_->itemData(i).toInt() == logicalIndex) {
            combo_sort_column_->setCurrentIndex(i);
            break;
        }
    }

    updateOrderButtonState();
    is_syncing_ = false;
}

/**
 * @brief Atualiza o contador de registros visíveis e totais com estilização visual.
 * @param visibleCount Quantidade de itens visíveis.
 * @param totalCount Quantidade total de itens.
 */
void TableSearchSortBar::updateCountLabel(int visibleCount, int totalCount) {
    if (!lbl_count_) return;

    if (search_edit_->text().trimmed().isEmpty()) {
        lbl_count_->setText(QString("%1 %2").arg(totalCount).arg(totalCount == 1 ? entitySingular_ : entityPlural_));
        lbl_count_->setStyleSheet(
            "background-color: palette(base); color: palette(window-text); border: 1px solid palette(mid); "
            "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: 500;");
    } else if (visibleCount == 0) {
        lbl_count_->setText("Nenhum resultado");
        lbl_count_->setStyleSheet(
            "background-color: rgba(239, 68, 68, 0.15); color: #dc2626; border: 1px solid rgba(239, 68, 68, 0.4); "
            "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
    } else {
        lbl_count_->setText(QString("%1 de %2").arg(visibleCount).arg(totalCount));
        lbl_count_->setStyleSheet(
            "background-color: rgba(2, 132, 199, 0.15); color: #0284c7; border: 1px solid rgba(2, 132, 199, 0.4); "
            "border-radius: 10px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
    }
}

/**
 * @brief Atualiza o texto e tooltip do botão de ordenação conforme a direção ativa.
 */
void TableSearchSortBar::updateOrderButtonState() {
    if (!btn_sort_order_) return;
    if (current_sort_order_ == Qt::AscendingOrder) {
        btn_sort_order_->setText("▲ A → Z");
        btn_sort_order_->setToolTip("Ordem Crescente (A a Z). Clique para alternar para Decrescente.");
    } else {
        btn_sort_order_->setText("▼ Z → A");
        btn_sort_order_->setToolTip("Ordem Decrescente (Z a A). Clique para alternar para Crescente.");
    }
}

} // namespace scheduler::ui
