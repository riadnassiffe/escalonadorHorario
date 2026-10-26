/**
 * @file TimeSlot.cpp
 * @brief Implementação das funções utilitárias de manipulação de dias, turnos e intervalos de tempo.
 */

#include "model/TimeSlot.h"
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace scheduler::model {

/**
 * @brief Converte o enum DayOfWeek em sigla textual ("SEG", "TER", "QUA", "QUI", "SEX").
 * @param day Dia da semana a ser convertido.
 * @return String com a sigla de 3 letras do dia da semana.
 */
std::string dayToString(DayOfWeek day) {
    switch (day) {
        case DayOfWeek::SEG: return "SEG";
        case DayOfWeek::TER: return "TER";
        case DayOfWeek::QUA: return "QUA";
        case DayOfWeek::QUI: return "QUI";
        case DayOfWeek::SEX: return "SEX";
    }
    return "SEG";
}

/**
 * @brief Converte o enum DayOfWeek em nome por extenso em português ("Segunda-feira", etc.).
 * @param day Dia da semana a ser convertido.
 * @return String com o nome completo do dia da semana.
 */
std::string dayToLongString(DayOfWeek day) {
    switch (day) {
        case DayOfWeek::SEG: return "Segunda-feira";
        case DayOfWeek::TER: return "Terça-feira";
        case DayOfWeek::QUA: return "Quarta-feira";
        case DayOfWeek::QUI: return "Quinta-feira";
        case DayOfWeek::SEX: return "Sexta-feira";
    }
    return "Segunda-feira";
}

/**
 * @brief Converte uma representação textual no enum DayOfWeek correspondente.
 * @param str Sigla ou nome do dia (ex: "SEG", "Segunda", "Segunda-feira").
 * @return Valor correspondente de DayOfWeek.
 */
DayOfWeek stringToDay(const std::string& str) {
    if (str == "SEG" || str == "seg" || str == "Segunda" || str == "Segunda-feira") return DayOfWeek::SEG;
    if (str == "TER" || str == "ter" || str == "Terca" || str == "Terça" || str == "Terça-feira") return DayOfWeek::TER;
    if (str == "QUA" || str == "qua" || str == "Quarta" || str == "Quarta-feira") return DayOfWeek::QUA;
    if (str == "QUI" || str == "qui" || str == "Quinta" || str == "Quinta-feira") return DayOfWeek::QUI;
    if (str == "SEX" || str == "sex" || str == "Sexta" || str == "Sexta-feira") return DayOfWeek::SEX;
    return DayOfWeek::SEG;
}

/**
 * @brief Retorna um vetor contendo todos os dias úteis letivos ordenados de SEG a SEX.
 * @return Referência constante para o vetor estático de DayOfWeek.
 */
const std::vector<DayOfWeek>& allDays() {
    static const std::vector<DayOfWeek> days = {
        DayOfWeek::SEG,
        DayOfWeek::TER,
        DayOfWeek::QUA,
        DayOfWeek::QUI,
        DayOfWeek::SEX
    };
    return days;
}

/**
 * @brief Converte o enum ShiftType para sua representação textual ("MANHA", "TARDE", "NOITE", "INTEGRAL").
 * @param shift Turno a ser convertido.
 * @return String correspondente ao turno.
 */
std::string shiftToString(ShiftType shift) {
    switch (shift) {
        case ShiftType::MANHA: return "MANHA";
        case ShiftType::TARDE: return "TARDE";
        case ShiftType::NOITE: return "NOITE";
        case ShiftType::INTEGRAL: return "INTEGRAL";
    }
    return "MANHA";
}

/**
 * @brief Converte uma string no enum ShiftType correspondente.
 * @param str Nome do turno (ex: "MANHA", "Manhã", "TARDE", "NOITE", "INTEGRAL").
 * @return Valor correspondente de ShiftType.
 */
ShiftType stringToShift(const std::string& str) {
    if (str == "MANHA" || str == "Manhã" || str == "manha") return ShiftType::MANHA;
    if (str == "TARDE" || str == "Tarde" || str == "tarde") return ShiftType::TARDE;
    if (str == "NOITE" || str == "Noite" || str == "noite") return ShiftType::NOITE;
    if (str == "INTEGRAL" || str == "Integral" || str == "integral") return ShiftType::INTEGRAL;
    return ShiftType::MANHA;
}

/**
 * @brief Retorna os três turnos de trabalho padrão (MANHA, TARDE, NOITE).
 * @return Referência constante para vetor estático com os turnos de trabalho.
 */
const std::vector<ShiftType>& allWorkShifts() {
    static const std::vector<ShiftType> shifts = {
        ShiftType::MANHA,
        ShiftType::TARDE,
        ShiftType::NOITE
    };
    return shifts;
}

/**
 * @brief Determina o turno correspondente a um slot específico do dia.
 * @param slot Índice do slot (0 a 287).
 * @return ShiftType associado ao slot.
 */
ShiftType slotToShift(int slot) {
    constexpr int SLOT_12_30 = 150; // 12h30 = 750min / 5
    constexpr int SLOT_18_00 = 216; // 18h00 = 1080min / 5
    if (slot < SLOT_12_30) return ShiftType::MANHA;
    if (slot < SLOT_18_00) return ShiftType::TARDE;
    return ShiftType::NOITE;
}

/**
 * @brief Construtor a partir de horários em formato de texto "HH:MM".
 * @param start_str Horário de início (ex: "07:30").
 * @param end_str Horário de término (ex: "08:15").
 * @throws std::invalid_argument se o início for posterior ao término.
 */
TimeInterval::TimeInterval(const std::string& start_str, const std::string& end_str)
    : start_slot(timeStringToSlot(start_str)), end_slot(timeStringToSlot(end_str)) {
    if (start_slot > end_slot) {
        throw std::invalid_argument("TimeInterval: start cannot be after end: " + start_str + " > " + end_str);
    }
}

/**
 * @brief Converte uma string no formato "HH:MM" no índice correspondente de slot de 5 minutos.
 * @param time_str String representando hora e minuto (ex: "08:30").
 * @return Índice inteiro do slot (0 a 287).
 * @throws std::invalid_argument se o formato ou valores forem inválidos.
 */
int TimeInterval::timeStringToSlot(const std::string& time_str) {
    int h = 0, m = 0;
    if (std::sscanf(time_str.c_str(), "%d:%d", &h, &m) != 2) {
        throw std::invalid_argument("Invalid time string: " + time_str);
    }
    if (h < 0 || h > 23 || m < 0 || m > 59) {
        throw std::invalid_argument("Time out of range: " + time_str);
    }
    int total_minutes = h * 60 + m;
    return total_minutes / SLOT_MINUTES;
}

/**
 * @brief Converte um índice de slot em string formatada "HH:MM".
 * @param slot Índice inteiro do slot (0 a 287).
 * @return String com a hora e minuto formatados (ex: "08:30").
 */
std::string TimeInterval::slotToTimeString(int slot) {
    if (slot < 0) slot = 0;
    int total_minutes = slot * SLOT_MINUTES;
    int h = (total_minutes / 60) % 24;
    int m = total_minutes % 60;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
    return std::string(buf);
}

/**
 * @brief Retorna a string do horário inicial formatado "HH:MM".
 * @return String formatada do início do intervalo.
 */
std::string TimeInterval::startString() const {
    return slotToTimeString(start_slot);
}

/**
 * @brief Retorna a string do horário final formatado "HH:MM".
 * @return String formatada do fim do intervalo.
 */
std::string TimeInterval::endString() const {
    return slotToTimeString(end_slot);
}

/**
 * @brief Retorna a representação textual do intervalo ("HH:MM - HH:MM").
 * @return String formatada do intervalo completo.
 */
std::string TimeInterval::toString() const {
    return startString() + " - " + endString();
}

} // namespace scheduler::model
