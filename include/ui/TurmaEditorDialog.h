/**
 * @file TurmaEditorDialog.h
 * @brief Diálogo para cadastro e edição de turmas, slots de horário diários e salas vinculadas.
 */

#pragma once

#include "model/Turma.h"
#include "model/Room.h"
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QTimeEdit>
#include <QRadioButton>
#include <QListWidget>
#include <QTableWidget>
#include <QLabel>
#include <QTabWidget>
#include <array>
#include <map>
#include <vector>

namespace scheduler::ui {

/**
 * @struct DayScheduleWidgets
 * @brief Agrupa os widgets de interface para edição da lista de slots letivos de um dia específico.
 */
struct DayScheduleWidgets {
    QCheckBox* chk_dia_ativo{nullptr};    ///< Checkbox indicando se a turma tem aulas neste dia.
    QTableWidget* table_slots{nullptr};   ///< Tabela com os slots letivos (Início, Fim, Duração).
    QLabel* lbl_status{nullptr};          ///< Rótulo para mensagens de validação, durações e intervalos detectados.
    QWidget* content_panel{nullptr};      ///< Painel contêiner dos horários do dia.
};

/**
 * @class TurmaEditorDialog
 * @brief Diálogo gráfico para inserção e edição completa de turmas acadêmicas.
 *
 * Configura nome, turno padrão, slots letivos específicos por dia da semana (com início e fim),
 * geração rápida em lote, modelos por turno e política de alocação de salas teóricas.
 */
class TurmaEditorDialog : public QDialog {
    Q_OBJECT

public:
    explicit TurmaEditorDialog(
        const std::vector<model::Room>& salas_disponiveis,
        QWidget* parent = nullptr,
        const model::Turma* turma = nullptr,
        const std::string& default_id = "",
        const std::map<model::ShiftType, std::vector<model::TimeInterval>>& slot_templates = {});

    model::Turma getTurma() const;

private slots:
    void validateAllDays();
    void onSalaModeChanged();

private:
    void setupUi();
    void loadTurma(const model::Turma& turma);
    QWidget* createDayTab(int day_idx);

    void addSlotRow(int day_idx, const QTime& start_t, const QTime& end_t);
    void removeSelectedSlotRow(int day_idx);
    void openGenerateSlotsDialog(int day_idx);
    void updateDaySlotDurations(int day_idx);

    void applyPresetToDay(int day_idx, model::ShiftType preset_type);
    void applyPresetToAllDays(model::ShiftType preset_type);
    void copyDayScheduleToOthers(int source_day_idx);

    static model::JornadaTurma getJornadaFromWidgets(const DayScheduleWidgets& w);
    void setWidgetsFromJornada(int day_idx, const model::JornadaTurma& j);

    const std::vector<model::Room>& salas_disponiveis_;
    std::map<model::ShiftType, std::vector<model::TimeInterval>> slot_templates_;

    std::string id_{"T01"};
    QLineEdit* txt_nome_{nullptr};
    QComboBox* combo_turno_{nullptr};

    std::array<DayScheduleWidgets, 5> days_widgets_{};
    QTabWidget* tab_dias_{nullptr};
    QLabel* lbl_resumo_semanal_{nullptr};

    QRadioButton* rad_sala_fixa_{nullptr};
    QComboBox* combo_sala_fixa_{nullptr};
    QRadioButton* rad_sala_opt_{nullptr};
    QListWidget* list_salas_permitidas_{nullptr};
};

} // namespace scheduler::ui
