/**
 * @file MainWindow.h
 * @brief Janela principal da aplicação gráfica do escalonador de horários.
 */

#pragma once

#include "model/InstitutionData.h"
#include "solver/ScheduleSolver.h"
#include "ui/TimetableView.h"
#include "ui/ConfigWidget.h"
#include "ui/TableSearchSortBar.h"
#include <QMainWindow>
#include <QTabWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QToolButton>
#include <QAction>
#include <QList>
#include <QTimer>
#include <QElapsedTimer>
#include <QCloseEvent>
#include <thread>

namespace scheduler::ui {

/**
 * @class MainWindow
 * @brief Janela principal da aplicação gráfica (GUI) do Escalonador de Horários.
 *
 * Integra o gerenciamento do projeto (abrir, salvar, novo, carregar exemplos),
 * cadastro e edição com busca e ordenação das entidades acadêmicas (professores, turmas,
 * salas e disciplinas), acionamento de múltiplos algoritmos de otimização, exibição
 * em tempo real dos indicadores de desempenho (KPIs), visualizador interativo semanal e
 * exportação de relatórios em HTML e CSV.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief Constrói a janela principal e inicializa temas, menus, tabelas e visualizador.
     * @param parent Widget pai na hierarquia Qt (opcional).
     */
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    /**
     * @brief Carrega um arquivo JSON de projeto diretamente para a interface.
     * @param path Caminho do arquivo JSON.
     * @return true se carregado com sucesso; false caso contrário.
     */
    bool loadProjectFile(const QString& path);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    /**
     * @brief Cria um novo projeto vazio com dados padrão.
     */
    void onNewProject();

    /**
     * @brief Abre uma caixa de diálogo para carregar dados institucionais a partir de arquivo JSON.
     */
    void onOpenFile();

    /**
     * @brief Salva os dados acadêmicos e a grade atual em arquivo JSON.
     */
    void onSaveFile();

    /**
     * @brief Carrega o cenário acadêmico de demonstração padrão no sistema.
     */
    void onLoadDefaultExample();

    /**
     * @brief Abre o diálogo de otimização e dispara o solver em thread de trabalho.
     */
    void onOptimize();

    /**
     * @brief Exporta o relatório completo da grade horária em formato HTML estilizado.
     */
    void onExportHtml();

    /**
     * @brief Exporta as alocações da grade em arquivo de planilha CSV.
     */
    void onExportCsv();

    // CRUD Professores
    /**
     * @brief Abre formulário para adicionar novo professor.
     */
    void onAddProfessor();

    /**
     * @brief Abre formulário para editar o professor selecionado.
     */
    void onEditProfessor();

    /**
     * @brief Remove o professor selecionado após confirmação.
     */
    void onRemoveProfessor();

    // CRUD Turmas
    /**
     * @brief Abre formulário para adicionar nova turma.
     */
    void onAddTurma();

    /**
     * @brief Abre formulário para editar a turma selecionada.
     */
    void onEditTurma();

    /**
     * @brief Remove a turma selecionada após confirmação.
     */
    void onRemoveTurma();

    // CRUD Salas
    /**
     * @brief Abre formulário para cadastrar nova sala ou laboratório.
     */
    void onAddSala();

    /**
     * @brief Abre formulário para editar a sala selecionada.
     */
    void onEditSala();

    /**
     * @brief Remove a sala selecionada após confirmação.
     */
    void onRemoveSala();

    // CRUD Disciplinas
    /**
     * @brief Abre formulário para cadastrar nova disciplina curricular.
     */
    void onAddDisciplina();

    /**
     * @brief Abre formulário para editar a disciplina selecionada.
     */
    void onEditDisciplina();

    /**
     * @brief Remove a disciplina selecionada após confirmação.
     */
    void onRemoveDisciplina();

private:
    /**
     * @brief Configura a estrutura principal de layout, abas e barra de status.
     */
    void setupUi();

    /**
     * @brief Cria os menus da barra superior (Arquivo, Otimização, Visualizar, Ajuda).
     */
    void setupMenuBar();

    /**
     * @brief Monta a barra de ferramentas (toolbar) com atalhos rápidos e ações principais.
     */
    void setupToolBar();

    /**
     * @brief Constrói os cards visuais do painel superior de indicadores-chave (KPIs).
     */
    void setupKpiBanner();

    /**
     * @brief Atualiza o estilo visual e background do banner de KPIs conforme o tema ativo.
     */
    void updateKpiBannerTheme();

    /**
     * @brief Atualiza os valores e cores dos KPIs com base nas estatísticas da grade atual.
     */
    void updateKpiBanner();

    /**
     * @brief Constrói a aba de listagem, busca e operações de Professores.
     * @return QWidget* contendo a interface de professores.
     */
    QWidget* createProfessoresTab();

    /**
     * @brief Constrói a aba de listagem, busca e operações de Turmas.
     * @return QWidget* contendo a interface de turmas.
     */
    QWidget* createTurmasTab();

    /**
     * @brief Constrói a aba de listagem, busca e operações de Salas.
     * @return QWidget* contendo a interface de salas.
     */
    QWidget* createSalasTab();

    /**
     * @brief Constrói a aba de listagem, busca e operações de Disciplinas.
     * @return QWidget* contendo a interface de disciplinas.
     */
    QWidget* createDisciplinasTab();

    /**
     * @brief Constrói a aba de visualização de logs e mensagens de diagnóstico do solver.
     * @return QWidget* contendo a interface de logs.
     */
    QWidget* createLogsTab();

    /**
     * @brief Recarrega os registros de todas as tabelas de cadastro na interface.
     */
    void refreshAllTables();

    /**
     * @brief Atualiza a tabela de professores e sincroniza a barra de busca e ordenação.
     */
    void refreshProfessoresTable();

    /**
     * @brief Atualiza a tabela de turmas e sincroniza a barra de busca e ordenação.
     */
    void refreshTurmasTable();

    /**
     * @brief Atualiza a tabela de salas e sincroniza a barra de busca e ordenação.
     */
    void refreshSalasTable();

    /**
     * @brief Atualiza a tabela de disciplinas e sincroniza a barra de busca e ordenação.
     */
    void refreshDisciplinasTable();

    void onStopOptimization();
    void setOptimizationUiState(bool running);
    void onOptimizationProgress(const QString& algo_name, const QString& msg, double ratio);
    void onOptimizationFinished(model::Timetable tt,
                                bool was_stopped,
                                model::EscopoSemestreOtimizacao solved_scope);
    void updateOptimizationStatusBar();

    model::InstitutionData data_;
    solver::ScheduleSolver solver_;

    QTabWidget* tab_widget_{nullptr};
    TimetableView* timetable_view_{nullptr};
    ConfigWidget* config_widget_{nullptr};
    QToolButton* btn_optimize_{nullptr};
    QList<QWidget*> mutating_widgets_;
    QList<QAction*> mutating_actions_;

    bool is_optimizing_{false};
    bool stop_requested_ui_{false};
    QString active_algo_name_;
    QString last_solver_msg_;
    std::thread solver_thread_;
    QTimer* elapsed_timer_{nullptr};
    QElapsedTimer optimization_elapsed_;

    // KPI Banner Widgets
    QFrame* kpi_banner_card_{nullptr};
    QLabel* lbl_kpi_status_{nullptr};
    QLabel* lbl_kpi_tempo_{nullptr};
    QLabel* lbl_kpi_aulas_{nullptr};
    QLabel* lbl_kpi_janelas_{nullptr};
    QLabel* lbl_kpi_dias_{nullptr};
    QLabel* lbl_kpi_pref_dias_{nullptr};
    QLabel* lbl_kpi_choques_{nullptr};

    // Tabelas de Cadastro
    QTableWidget* table_professores_{nullptr};
    TableSearchSortBar* search_bar_professores_{nullptr};
    QTableWidget* table_turmas_{nullptr};
    TableSearchSortBar* search_bar_turmas_{nullptr};
    QTableWidget* table_salas_{nullptr};
    TableSearchSortBar* search_bar_salas_{nullptr};
    QTableWidget* table_disciplinas_{nullptr};
    TableSearchSortBar* search_bar_disciplinas_{nullptr};
    QPlainTextEdit* txt_logs_{nullptr};

    QProgressBar* progress_bar_{nullptr};
};

} // namespace scheduler::ui
