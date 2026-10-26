#pragma once

/**
 * @file Professor.h
 * @brief Definição da classe Professor, contendo dados, disponibilidades e preferências do docente.
 */

#include "model/TimeSlot.h"
#include <string>
#include <vector>
#include <map>
#include <array>

namespace scheduler::model {

/**
 * @class Professor
 * @brief Representa um docente no sistema com suas restrições e preferências pedagógicas.
 *
 * Armazena informações cadastrais, preferências diárias (pesos de 0 a 10), janelas de disponibilidade
 * de horário por dia da semana, limites diários de carga horária e aulas, aversão a janelas entre aulas
 * e preferências de turnos de trabalho.
 */
class Professor {
public:
    /**
     * @brief Construtor padrão da classe Professor. Inicializa preferências neutras padrão.
     */
    Professor() {
        initDefaults();
    }

    /**
     * @brief Construtor parametrizado com identificador e nome do professor.
     * @param id Identificador único do professor (ex: "P01").
     * @param nome Nome completo ou institucional do professor.
     */
    Professor(std::string id, std::string nome)
        : id_(std::move(id)), nome_(std::move(nome)) {
        initDefaults();
    }

    /**
     * @brief Obtém o identificador único do professor.
     * @return Referência constante para a string do ID.
     */
    const std::string& id() const noexcept { return id_; }

    /**
     * @brief Define o identificador único do professor.
     * @param id Novo ID do professor.
     */
    void setId(std::string id) { id_ = std::move(id); }

    /**
     * @brief Obtém o nome do professor.
     * @return Referência constante para a string do nome.
     */
    const std::string& nome() const noexcept { return nome_; }

    /**
     * @brief Define o nome do professor.
     * @param nome Novo nome do docente.
     */
    void setNome(std::string nome) { nome_ = std::move(nome); }

    /**
     * @brief Obtém o peso de preferência do professor para lecionar em determinado dia (0 a 10).
     * @param d Dia da semana avaliado.
     * @return Inteiro de 0 (menos desejado) a 10 (mais preferido).
     */
    int pesoDia(DayOfWeek d) const noexcept {
        return peso_dias_[static_cast<size_t>(d)];
    }

    /**
     * @brief Define o peso de preferência para determinado dia da semana (fixado entre 0 e 10).
     * @param d Dia da semana configurado.
     * @param peso Valor numérico da preferência (0 a 10).
     */
    void setPesoDia(DayOfWeek d, int peso) {
        if (peso < 0) peso = 0;
        if (peso > 10) peso = 10;
        peso_dias_[static_cast<size_t>(d)] = peso;
    }

    /**
     * @brief Calcula o custo de penalidade associado a alocar o professor no dia (c_p,d = 10 - omega_p,d).
     * @param d Dia da semana a ser avaliado.
     * @return Valor da penalidade para a função objetivo do otimizador.
     */
    int penalidadeDia(DayOfWeek d) const noexcept {
        return 10 - pesoDia(d);
    }

    /**
     * @brief Obtém a lista de intervalos de horário disponíveis para o professor no dia informado.
     * @param d Dia da semana consultado.
     * @return Referência constante para o vetor de intervalos de disponibilidade.
     */
    const std::vector<TimeInterval>& disponibilidade(DayOfWeek d) const {
        static const std::vector<TimeInterval> empty_intervals;
        auto it = disponibilidade_.find(d);
        if (it != disponibilidade_.end()) {
            return it->second;
        }
        return empty_intervals;
    }

    /**
     * @brief Retorna o mapa completo de disponibilidades do professor para todos os dias.
     * @return Referência constante para o mapa associando cada DayOfWeek aos seus TimeIntervals.
     */
    const std::map<DayOfWeek, std::vector<TimeInterval>>& todasDisponibilidades() const noexcept {
        return disponibilidade_;
    }

    /**
     * @brief Define todos os intervalos de disponibilidade para determinado dia da semana.
     * @param d Dia da semana configurado.
     * @param intervals Vetor com os intervalos em que o professor pode lecionar.
     */
    void setDisponibilidade(DayOfWeek d, std::vector<TimeInterval> intervals) {
        disponibilidade_[d] = std::move(intervals);
    }

    /**
     * @brief Adiciona um intervalo de horário disponível para um dia da semana.
     * @param d Dia da semana correspondente.
     * @param interval Intervalo de tempo disponível a ser adicionado.
     */
    void addIntervaloDisponivel(DayOfWeek d, const TimeInterval& interval) {
        disponibilidade_[d].push_back(interval);
    }

    /**
     * @brief Remove todos os intervalos de disponibilidade cadastrados para determinado dia.
     * @param d Dia da semana a ser limpo.
     */
    void clearDisponibilidade(DayOfWeek d) {
        disponibilidade_.erase(d);
    }

    /**
     * @brief Verifica se um intervalo de tempo [start_slot, end_slot) está totalmente contido nas disponibilidades do dia.
     * @param d Dia da semana testado.
     * @param start_slot Slot inicial da aula.
     * @param end_slot Slot final da aula.
     * @return true se o professor estiver plenamente disponível nesse intervalo, false caso contrário.
     */
    bool isDisponivelNoIntervalo(DayOfWeek d, int start_slot, int end_slot) const;

    /**
     * @brief Obtém o limite máximo de carga horária diária do professor em minutos.
     * @return Limite em minutos (0 se não houver limite estipulado).
     */
    int cargaMaximaDiariaMinutos() const noexcept { return carga_maxima_diaria_minutos_; }

    /**
     * @brief Define o limite máximo de carga horária diária do professor em minutos.
     * @param minutos Limite diário em minutos (0 = sem limite).
     */
    void setCargaMaximaDiariaMinutos(int minutos) noexcept { carga_maxima_diaria_minutos_ = minutos; }

    /**
     * @brief Obtém o número máximo de aulas permitidas para o professor em um único dia.
     * @return Quantidade máxima de aulas (0 se não houver limite).
     */
    int maxAulasPorDia() const noexcept { return max_aulas_por_dia_; }

    /**
     * @brief Define o número máximo de aulas permitidas para o professor em um único dia.
     * @param max_aulas Quantidade máxima de aulas por dia (0 = sem limite).
     */
    void setMaxAulasPorDia(int max_aulas) noexcept { max_aulas_por_dia_ = max_aulas; }

    /**
     * @brief Obtém o peso de aversão do professor a janelas ociosas entre aulas (0 a 10).
     * @return Valor numérico da aversão (10 = máxima aversão a janelas, 0 = indiferente).
     */
    int pesoJanela() const noexcept { return peso_janela_; }

    /**
     * @brief Define o peso de aversão do professor a janelas ociosas entre aulas.
     * @param peso Valor numérico de aversão (fixado entre 0 e 10).
     */
    void setPesoJanela(int peso) noexcept {
        if (peso < 0) peso = 0;
        if (peso > 10) peso = 10;
        peso_janela_ = peso;
    }

    /**
     * @brief Retorna os turnos de trabalho pretendidos pelo professor para determinado dia.
     * @param d Dia da semana consultado.
     * @return Referência constante para o vetor com os turnos pretendidos.
     */
    const std::vector<ShiftType>& turnosPretendidos(DayOfWeek d) const;

    /**
     * @brief Verifica se o professor pretende trabalhar em um dado turno e dia da semana.
     * @param d Dia da semana verificado.
     * @param shift Turno avaliado (MANHA, TARDE, NOITE, INTEGRAL).
     * @return true se o turno for compatível com as preferências, false caso contrário.
     */
    bool pretendeTurno(DayOfWeek d, ShiftType shift) const;

    /**
     * @brief Define os turnos pretendidos pelo professor para determinado dia da semana.
     * @param d Dia da semana configurado.
     * @param turnos Vetor com os turnos pretendidos.
     */
    void setTurnosPretendidos(DayOfWeek d, std::vector<ShiftType> turnos);

    /**
     * @brief Adiciona um turno pretendido para determinado dia da semana.
     * @param d Dia da semana configurado.
     * @param shift Turno a ser incluído.
     */
    void addTurnoPretendido(DayOfWeek d, ShiftType shift);

    /**
     * @brief Limpa as preferências de turnos de trabalho cadastradas para determinado dia.
     * @param d Dia da semana a ser limpo.
     */
    void clearTurnosPretendidos(DayOfWeek d);

    /**
     * @brief Retorna o mapa de preferências de turnos para todos os dias.
     * @return Referência constante para o mapa associando cada dia aos turnos pretendidos.
     */
    const std::map<DayOfWeek, std::vector<ShiftType>>& todasPreferenciasTurnos() const noexcept {
        return turnos_pretendidos_;
    }

private:
    /**
     * @brief Inicializa valores neutros padrão para preferências de dias, aversão a janelas e turnos.
     */
    void initDefaults();

    std::string id_;
    std::string nome_;
    std::array<int, 5> peso_dias_{5, 5, 5, 5, 5};
    int peso_janela_{8}; // Aversão a janelas individual do professor
    std::map<DayOfWeek, std::vector<TimeInterval>> disponibilidade_;
    std::map<DayOfWeek, std::vector<ShiftType>> turnos_pretendidos_;
    int carga_maxima_diaria_minutos_{0}; // 0 = sem limite
    int max_aulas_por_dia_{0};           // 0 = sem limite
};

} // namespace scheduler::model
