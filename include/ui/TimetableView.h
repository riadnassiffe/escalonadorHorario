/**
 * @file TimetableView.h
 * @brief Visualizador interativo e gerenciador da grade de horários gerada.
 */

#pragma once

#include "model/InstitutionData.h"
#include "ui/TimetableGridWidget.h"
#include <QWidget>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <vector>

namespace scheduler::ui {

/**
 * @class TimetableView
 * @brief Widget principal para visualização, filtragem e edição direta da grade horária semanal.
 *
 * Suporta quatro modos de perspectiva (por Turma, por Professor, por Sala e Todas as Disciplinas/Turmas),
 * filtragem por semestre letivo, busca em tempo real de disciplinas/professores, menu de contexto com fixação (Pin),
 * alteração de sala com verificação de conflitos, movimentação manual e suporte a Drag & Drop.
 */
class TimetableView : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constrói a visualização da grade horária.
     * @param data Referência para o modelo de dados acadêmicos e grade.
     * @param parent Widget pai na hierarquia Qt.
     */
    explicit TimetableView(model::InstitutionData& data, QWidget* parent = nullptr);

    /**
     * @brief Recarrega os dados e redesenha completamente a grade na tela.
     */
    void refreshView();

    /**
     * @brief Habilita ou desabilita edições manuais na grade durante otimizações em segundo plano.
     */
    void setReadOnlyMode(bool read_only);

signals:
    /**
     * @brief Emitido quando uma aula tem seu estado de fixação alterado pelo usuário.
     * @param aula_id ID da aula.
     * @param pinned true se foi fixada; false se desfixada.
     */
    void lessonPinChanged(const QString& aula_id, bool pinned);

    /**
     * @brief Emitido após a movimentação bem-sucedida de uma aula.
     * @param aula_id ID da aula movimentada.
     * @param details Descrição textual da movimentação realizada.
     */
    void lessonMoved(const QString& aula_id, const QString& details);

    /**
     * @brief Emitido quando qualquer alteração manual for realizada na grade.
     */
    void timetableModified();

    /**
     * @brief Emitido quando for solicitada uma reotimização a partir da grade atual.
     */
    void requestReoptimization();

public slots:
    /**
     * @brief Trata a solicitação de soltura (drop) de uma aula em nova célula da tabela.
     * @param aula_id ID da aula arrastada.
     * @param target_row Linha da célula de destino.
     * @param target_col Coluna da célula de destino.
     */
    void onLessonDropRequested(const QString& aula_id, int target_row, int target_col);

    /**
     * @brief Abre o diálogo de movimentação manual de horário e sala para a aula informada.
     * @param aula_id ID da aula a movimentar.
     */
    void moverAulaManual(const QString& aula_id);

    /**
     * @brief Abre o diálogo de permuta direta de horários (Swap) entre a aula e outra aula da grade.
     * @param aula_id ID da aula de origem (se vazio, usa a aula atualmente selecionada ou permite escolher).
     */
    void trocarAulaManual(const QString& aula_id = QString());

    /**
     * @brief Abre o diálogo específico para alteração ou permuta de salas para a aula.
     * @param aula_id ID da aula.
     */
    void mudarSalaManual(const QString& aula_id);

    /**
     * @brief Abre o diálogo de alocação manual de disciplina na grade horária.
     * @param row Linha da célula da tabela selecionada (-1 se nenhuma).
     * @param col Coluna da célula da tabela selecionada (-1 se nenhuma).
     */
    void alocarAulaManual(int row = -1, int col = -1);

    /**
     * @brief Limpa todas as alocações da grade horária permitindo reiniciar a montagem da grade.
     */
    void limparGradeCompleta();

    /**
     * @brief Trata a seleção individual de uma aula pelo usuário na grade horária.
     * @param aula_id ID da aula selecionada.
     */
    void onLessonSelected(const QString& aula_id);

    /**
     * @brief Limpa a seleção atual de aula e restaura a exibição padrão da grade.
     */
    void onEmptyCellClicked();

private slots:
    /**
     * @brief Responde à mudança de perspectiva (Turma, Professor ou Sala).
     * @param index Novo índice do tipo de visão.
     */
    void onViewTypeChanged(int index);

    /**
     * @brief Responde à seleção de uma entidade específica no combo (ex: Turma 1A).
     * @param index Novo índice da entidade selecionada.
     */
    void onTargetEntityChanged(int index);

    /**
     * @brief Filtra a visualização entre 1º Semestre, 2º Semestre ou Ambos.
     * @param index Índice do semestre selecionado.
     */
    void onSemestreFiltroChanged(int index);

    /**
     * @brief Exibe o menu de contexto ao clicar com botão direito sobre uma célula.
     * @param pos Coordenadas locais do clique.
     */
    void onCellContextMenu(const QPoint& pos);

    /**
     * @brief Trata o clique duplo sobre uma célula para abrir movimentação rápida.
     * @param row Linha da célula.
     * @param col Coluna da célula.
     */
    void onCellDoubleClicked(int row, int col);

    /**
     * @brief Filtra e destaca na grade as aulas que coincidem com o termo buscado.
     * @param query Texto digitado no campo de busca.
     */
    void onSearchLessonChanged(const QString& query);

    /**
     * @brief Alterna a orientação da grade semanal entre Horário por Dia e Dia por Horário.
     */
    void onToggleGridOrientation();

    /**
     * @brief Exibe um menu resumido com os choques ativos da grade para navegação rápida.
     */
    void onShowConflictsMenu();

private:
    /**
     * @brief Inicializa e monta a barra de ferramentas superior, banner informativo e a grade.
     */
    void setupUi();

    /**
     * @brief Atualiza o estilo visual e background do banner informativo conforme o tema ativo.
     */
    void updateBannerTheme();

    /**
     * @brief Preenche o combobox de entidades alvo de acordo com o modo de visualização atual.
     */
    void populateTargetCombo();

    /**
     * @brief Constrói as linhas horárias, colunas de dias e renderiza os cartões de aula e intervalos.
     */
    void renderTimetable();

    /**
     * @brief Verifica choques de turmas, docentes e salas ao mover uma aula para novo dia e horário.
     * @param aid ID da aula candidata a ser movida.
     * @param dia Novo dia pretendido.
     * @param intervalo Novo intervalo de tempo pretendido.
     * @return Vetor de strings com os conflitos detectados (vazio se livre).
     */
    std::vector<std::string> verificarConflitosMovimentacao(const std::string& aid,
                                                            model::DayOfWeek dia,
                                                            const model::TimeInterval& intervalo) const;

    /**
     * @brief Formata rótulos resumidos e deduplicados de choques para exibição compacta na grade.
     */
    QString formatShortConflictTags(const std::vector<model::SlotConflictInfo>& infos, int max_tags = 2) const;

    /**
     * @brief Atualiza a pílula de resumo de choques no subpainel da grade.
     */
    void updateConflictSummaryPill(const std::map<std::string, std::vector<model::SlotConflictInfo>>& conflitos_map);

    model::InstitutionData& data_;
    QComboBox* combo_view_type_{nullptr};
    QLabel* lbl_entity_{nullptr};
    QComboBox* combo_target_{nullptr};
    QComboBox* combo_semestre_filtro_{nullptr};
    QLabel* lbl_info_banner_{nullptr};
    QPushButton* btn_clear_selection_{nullptr};
    QPushButton* btn_conflict_summary_{nullptr};
    QLineEdit* search_lesson_{nullptr};
    QLabel* lbl_search_count_{nullptr};
    TimetableGridWidget* table_{nullptr};
    QPushButton* btn_toggle_orientation_{nullptr};
    QPushButton* btn_swap_lesson_{nullptr};
    QPushButton* btn_add_lesson_{nullptr};
    QPushButton* btn_clear_timetable_{nullptr};

    bool transpose_grid_{false};
    bool read_only_mode_{false};
    QString selected_aula_id_;
    std::vector<model::TimeInterval> row_intervals_;
};

} // namespace scheduler::ui


