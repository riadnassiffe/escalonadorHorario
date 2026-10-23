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

namespace scheduler::ui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onNewProject();
    void onOpenFile();
    void onSaveFile();
    void onLoadDefaultExample();
    void onOptimize();
    void onExportHtml();
    void onExportCsv();

    // CRUD Professores
    void onAddProfessor();
    void onEditProfessor();
    void onRemoveProfessor();

    // CRUD Turmas
    void onAddTurma();
    void onEditTurma();
    void onRemoveTurma();

    // CRUD Salas
    void onAddSala();
    void onEditSala();
    void onRemoveSala();

    // CRUD Disciplinas
    void onAddDisciplina();
    void onEditDisciplina();
    void onRemoveDisciplina();

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupKpiBanner();
    void updateKpiBanner();

    QWidget* createProfessoresTab();
    QWidget* createTurmasTab();
    QWidget* createSalasTab();
    QWidget* createDisciplinasTab();
    QWidget* createLogsTab();

    void refreshAllTables();
    void refreshProfessoresTable();
    void refreshTurmasTable();
    void refreshSalasTable();
    void refreshDisciplinasTable();

    model::InstitutionData data_;
    solver::ScheduleSolver solver_;

    QTabWidget* tab_widget_{nullptr};
    TimetableView* timetable_view_{nullptr};
    ConfigWidget* config_widget_{nullptr};

    // KPI Banner Widgets
    QLabel* lbl_kpi_status_{nullptr};
    QLabel* lbl_kpi_tempo_{nullptr};
    QLabel* lbl_kpi_aulas_{nullptr};
    QLabel* lbl_kpi_janelas_{nullptr};
    QLabel* lbl_kpi_dias_{nullptr};
    QLabel* lbl_kpi_pref_dias_{nullptr};

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
