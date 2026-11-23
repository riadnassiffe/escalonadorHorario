#pragma once

/**
 * @file Turma.h
 * @brief Definições da struct JornadaTurma e da classe Turma, gerenciando slots letivos configuráveis e turmas.
 */

#include "model/TimeSlot.h"
#include <string>
#include <vector>
#include <map>
#include <optional>

namespace scheduler::model {

/**
 * @struct JornadaTurma
 * @brief Modela a grade diária de horários de uma turma como uma lista ordenada de slots letivos.
 *
 * Cada slot representa um intervalo [start_min, end_min) em que uma aula com duração exata
 * pode ser alocada. As lacunas entre o fim de um slot e o início do próximo representam
 * automaticamente os intervalos de recreio ou almoço.
 */
struct JornadaTurma {
    std::vector<TimeInterval> slots_letivos; ///< Slots letivos do dia ordenados cronologicamente e disjuntos.

    /**
     * @brief Construtor padrão (jornada vazia sem slots).
     */
    JornadaTurma() = default;

    /**
     * @brief Construtor a partir de uma lista explícita de slots letivos.
     * @param slots_list Lista de intervalos [início, fim) dos slots letivos.
     */
    explicit JornadaTurma(std::vector<TimeInterval> slots_list);

    /**
     * @brief Construtor legado de conveniência com 2 intervalos ativos (converte blocos em slots).
     */
    JornadaTurma(const std::string& ini, const std::string& fim,
                 const std::string& int1_ini, const std::string& int1_fim,
                 const std::string& int2_ini, const std::string& int2_fim);

    /**
     * @brief Construtor legado configurando blocos e até 3 intervalos (converte blocos em slots).
     */
    JornadaTurma(const std::string& ini, const std::string& fim,
                 const std::string& int1_ini, const std::string& int1_fim, bool int1_ativo,
                 const std::string& int2_ini, const std::string& int2_fim, bool int2_ativo,
                 const std::string& int3_ini, const std::string& int3_fim, bool int3_ativo);

    /**
     * @brief Ordena os slots cronologicamente por horário de início e fim.
     */
    void ordenarSlots();

    /**
     * @brief Valida a consistência dos slots (não vazio, start < end e sem sobreposição entre slots).
     * @return true se a lista de slots for válida; false caso contrário.
     */
    bool isValid() const noexcept;

    /**
     * @brief Retorna o intervalo envolvente da jornada total [primeiro_ini, ultimo_fim).
     * @return TimeInterval abrangendo do início do primeiro slot ao término do último.
     */
    TimeInterval jornadaTotal() const noexcept;

    /**
     * @brief Calcula automaticamente os intervalos/recreios a partir das lacunas entre slots consecutivos.
     * @return Vetor com os intervalos de descanso [slots[k].end_min, slots[k+1].start_min).
     */
    std::vector<TimeInterval> intervalosAtivos() const;

    /**
     * @brief Soma total de minutos letivos disponíveis nos slots do dia.
     * @return Total de minutos letivos.
     */
    int minutosLetivos() const noexcept;

    /**
     * @brief Verifica se o intervalo informado coincide exatamente com algum slot cadastrado.
     * @param target Intervalo [start_min, end_min) testado.
     * @return true se existir um slot idêntico em slots.
     */
    bool contemSlotExato(const TimeInterval& target) const noexcept;

    /**
     * @brief Verifica se um minuto do dia cai dentro de algum intervalo de recreio entre slots.
     * @param minute Minuto desde 00:00.
     * @return true se estiver em uma lacuna entre dois slots letivos.
     */
    bool isMinutoEmIntervalo(int minute) const noexcept;

    /**
     * @brief Gera uma lista padrão de slots letivos para o turno especificado.
     * @param shift Turno desejado.
     * @return Vetor de TimeInterval com os slots padrão.
     */
    static std::vector<TimeInterval> defaultSlotsForShift(ShiftType shift);

    /**
     * @brief Particiona blocos contínuos legados em slots individuais de aula (50/45 min).
     * @param blocos Blocos contínuos de horário letivo.
     * @param duracao_preferencial Duração preferencial em minutos (ex: 50 ou 45).
     * @return Vetor ordenado de slots letivos.
     */
    static std::vector<TimeInterval> gerarSlotsDeBlocos(const std::vector<TimeInterval>& blocos,
                                                        int duracao_preferencial = 50);

    bool operator==(const JornadaTurma& o) const noexcept { return slots_letivos == o.slots_letivos; }
    bool operator!=(const JornadaTurma& o) const noexcept { return !(*this == o); }
};

/**
 * @class Turma
 * @brief Representa um agrupamento de estudantes (classe escolar) com grade de slots e restrições de sala.
 */
class Turma {
public:
    Turma() = default;

    /**
     * @brief Construtor parametrizado de Turma com slots padrão compatíveis com o turno.
     * @param id Identificador único da turma (ex: "T01").
     * @param nome Nome ou descrição da turma (ex: "1º Ano A - Ensino Médio").
     * @param turno Turno de estudos da turma (padrão: ShiftType::MANHA).
     */
    Turma(std::string id, std::string nome, ShiftType turno = ShiftType::MANHA);

    const std::string& id() const noexcept { return id_; }
    void setId(std::string id) { id_ = std::move(id); }

    const std::string& nome() const noexcept { return nome_; }
    void setNome(std::string nome) { nome_ = std::move(nome); }

    ShiftType turno() const noexcept { return turno_; }
    void setTurno(ShiftType turno) noexcept { turno_ = turno; }

    const std::vector<DayOfWeek>& diasLetivos() const noexcept { return dias_letivos_; }
    void setDiasLetivos(std::vector<DayOfWeek> dias) { dias_letivos_ = std::move(dias); }

    bool funcionaNoDia(DayOfWeek d) const noexcept;

    const JornadaTurma& jornadaPadrao() const noexcept { return jornada_padrao_; }
    void setJornadaPadrao(JornadaTurma jornada) {
        jornada.ordenarSlots();
        jornada_padrao_ = std::move(jornada);
    }

    const std::map<DayOfWeek, JornadaTurma>& excecoesPorDia() const noexcept { return excecoes_por_dia_; }
    void setExcecoesPorDia(std::map<DayOfWeek, JornadaTurma> exc) {
        for (auto& [d, j] : exc) {
            (void)d;
            j.ordenarSlots();
        }
        excecoes_por_dia_ = std::move(exc);
    }

    void setExcecaoDia(DayOfWeek d, JornadaTurma jornada) {
        jornada.ordenarSlots();
        excecoes_por_dia_[d] = std::move(jornada);
    }
    void removeExcecaoDia(DayOfWeek d) { excecoes_por_dia_.erase(d); }
    void clearExcecoes() { excecoes_por_dia_.clear(); }
    bool temExcecoes() const noexcept { return !excecoes_por_dia_.empty(); }

    JornadaTurma jornadaParaDia(DayOfWeek d) const;

    const std::string& salaTeoricaId() const noexcept { return sala_teorica_id_; }
    void setSalaTeoricaId(std::string sala_id) { sala_teorica_id_ = std::move(sala_id); }
    bool temSalaTeoricaPredefinida() const noexcept { return !sala_teorica_id_.empty(); }

    const std::vector<std::string>& salasTeoricasPermitidas() const noexcept { return salas_teoricas_permitidas_; }
    void setSalasTeoricasPermitidas(std::vector<std::string> salas) { salas_teoricas_permitidas_ = std::move(salas); }
    void addSalaTeoricaPermitida(std::string sala_id) { salas_teoricas_permitidas_.push_back(std::move(sala_id)); }

private:
    std::string id_;
    std::string nome_;
    ShiftType turno_{ShiftType::MANHA};
    std::vector<DayOfWeek> dias_letivos_{allDays()};
    JornadaTurma jornada_padrao_;
    std::map<DayOfWeek, JornadaTurma> excecoes_por_dia_;
    std::string sala_teorica_id_;
    std::vector<std::string> salas_teoricas_permitidas_;
};

} // namespace scheduler::model
