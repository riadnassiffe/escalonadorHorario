/**
 * @file LessonEditorDialog.h
 * @brief Diálogo para cadastro e edição de disciplinas acadêmicas e parâmetros de alocação de aulas.
 */

#pragma once

#include "model/Lesson.h"
#include "model/Professor.h"
#include "model/Turma.h"
#include "model/Room.h"
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QListWidget>
#include <QCheckBox>
#include <QLabel>
#include <vector>

namespace scheduler::ui {

/**
 * @class LessonEditorDialog
 * @brief Diálogo gráfico para inserção e edição completa de disciplinas curriculares.
 *
 * Configura nome, regime letivo (anual ou semestral), semestre de oferta, tipo de ambiente,
 * duração de aula em minutos, quantidade de aulas semanais, professor responsável,
 * turma destinatária, aulas consecutivas e salas/laboratórios alternativos.
 */
class LessonEditorDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constrói a janela de diálogo para edição ou criação de disciplina.
     * @param profs Vetor de professores disponíveis.
     * @param turmas Vetor de turmas disponíveis.
     * @param salas Vetor de salas e laboratórios disponíveis.
     * @param parent Widget pai na hierarquia Qt.
     * @param disc Ponteiro opcional para disciplina existente (nullptr cria nova disciplina).
     * @param default_id ID sugerido se for novo registro.
     */
    explicit LessonEditorDialog(const std::vector<model::Professor>& profs,
                                const std::vector<model::Turma>& turmas,
                                const std::vector<model::Room>& salas,
                                QWidget* parent = nullptr,
                                const model::Disciplina* disc = nullptr,
                                const std::string& default_id = "");

    /**
     * @brief Constrói e retorna uma nova instância de model::Disciplina com os dados preenchidos.
     * @return model::Disciplina configurada.
     */
    model::Disciplina getDisciplina() const;

private slots:
    /**
     * @brief Slot acionado na mudança do tipo de ambiente (teórica vs laboratório).
     * @param index Novo índice selecionado no combobox.
     */
    void onTipoChanged(int index);

    /**
     * @brief Slot acionado na alteração do regime (anual vs semestral).
     * @param index Novo índice selecionado no combobox.
     */
    void onRegimeChanged(int index);

private:
    /**
     * @brief Inicializa e monta a interface do formulário e botões de ação.
     */
    void setupUi();

    /**
     * @brief Carrega os dados de uma disciplina existente nos campos do formulário.
     * @param disc Objeto model::Disciplina com os dados originais.
     */
    void loadDisciplina(const model::Disciplina& disc);

    /**
     * @brief Filtra a lista de professores pelo texto digitado sem alterar as seleções.
     */
    void filterProfessores(const QString& query);

    /**
     * @brief Atualiza o rótulo de resumo com a quantidade e nomes dos professores selecionados.
     */
    void updateProfessoresSummary();

    const std::vector<model::Professor>& profs_;
    const std::vector<model::Turma>& turmas_;
    const std::vector<model::Room>& salas_;

    std::string id_{"D01"};
    QLineEdit* txt_nome_{nullptr};
    QComboBox* combo_tipo_{nullptr};
    QComboBox* combo_regime_{nullptr};
    QComboBox* combo_semestre_{nullptr};
    QComboBox* combo_duracao_{nullptr};
    QSpinBox* spin_aulas_semanais_{nullptr};
    QLabel* lbl_profs_resumo_{nullptr};
    QLineEdit* txt_search_prof_{nullptr};
    QListWidget* list_professores_{nullptr};
    QComboBox* combo_turma_{nullptr};
    QListWidget* list_salas_alternativas_{nullptr};
    QWidget* widget_salas_alternativas_{nullptr};
    QCheckBox* check_consecutivas_{nullptr};
    QSpinBox* spin_max_dia_{nullptr};
};

} // namespace scheduler::ui
