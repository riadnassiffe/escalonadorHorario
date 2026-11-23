/**
 * @file Timetable.cpp
 * @brief Implementação dos métodos de filtragem e busca da classe Timetable.
 */

#include "model/Timetable.h"
#include <algorithm>

namespace scheduler::model {

/**
 * @brief Filtra e retorna as aulas alocadas para uma turma em um dia da semana, ordenadas cronologicamente.
 * @param turma_id Identificador da turma pesquisada.
 * @param d Dia da semana filtrado.
 * @param semestre_filtro Filtro semestral (0 = sem filtro, 1 = 1º sem, 2 = 2º sem).
 * @return Vetor de AllocatedLesson ordenado pelo slot inicial.
 */
std::vector<AllocatedLesson> Timetable::aulasPorTurma(const std::string& turma_id, DayOfWeek d, int semestre_filtro) const {
    std::vector<AllocatedLesson> res;
    for (const auto& aula : alocacoes_) {
        if (aula.dia == d) {
            if (semestre_filtro > 0) {
                bool match_sem = (aula.regime == RegimeDisciplina::ANUAL) ||
                                 (aula.semestre_alocado == semestre_filtro) ||
                                 (aula.semestre == SemestreLetivo::AMBOS);
                if (!match_sem) continue;
            }
            if (std::find(aula.turmas.begin(), aula.turmas.end(), turma_id) != aula.turmas.end()) {
                res.push_back(aula);
            }
        }
    }
    std::sort(res.begin(), res.end(), [](const AllocatedLesson& a, const AllocatedLesson& b) {
        return a.intervalo.start_min < b.intervalo.start_min;
    });
    return res;
}

/**
 * @brief Filtra e retorna as aulas lecionadas por um professor em um dia da semana, ordenadas cronologicamente.
 * @param prof_id Identificador do professor pesquisado.
 * @param d Dia da semana filtrado.
 * @param semestre_filtro Filtro semestral (0 = sem filtro, 1 = 1º sem, 2 = 2º sem).
 * @return Vetor de AllocatedLesson ordenado pelo slot inicial.
 */
std::vector<AllocatedLesson> Timetable::aulasPorProfessor(const std::string& prof_id, DayOfWeek d, int semestre_filtro) const {
    std::vector<AllocatedLesson> res;
    for (const auto& aula : alocacoes_) {
        if (aula.dia == d) {
            if (semestre_filtro > 0) {
                bool match_sem = (aula.regime == RegimeDisciplina::ANUAL) ||
                                 (aula.semestre_alocado == semestre_filtro) ||
                                 (aula.semestre == SemestreLetivo::AMBOS);
                if (!match_sem) continue;
            }
            if (std::find(aula.professores.begin(), aula.professores.end(), prof_id) != aula.professores.end()) {
                res.push_back(aula);
            }
        }
    }
    std::sort(res.begin(), res.end(), [](const AllocatedLesson& a, const AllocatedLesson& b) {
        return a.intervalo.start_min < b.intervalo.start_min;
    });
    return res;
}

/**
 * @brief Filtra e retorna as aulas alocadas em uma sala em um dia da semana, ordenadas cronologicamente.
 * @param sala_id Identificador da sala pesquisada.
 * @param d Dia da semana filtrado.
 * @param semestre_filtro Filtro semestral (0 = sem filtro, 1 = 1º sem, 2 = 2º sem).
 * @return Vetor de AllocatedLesson ordenado pelo slot inicial.
 */
std::vector<AllocatedLesson> Timetable::aulasPorSala(const std::string& sala_id, DayOfWeek d, int semestre_filtro) const {
    std::vector<AllocatedLesson> res;
    for (const auto& aula : alocacoes_) {
        if (aula.dia == d) {
            if (semestre_filtro > 0) {
                bool match_sem = (aula.regime == RegimeDisciplina::ANUAL) ||
                                 (aula.semestre_alocado == semestre_filtro) ||
                                 (aula.semestre == SemestreLetivo::AMBOS);
                if (!match_sem) continue;
            }
            if (std::find(aula.salas.begin(), aula.salas.end(), sala_id) != aula.salas.end()) {
                res.push_back(aula);
            }
        }
    }
    std::sort(res.begin(), res.end(), [](const AllocatedLesson& a, const AllocatedLesson& b) {
        return a.intervalo.start_min < b.intervalo.start_min;
    });
    return res;
}

/**
 * @brief Busca uma aula alocada na grade pelo seu ID.
 * @param aula_id Identificador único da aula alocada.
 * @return Ponteiro mutável para a AllocatedLesson encontrada, ou nullptr caso não exista.
 */
AllocatedLesson* Timetable::findAula(const std::string& aula_id) {
    for (auto& aula : alocacoes_) {
        if (aula.aula_id == aula_id) {
            return &aula;
        }
    }
    return nullptr;
}

const AllocatedLesson* Timetable::findAula(const std::string& aula_id) const {
    for (const auto& aula : alocacoes_) {
        if (aula.aula_id == aula_id) {
            return &aula;
        }
    }
    return nullptr;
}

} // namespace scheduler::model
