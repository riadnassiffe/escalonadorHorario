#pragma once

/**
 * @file TimeSlot.h
 * @brief Tipos, constantes e funções utilitárias para representação temporal e intervalos em slots de 5 minutos.
 */

#include <string>
#include <vector>
#include <stdexcept>

namespace scheduler::model {

/** @brief Duração em minutos de cada slot de tempo atômico. */
constexpr int SLOT_MINUTES = 5;

/** @brief Quantidade de slots de tempo por hora (12 slots). */
constexpr int SLOTS_PER_HOUR = 60 / SLOT_MINUTES; // 12

/** @brief Quantidade de slots de tempo por dia completo de 24 horas (288 slots). */
constexpr int SLOTS_PER_DAY = 24 * SLOTS_PER_HOUR; // 288

/**
 * @enum DayOfWeek
 * @brief Representa os dias letivos da semana considerados no escalonamento (segunda a sexta).
 */
enum class DayOfWeek {
    SEG = 0, ///< Segunda-feira
    TER = 1, ///< Terça-feira
    QUA = 2, ///< Quarta-feira
    QUI = 3, ///< Quinta-feira
    SEX = 4  ///< Sexta-feira
};

/**
 * @brief Converte o enum DayOfWeek em sigla textual ("SEG", "TER", "QUA", "QUI", "SEX").
 * @param day Dia da semana a ser convertido.
 * @return String com a sigla de 3 letras do dia da semana.
 */
std::string dayToString(DayOfWeek day);

/**
 * @brief Converte o enum DayOfWeek em nome por extenso em português ("Segunda-feira", etc.).
 * @param day Dia da semana a ser convertido.
 * @return String com o nome completo do dia da semana.
 */
std::string dayToLongString(DayOfWeek day);

/**
 * @brief Converte uma representação textual no enum DayOfWeek correspondente.
 * @param str Sigla ou nome do dia (ex: "SEG", "Segunda", "Segunda-feira").
 * @return Valor correspondente de DayOfWeek.
 */
DayOfWeek stringToDay(const std::string& str);

/**
 * @brief Retorna um vetor contendo todos os dias úteis letivos ordenados de SEG a SEX.
 * @return Referência constante para o vetor de DayOfWeek.
 */
const std::vector<DayOfWeek>& allDays();

/**
 * @enum ShiftType
 * @brief Representa os turnos de funcionamento da instituição de ensino.
 */
enum class ShiftType {
    MANHA,    ///< Turno matutino (geralmente até 12:30)
    TARDE,    ///< Turno vespertino (12:30 às 18:00)
    NOITE,    ///< Turno noturno (após 18:00)
    INTEGRAL  ///< Turno integral (abrange múltiplos turnos)
};

/**
 * @brief Converte o enum ShiftType para sua representação textual ("MANHA", "TARDE", "NOITE", "INTEGRAL").
 * @param shift Turno a ser convertido.
 * @return String correspondente ao turno.
 */
std::string shiftToString(ShiftType shift);

/**
 * @brief Converte uma string no enum ShiftType correspondente.
 * @param str Nome do turno (ex: "MANHA", "Manhã", "TARDE", "NOITE", "INTEGRAL").
 * @return Valor correspondente de ShiftType.
 */
ShiftType stringToShift(const std::string& str);

/**
 * @brief Retorna os três turnos de trabalho padrão (MANHA, TARDE, NOITE).
 * @return Referência constante para vetor com os turnos de trabalho.
 */
const std::vector<ShiftType>& allWorkShifts();

/**
 * @brief Determina o turno correspondente a um slot específico do dia.
 * @param slot Índice do slot (0 a 287).
 * @return ShiftType associado ao slot.
 */
ShiftType slotToShift(int slot);

/**
 * @struct TimeInterval
 * @brief Representa um intervalo contínuo de tempo semiaberto [start_slot, end_slot).
 *
 * Utiliza slots discretos de 5 minutos, onde o slot 0 representa 00:00 e o slot 287 representa 23:55-24:00.
 */
struct TimeInterval {
    int start_slot{0}; ///< Índice do slot inicial do intervalo (inclusivo).
    int end_slot{0};   ///< Índice do slot final do intervalo (exclusivo).

    /**
     * @brief Construtor padrão, inicializa intervalo vazio [0, 0).
     */
    TimeInterval() = default;

    /**
     * @brief Construtor a partir dos índices de slots inicial e final.
     * @param start Slot inicial (inclusivo).
     * @param end Slot final (exclusivo).
     * @throws std::invalid_argument se start > end.
     */
    TimeInterval(int start, int end) : start_slot(start), end_slot(end) {
        if (start > end) {
            throw std::invalid_argument("TimeInterval: start_slot cannot be greater than end_slot");
        }
    }

    /**
     * @brief Construtor a partir de horários no formato "HH:MM".
     * @param start_str Horário inicial (ex: "07:30").
     * @param end_str Horário final (ex: "08:15").
     * @throws std::invalid_argument se o formato for inválido ou se start > end.
     */
    TimeInterval(const std::string& start_str, const std::string& end_str);

    /**
     * @brief Calcula a duração do intervalo em quantidade de slots de 5 minutos.
     * @return Número de slots de duração.
     */
    int durationSlots() const noexcept { return end_slot - start_slot; }

    /**
     * @brief Calcula a duração do intervalo em minutos.
     * @return Duração total em minutos.
     */
    int durationMinutes() const noexcept { return durationSlots() * SLOT_MINUTES; }

    /**
     * @brief Verifica se um determinado slot está contido no intervalo.
     * @param slot Índice do slot testado.
     * @return true se o slot pertencer a [start_slot, end_slot), false caso contrário.
     */
    bool contains(int slot) const noexcept {
        return slot >= start_slot && slot < end_slot;
    }

    /**
     * @brief Verifica se outro intervalo está totalmente contido neste intervalo.
     * @param other Intervalo a ser verificado.
     * @return true se other estiver inteiramente dentro deste intervalo, false caso contrário.
     */
    bool contains(const TimeInterval& other) const noexcept {
        return other.start_slot >= start_slot && other.end_slot <= end_slot;
    }

    /**
     * @brief Verifica se há sobreposição (interseção não nula) com outro intervalo de tempo.
     * @param other Intervalo a ser comparado.
     * @return true se houver interseção temporal, false caso contrário.
     */
    bool overlaps(const TimeInterval& other) const noexcept {
        return start_slot < other.end_slot && other.start_slot < end_slot;
    }

    /**
     * @brief Retorna a string do horário inicial no formato "HH:MM".
     * @return String formatada do horário de início.
     */
    std::string startString() const;

    /**
     * @brief Retorna a string do horário final no formato "HH:MM".
     * @return String formatada do horário de término.
     */
    std::string endString() const;

    /**
     * @brief Retorna a representação textual do intervalo no formato "HH:MM - HH:MM".
     * @return String formatada do intervalo.
     */
    std::string toString() const;

    /**
     * @brief Converte uma string "HH:MM" no índice de slot correspondente (0 a 287).
     * @param time_str String no formato "HH:MM".
     * @return Índice inteiro do slot de 5 minutos.
     * @throws std::invalid_argument se o formato ou valores de hora/minuto forem inválidos.
     */
    static int timeStringToSlot(const std::string& time_str);

    /**
     * @brief Converte um índice de slot em string formatada "HH:MM".
     * @param slot Índice do slot (0 a 287).
     * @return String formatada "HH:MM".
     */
    static std::string slotToTimeString(int slot);
};

} // namespace scheduler::model
