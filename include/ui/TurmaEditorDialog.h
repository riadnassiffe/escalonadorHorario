/**
 * @file TurmaEditorDialog.h
 * @brief Diálogo para cadastro e edição de turmas, jornadas diárias e salas vinculadas.
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
#include <QLabel>
#include <QTabWidget>
#include <vector>
#include <array>

namespace scheduler::ui {

/**
 * @struct DayScheduleWidgets
 * @brief Agrupa os widgets de interface para controle de jornada e intervalos de um dia específico.
 */
struct DayScheduleWidgets {
    QCheckBox* chk_dia_ativo{nullptr};    ///< Checkbox indicando se a turma tem aulas neste dia.
    QTimeEdit* time_inicio{nullptr};      ///< Horário de início do turno.
    QTimeEdit* time_fim{nullptr};         ///< Horário de término do turno.
    QCheckBox* chk_int1{nullptr};         ///< Habilitação do intervalo/recreio 1.
    QTimeEdit* time_int1_ini{nullptr};    ///< Início do intervalo 1.
    QTimeEdit* time_int1_fim{nullptr};    ///< Término do intervalo 1.
    QCheckBox* chk_int2{nullptr};         ///< Habilitação do intervalo/almoço 2.
    QTimeEdit* time_int2_ini{nullptr};    ///< Início do intervalo 2.
    QTimeEdit* time_int2_fim{nullptr};    ///< Término do intervalo 2.
    QCheckBox* chk_int3{nullptr};         ///< Habilitação do intervalo 3.
    QTimeEdit* time_int3_ini{nullptr};    ///< Início do intervalo 3.
    QTimeEdit* time_int3_fim{nullptr};    ///< Término do intervalo 3.
    QLabel* lbl_status{nullptr};          ///< Rótulo para mensagens de validação e minutos letivos.
    QWidget* content_panel{nullptr};      ///< Painel contêiner dos horários do dia.
};

/**
 * @class TurmaEditorDialog
 * @brief Diálogo gráfico para inserção e edição completa de turmas acadêmicas.
 *
 * Configura nome, turno padrão, jornadas específicas por dia da semana (com suporte a até 3 intervalos),
 * e política de alocação de salas teóricas (sala fixa exclusiva ou salas permitidas).
 */
class TurmaEditorDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constrói a janela de diálogo para edição ou criação de turma.
     * @param salas_disponiveis Vetor de salas cadastradas na instituição.
     * @param parent Widget pai na hierarquia Qt.
     * @param turma Ponteiro opcional para turma existente (nullptr para nova turma).
     * @param default_id ID sugerido se for novo registro.
     */
    explicit TurmaEditorDialog(const std::vector<model::Room>& salas_disponiveis,
                               QWidget* parent = nullptr,
                               const model::Turma* turma = nullptr,
                               const std::string& default_id = "");

    /**
     * @brief Constrói e retorna uma nova instância de model::Turma com os dados da janela.
     * @return model::Turma preenchida.
     */
    model::Turma getTurma() const;

private slots:
    /**
     * @brief Valida os horários e intervalos de todos os dias da semana e atualiza o resumo semanal.
     */
    void validateAllDays();

    /**
     * @brief Atualiza a visibilidade e habilitação dos controles de sala fixa vs. salas permitidas.
     */
    void onSalaModeChanged();

private:
    /**
     * @brief Inicializa e posiciona os componentes visuais do diálogo.
     */
    void setupUi();

    /**
     * @brief Carrega os dados de uma turma existente nos campos e abas do formulário.
     * @param turma Instância da turma a ser carregada.
     */
    void loadTurma(const model::Turma& turma);

    /**
     * @brief Cria o painel de interface correspondente à aba de um dia da semana.
     * @param day_idx Índice do dia (0 = Segunda, ..., 4 = Sexta).
     * @return QWidget* contendo os controles do dia.
     */
    QWidget* createDayTab(int day_idx);

    /**
     * @brief Aplica uma predefinição de horários (Manhã, Tarde, Noite, etc.) a um dia específico.
     * @param day_idx Índice do dia da semana.
     * @param preset_type Tipo de turno da predefinição.
     */
    void applyPresetToDay(int day_idx, model::ShiftType preset_type);

    /**
     * @brief Aplica uma predefinição de horários a todos os dias úteis.
     * @param preset_type Tipo de turno da predefinição.
     */
    void applyPresetToAllDays(model::ShiftType preset_type);

    /**
     * @brief Replica a jornada configurada em um dia para os demais dias da semana.
     * @param source_day_idx Índice do dia de origem.
     */
    void copyDayScheduleToOthers(int source_day_idx);

    /**
     * @brief Extrai uma estrutura JornadaTurma a partir dos widgets visuais do dia.
     * @param w Estrutura DayScheduleWidgets correspondente ao dia.
     * @return model::JornadaTurma preenchida.
     */
    static model::JornadaTurma getJornadaFromWidgets(const DayScheduleWidgets& w);

    /**
     * @brief Preenche os widgets de um dia com os valores contidos em uma JornadaTurma.
     * @param w Estrutura DayScheduleWidgets a ser preenchida.
     * @param j Objeto model::JornadaTurma com os dados.
     */
    static void setWidgetsFromJornada(DayScheduleWidgets& w, const model::JornadaTurma& j);

    const std::vector<model::Room>& salas_disponiveis_;

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
