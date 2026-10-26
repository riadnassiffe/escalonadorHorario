/**
 * @file ProfessorEditorDialog.h
 * @brief Diálogo para cadastro e edição de dados, disponibilidades e preferências docentes.
 */

#pragma once

#include "model/Professor.h"
#include <QDialog>
#include <QLineEdit>
#include <QSlider>
#include <QLabel>
#include <QSpinBox>
#include <QTableWidget>
#include <QCheckBox>
#include <array>

namespace scheduler::ui {

/**
 * @class ProfessorEditorDialog
 * @brief Diálogo para configuração detalhada de perfil docente.
 *
 * Permite cadastrar nome, carga horária diária máxima, pesos de preferência para cada
 * dia útil (-2 a +2), aversão a janelas de horário, turnos pretendidos (Manhã, Tarde, Noite)
 * e intervalos exatos de disponibilidade semanal.
 */
class ProfessorEditorDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constrói a janela de diálogo para inserção ou edição de professor.
     * @param parent Widget pai na hierarquia Qt.
     * @param prof Ponteiro opcional para professor existente (nullptr cria novo registro).
     * @param default_id ID sugerido se for inclusão de novo professor.
     */
    explicit ProfessorEditorDialog(QWidget* parent = nullptr,
                                   const model::Professor* prof = nullptr,
                                   const std::string& default_id = "");

    /**
     * @brief Constrói e retorna a entidade Professor com base nos dados configurados no diálogo.
     * @return model::Professor preenchida.
     */
    model::Professor getProfessor() const;

private slots:
    /**
     * @brief Slot acionado na movimentação dos sliders de preferência de dias úteis.
     */
    void onSliderValueChanged();

    /**
     * @brief Slot acionado na movimentação do slider de tolerância/peso a janelas de horário.
     */
    void onSliderJanelaChanged();

    /**
     * @brief Adiciona uma nova linha de intervalo de disponibilidade na tabela.
     */
    void onAddInterval();

    /**
     * @brief Remove o intervalo de disponibilidade atualmente selecionado na tabela.
     */
    void onRemoveInterval();

    /**
     * @brief Preenche a tabela com a disponibilidade padrão de Manhã (07:00 às 12:00 de Seg a Sex).
     */
    void onSetDefaultMorning();

    /**
     * @brief Preenche a tabela com a disponibilidade padrão de Tarde (13:00 às 18:00 de Seg a Sex).
     */
    void onSetDefaultAfternoon();

    /**
     * @brief Preenche a tabela com a disponibilidade padrão de Noite (18:30 às 22:30 de Seg a Sex).
     */
    void onSetDefaultNight();

    /**
     * @brief Preenche a tabela com disponibilidade integral (07:00 às 22:30 de Seg a Sex).
     */
    void onSetDefaultAllDay();

    /**
     * @brief Preenche a tabela com disponibilidade comercial estendida (07:00 às 18:00 de Seg a Sex).
     */
    void onSetDefaultWholeDay();

    /**
     * @brief Limpa todos os intervalos de disponibilidade cadastrados na tabela.
     */
    void onClearAllIntervals();

    /**
     * @brief Marca todos os turnos para todos os dias úteis.
     */
    void onSelectAllShifts();

    /**
     * @brief Marca exclusivamente os turnos matutinos para todos os dias.
     */
    void onSelectMorningOnly();

    /**
     * @brief Marca os turnos de manhã e tarde para todos os dias.
     */
    void onSelectMorningAfternoon();

private:
    /**
     * @brief Constrói os elementos de interface, abas de preferências e tabelas.
     */
    void setupUi();

    /**
     * @brief Carrega os dados de um professor existente nas abas e controles do formulário.
     * @param prof Instância do professor existente.
     */
    void loadProfessor(const model::Professor& prof);

    std::string id_{"P01"};
    QLineEdit* txt_nome_{nullptr};
    std::array<QSlider*, 5> sliders_dias_{};
    std::array<QLabel*, 5> lbls_valores_dias_{};
    std::array<QLabel*, 5> lbls_desc_dias_{};
    
    // Vontade individual do professor quanto a janelas
    QSlider* slider_peso_janela_{nullptr};
    QLabel* lbl_val_peso_janela_{nullptr};
    QLabel* lbl_desc_peso_janela_{nullptr};

    // Turnos pretendidos por dia da semana (5 dias x 3 turnos: 0=Manhã, 1=Tarde, 2=Noite)
    std::array<std::array<QCheckBox*, 3>, 5> check_turnos_dias_{};

    QTableWidget* table_disp_{nullptr};
    QSpinBox* spin_carga_max_{nullptr};
};

} // namespace scheduler::ui
