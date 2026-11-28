#pragma once

/**
 * @file TimeSlot.h
 * @brief Tipos, constantes e funções utilitárias para representação temporal em minutos [0, 1440).
 */

#include <string>
#include <vector>
#include <stdexcept>

namespace scheduler::model {

/** @brief Quantidade total de minutos em um dia completo de 24 horas (1440 minutos). */
constexpr int MINUTES_PER_DAY = 24 * 60; // 1440

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
 * @brief Determina o turno correspondente a um instante em minutos desde a meia-noite.
 * @param minutes Minutos desde 00:00 (0 a 1439).
 * @return ShiftType associado ao horário.
 */
ShiftType timeToShift(int minutes);

/**
 * @struct TimeInterval
 * @brief Representa um intervalo contínuo de tempo semiaberto [start_min, end_min) em minutos desde 00:00.
 *
 * Exemplo: 07:30 às 08:20 corresponde a [450, 500) com durationMinutes() == 50.
 */
struct TimeInterval {
    int start_min{0}; ///< Minuto inicial do intervalo (inclusivo, 0..1440).
    int end_min{0};   ///< Minuto final do intervalo (exclusivo, 0..1440).

    /**
     * @brief Construtor padrão, inicializa intervalo vazio [0, 0).
     */
    TimeInterval() = default;

    /**
     * @brief Construtor a partir dos minutos inicial e final desde a meia-noite.
     * @param start Minuto inicial (inclusivo).
     * @param end Minuto final (exclusivo).
     * @throws std::invalid_argument se start > end.
     */
    TimeInterval(int start, int end) : start_min(start), end_min(end) {
        if (start > end) {
            throw std::invalid_argument("TimeInterval: start_min cannot be greater than end_min");
        }
    }

    /**
     * @brief Construtor a partir de horários no formato "HH:MM".
     * @param start_str Horário inicial (ex: "07:30").
     * @param end_str Horário final (ex: "08:20").
     * @throws std::invalid_argument se o formato for inválido ou se start > end.
     */
    TimeInterval(const std::string& start_str, const std::string& end_str);

    /**
     * @brief Calcula a duração do intervalo em minutos.
     * @return Duração total em minutos.
     */
    int durationMinutes() const noexcept { return end_min - start_min; }

    /**
     * @brief Verifica se um determinado minuto está contido no intervalo.
     * @param minute Minuto testado (desde 00:00).
     * @return true se o minuto pertencer a [start_min, end_min), false caso contrário.
     */
    bool contains(int minute) const noexcept {
        return minute >= start_min && minute < end_min;
    }

    /**
     * @brief Verifica se outro intervalo está totalmente contido neste intervalo.
     * @param other Intervalo a ser verificado.
     * @return true se other estiver inteiramente dentro deste intervalo, false caso contrário.
     */
    bool contains(const TimeInterval& other) const noexcept {
        return other.start_min >= start_min && other.end_min <= end_min;
    }

    /**
     * @brief Verifica se há sobreposição (interseção não nula) com outro intervalo de tempo.
     * @param other Intervalo a ser comparado.
     * @return true se houver interseção temporal, false caso contrário.
     */
    bool overlaps(const TimeInterval& other) const noexcept {
        return start_min < other.end_min && other.start_min < end_min;
    }

    bool operator==(const TimeInterval& o) const noexcept {
        return start_min == o.start_min && end_min == o.end_min;
    }

    bool operator!=(const TimeInterval& o) const noexcept {
        return !(*this == o);
    }

    bool operator<(const TimeInterval& o) const noexcept {
        return start_min != o.start_min ? start_min < o.start_min : end_min < o.end_min;
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
     * @brief Converte uma string "HH:MM" no total de minutos desde 00:00 (0 a 1440).
     * @param time_str String no formato "HH:MM".
     * @return Minutos inteiros desde 00:00.
     * @throws std::invalid_argument se o formato ou valores de hora/minuto forem inválidos.
     */
    static int timeStringToMinutes(const std::string& time_str);

    /**
     * @brief Converte um valor em minutos desde 00:00 em string formatada "HH:MM".
     * @param minutes Minutos desde 00:00 (0 a 1440).
     * @return String formatada "HH:MM".
     */
    static std::string minutesToTimeString(int minutes);
};

/**
 * @brief Formata uma duração em segundos no formato "HHh MMm SSs".
 */
std::string formatDurationSeconds(double total_seconds);

/**
 * @brief Formata uma duração em milissegundos em horas, minutos e segundos ("HHh MMm SSs" ou "00h 00m 00.XXs" para < 995 ms).
 */
std::string formatDurationMs(double duration_ms);

/**
 * @brief Converte uma duração em segundos para componentes inteiros de horas, minutos e segundos.
 */
void secondsToHms(double total_seconds, int& hours, int& minutes, int& seconds);

/**
 * @brief Converte horas, minutos e segundos para total em segundos (mínimo 1.0 s).
 */
double hmsToSeconds(int hours, int minutes, int seconds);

} // namespace scheduler::model
