#include "model/Timetable.h"
#include <algorithm>

namespace scheduler::model {

std::vector<AllocatedLesson> Timetable::aulasPorTurma(const std::string& turma_id, DayOfWeek d) const {
    std::vector<AllocatedLesson> res;
    for (const auto& aula : alocacoes_) {
        if (aula.dia == d) {
            if (std::find(aula.turmas.begin(), aula.turmas.end(), turma_id) != aula.turmas.end()) {
                res.push_back(aula);
            }
        }
    }
    std::sort(res.begin(), res.end(), [](const AllocatedLesson& a, const AllocatedLesson& b) {
        return a.intervalo.start_slot < b.intervalo.start_slot;
    });
    return res;
}

std::vector<AllocatedLesson> Timetable::aulasPorProfessor(const std::string& prof_id, DayOfWeek d) const {
    std::vector<AllocatedLesson> res;
    for (const auto& aula : alocacoes_) {
        if (aula.dia == d) {
            if (std::find(aula.professores.begin(), aula.professores.end(), prof_id) != aula.professores.end()) {
                res.push_back(aula);
            }
        }
    }
    std::sort(res.begin(), res.end(), [](const AllocatedLesson& a, const AllocatedLesson& b) {
        return a.intervalo.start_slot < b.intervalo.start_slot;
    });
    return res;
}

std::vector<AllocatedLesson> Timetable::aulasPorSala(const std::string& sala_id, DayOfWeek d) const {
    std::vector<AllocatedLesson> res;
    for (const auto& aula : alocacoes_) {
        if (aula.dia == d) {
            if (std::find(aula.salas.begin(), aula.salas.end(), sala_id) != aula.salas.end()) {
                res.push_back(aula);
            }
        }
    }
    std::sort(res.begin(), res.end(), [](const AllocatedLesson& a, const AllocatedLesson& b) {
        return a.intervalo.start_slot < b.intervalo.start_slot;
    });
    return res;
}

AllocatedLesson* Timetable::findAula(const std::string& aula_id) {
    for (auto& aula : alocacoes_) {
        if (aula.aula_id == aula_id) {
            return &aula;
        }
    }
    return nullptr;
}

} // namespace scheduler::model
