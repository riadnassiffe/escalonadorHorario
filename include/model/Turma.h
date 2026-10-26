#pragma once

/**
 * @file Turma.h
 * @brief Definições da struct JornadaTurma e da classe Turma, gerenciando horários, intervalos e turmas.
 */

#include "model/TimeSlot.h"
#include <string>
#include <vector>
#include <map>
#include <optional>

namespace scheduler::model {

/**
 * @struct JornadaTurma
 * @brief Modela o período diário de aulas de uma turma, composto pela jornada total e intervalos de recreio.
 *
 * Contém o intervalo total [T_ini, T_fim) e até 3 intervalos de recreio configuráveis
 * que dividem a jornada em múltiplos blocos letivos contínuos.
 */
struct JornadaTurma {
    TimeInterval jornada_total; ///< Intervalo completo de início e término do turno da turma.
    TimeInterval intervalo_1;   ///< Primeiro intervalo de recreio.
    bool int1_ativo{true};      ///< Indica se o primeiro intervalo de recreio está ativo.
    TimeInterval intervalo_2;   ///< Segundo intervalo de recreio.
    bool int2_ativo{true};      ///< Indica se o segundo intervalo de recreio está ativo.
    TimeInterval intervalo_3;   ///< Terceiro intervalo de recreio (opcional).
    bool int3_ativo{false};     ///< Indica se o terceiro intervalo de recreio está ativo.

    /**
     * @brief Construtor padrão da estrutura JornadaTurma.
     */
    JornadaTurma() = default;

    /**
     * @brief Construtor de conveniência com 2 intervalos ativos.
     * @param ini Horário de início da jornada (ex: "08:00").
     * @param fim Horário de término da jornada (ex: "12:30").
     * @param int1_ini Início do 1º intervalo (ex: "09:40").
     * @param int1_fim Término do 1º intervalo (ex: "09:55").
     * @param int2_ini Início do 2º intervalo (ex: "11:25").
     * @param int2_fim Término do 2º intervalo (ex: "11:40").
     */
    JornadaTurma(const std::string& ini, const std::string& fim,
                 const std::string& int1_ini, const std::string& int1_fim,
                 const std::string& int2_ini, const std::string& int2_fim);

    /**
     * @brief Construtor completo configurando até 3 intervalos com controle individual de ativação.
     * @param ini Horário de início da jornada.
     * @param fim Horário de término da jornada.
     * @param int1_ini Início do 1º intervalo.
     * @param int1_fim Término do 1º intervalo.
     * @param int1_ativo Flag de ativação do 1º intervalo.
     * @param int2_ini Início do 2º intervalo.
     * @param int2_fim Término do 2º intervalo.
     * @param int2_ativo Flag de ativação do 2º intervalo.
     * @param int3_ini Início do 3º intervalo.
     * @param int3_fim Término do 3º intervalo.
     * @param int3_ativo Flag de ativação do 3º intervalo.
     */
    JornadaTurma(const std::string& ini, const std::string& fim,
                 const std::string& int1_ini, const std::string& int1_fim, bool int1_ativo,
                 const std::string& int2_ini, const std::string& int2_fim, bool int2_ativo,
                 const std::string& int3_ini, const std::string& int3_fim, bool int3_ativo);

    /**
     * @brief Valida a consistência temporal da jornada (se intervalos estão dentro da jornada e sem sobreposição).
     * @return true se a configuração for válida; false caso haja inconsistências.
     */
    bool isValid() const noexcept;

    /**
     * @brief Retorna os blocos contínuos de tempo em que aulas podem ser alocadas (excluindo os intervalos).
     * @return Vetor de TimeInterval com os blocos letivos disponíveis.
     */
    std::vector<TimeInterval> blocosLetivos() const;

    /**
     * @brief Retorna a lista dos intervalos de recreio que estão ativos e ordenados cronologicamente.
     * @return Vetor com os intervalos de descanso ativos.
     */
    std::vector<TimeInterval> intervalosAtivos() const;

    /**
     * @brief Verifica se um determinado slot de tempo coincide com algum intervalo de recreio ativo.
     * @param slot Índice do slot (0 a 287).
     * @return true se o slot estiver dentro de um recreio ativo, false caso contrário.
     */
    bool isSlotEmIntervalo(int slot) const noexcept;

    /**
     * @brief Operador de igualdade entre duas configurações de JornadaTurma.
     * @param o Outra instância de JornadaTurma a ser comparada.
     * @return true se os parâmetros de horários e ativações forem idênticos.
     */
    bool operator==(const JornadaTurma& o) const noexcept;

    /**
     * @brief Operador de desigualdade entre duas configurações de JornadaTurma.
     * @param o Outra instância de JornadaTurma a ser comparada.
     * @return true se houver qualquer divergência de configuração.
     */
    bool operator!=(const JornadaTurma& o) const noexcept { return !(*this == o); }
};

/**
 * @class Turma
 * @brief Representa um agrupamento de estudantes (classe escolar) com jornada e restrições de sala.
 *
 * Cada turma possui turno associado (manhã, tarde, noite, integral), dias letivos de funcionamento,
 * jornada padrão com intervalos, possíveis exceções de jornada por dia da semana e definições sobre
 * sua sala teórica fixa.
 */
class Turma {
public:
    /**
     * @brief Construtor padrão da classe Turma.
     */
    Turma() = default;

    /**
     * @brief Construtor parametrizado de Turma com valores padrão de jornada matutina.
     * @param id Identificador único da turma (ex: "T01").
     * @param nome Nome ou descrição da turma (ex: "1º Ano A - Ensino Médio").
     * @param turno Turno de estudos da turma (padrão: ShiftType::MANHA).
     */
    Turma(std::string id, std::string nome, ShiftType turno = ShiftType::MANHA);

    /**
     * @brief Obtém o identificador único da turma.
     * @return Referência constante para a string do ID.
     */
    const std::string& id() const noexcept { return id_; }

    /**
     * @brief Define o identificador único da turma.
     * @param id Novo ID da turma.
     */
    void setId(std::string id) { id_ = std::move(id); }

    /**
     * @brief Obtém o nome ou série da turma.
     * @return Referência constante para a string do nome.
     */
    const std::string& nome() const noexcept { return nome_; }

    /**
     * @brief Define o nome ou série da turma.
     * @param nome Novo nome da turma.
     */
    void setNome(std::string nome) { nome_ = std::move(nome); }

    /**
     * @brief Obtém o turno principal de funcionamento da turma.
     * @return Valor correspondente de ShiftType.
     */
    ShiftType turno() const noexcept { return turno_; }

    /**
     * @brief Define o turno principal de funcionamento da turma.
     * @param turno Novo turno atribuído.
     */
    void setTurno(ShiftType turno) noexcept { turno_ = turno; }

    /**
     * @brief Obtém a lista dos dias da semana em que a turma possui atividades letivas.
     * @return Referência constante para o vetor de DayOfWeek.
     */
    const std::vector<DayOfWeek>& diasLetivos() const noexcept { return dias_letivos_; }

    /**
     * @brief Define os dias da semana em que a turma funciona.
     * @param dias Vetor de DayOfWeek representando os dias com aulas.
     */
    void setDiasLetivos(std::vector<DayOfWeek> dias) { dias_letivos_ = std::move(dias); }

    /**
     * @brief Verifica se a turma tem aulas no dia da semana especificado.
     * @param d Dia da semana verificado.
     * @return true se o dia estiver na lista de dias letivos da turma, false caso contrário.
     */
    bool funcionaNoDia(DayOfWeek d) const noexcept;

    /**
     * @brief Obtém a jornada padrão aplicada aos dias letivos da turma.
     * @return Referência constante para a JornadaTurma padrão.
     */
    const JornadaTurma& jornadaPadrao() const noexcept { return jornada_padrao_; }

    /**
     * @brief Define a jornada padrão da turma.
     * @param jornada Objeto JornadaTurma com horários e intervalos regulares.
     */
    void setJornadaPadrao(JornadaTurma jornada) { jornada_padrao_ = std::move(jornada); }

    /**
     * @brief Obtém o mapa de exceções de jornada cadastradas por dia da semana.
     * @return Referência constante para o mapa de exceções.
     */
    const std::map<DayOfWeek, JornadaTurma>& excecoesPorDia() const noexcept { return excecoes_por_dia_; }

    /**
     * @brief Define todas as exceções de jornada por dia da semana.
     * @param exc Mapa associando dias específicos a jornadas diferenciadas.
     */
    void setExcecoesPorDia(std::map<DayOfWeek, JornadaTurma> exc) { excecoes_por_dia_ = std::move(exc); }

    /**
     * @brief Define uma exceção de jornada para um dia específico da semana.
     * @param d Dia da semana com horário diferenciado.
     * @param jornada JornadaTurma específica para aquele dia.
     */
    void setExcecaoDia(DayOfWeek d, JornadaTurma jornada) { excecoes_por_dia_[d] = std::move(jornada); }

    /**
     * @brief Remove a exceção de jornada configurada para um determinado dia.
     * @param d Dia da semana cuja exceção deve ser excluída.
     */
    void removeExcecaoDia(DayOfWeek d) { excecoes_por_dia_.erase(d); }

    /**
     * @brief Remove todas as exceções de jornada cadastradas para a turma.
     */
    void clearExcecoes() { excecoes_por_dia_.clear(); }

    /**
     * @brief Verifica se a turma possui alguma exceção de jornada cadastrada.
     * @return true se houver exceções, false caso todos os dias sigam a jornada padrão.
     */
    bool temExcecoes() const noexcept { return !excecoes_por_dia_.empty(); }

    /**
     * @brief Retorna a jornada efetiva aplicável a um dia da semana (exceção se houver, ou jornada padrão).
     * @param d Dia da semana consultado.
     * @return JornadaTurma a ser utilizada no dia d.
     */
    JornadaTurma jornadaParaDia(DayOfWeek d) const;

    /**
     * @brief Obtém o ID da sala teórica predefinida para a turma (seção 3.6).
     * @return String com o ID da sala teórica fixa (vazia se o solver deve escolher).
     */
    const std::string& salaTeoricaId() const noexcept { return sala_teorica_id_; }

    /**
     * @brief Define a sala teórica fixa para a turma.
     * @param sala_id Identificador da sala selecionada.
     */
    void setSalaTeoricaId(std::string sala_id) { sala_teorica_id_ = std::move(sala_id); }

    /**
     * @brief Indica se a turma já possui uma sala teórica fixa pré-atribuída.
     * @return true se o ID da sala teórica não for vazio, false se estiver pendente de alocação.
     */
    bool temSalaTeoricaPredefinida() const noexcept { return !sala_teorica_id_.empty(); }

    /**
     * @brief Obtém a lista de IDs de salas teóricas candidatas/permitidas para a turma.
     * @return Referência constante para o vetor de IDs de salas permitidas.
     */
    const std::vector<std::string>& salasTeoricasPermitidas() const noexcept { return salas_teoricas_permitidas_; }

    /**
     * @brief Define o conjunto de salas teóricas permitidas para a turma.
     * @param salas Vetor de IDs de salas candidatas.
     */
    void setSalasTeoricasPermitidas(std::vector<std::string> salas) { salas_teoricas_permitidas_ = std::move(salas); }

    /**
     * @brief Adiciona uma sala à lista de salas teóricas permitidas para a turma.
     * @param sala_id Identificador da sala a ser permitida.
     */
    void addSalaTeoricaPermitida(std::string sala_id) { salas_teoricas_permitidas_.push_back(std::move(sala_id)); }

private:
    std::string id_;
    std::string nome_;
    ShiftType turno_{ShiftType::MANHA};
    std::vector<DayOfWeek> dias_letivos_{allDays()};
    JornadaTurma jornada_padrao_;
    std::map<DayOfWeek, JornadaTurma> excecoes_por_dia_;
    std::string sala_teorica_id_; // Se vazio, solver escolhe de salas_teoricas_permitidas
    std::vector<std::string> salas_teoricas_permitidas_;
};

} // namespace scheduler::model
