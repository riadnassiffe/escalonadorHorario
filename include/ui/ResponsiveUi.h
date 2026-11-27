#pragma once

/**
 * @file ResponsiveUi.h
 * @brief Utilitários compartilhados para layout responsivo e ajuste automático à tela na GUI Qt6.
 */

#include <QComboBox>
#include <QLayout>
#include <QRect>
#include <QScrollArea>
#include <QSize>
#include <QStyle>
#include <QWidget>
#include <vector>

namespace scheduler::ui {

/**
 * @class FlowLayout
 * @brief Layout que organiza widgets em fluxo horizontal com quebra automática para novas linhas.
 */
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget* parent = nullptr, int margin = -1, int hSpacing = -1, int vSpacing = -1);
    ~FlowLayout() override;

    void addItem(QLayoutItem* item) override;
    int horizontalSpacing() const;
    int verticalSpacing() const;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int width) const override;
    int count() const override;
    QLayoutItem* itemAt(int index) const override;
    QSize minimumSize() const override;
    void setGeometry(const QRect& rect) override;
    QSize sizeHint() const override;
    QLayoutItem* takeAt(int index) override;

private:
    int doLayout(const QRect& rect, bool testOnly) const;
    int smartSpacing(QStyle::PixelMetric pm) const;

    std::vector<QLayoutItem*> itemList_;
    int hSpace_;
    int vSpace_;
};

/**
 * @brief Ajusta o tamanho e posição de uma janela ou diálogo à área útil do monitor atual.
 * @param window Janela ou diálogo alvo.
 * @param preferred_size Dimensão preferencial desejada em telas grandes.
 * @param width_ratio Fração máxima da largura útil do monitor (padrão 0.96).
 * @param height_ratio Fração máxima da altura útil do monitor (padrão 0.92).
 */
void fitWindowToScreen(QWidget* window,
                       const QSize& preferred_size,
                       double width_ratio = 0.96,
                       double height_ratio = 0.92);

/**
 * @brief Envolve um widget de conteúdo em uma QScrollArea sem bordas com redimensionamento automático.
 * @param content Widget de conteúdo a ser rolável.
 * @param parent Widget pai da QScrollArea.
 * @return Ponteiro para a QScrollArea configurada.
 */
QScrollArea* wrapInScrollArea(QWidget* content, QWidget* parent = nullptr);

/**
 * @brief Configura um QComboBox para não expandir a largura mínima da janela com textos longos.
 * @param combo Combo alvo.
 * @param min_chars Quantidade mínima de caracteres para cálculo de tamanho.
 * @param min_width Largura mínima em pixels.
 */
void makeComboResponsive(QComboBox* combo, int min_chars = 16, int min_width = 120);

} // namespace scheduler::ui
