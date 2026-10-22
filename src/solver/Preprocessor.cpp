#include "solver/Preprocessor.h"
#include <algorithm>
#include <sstream>

namespace scheduler::solver {

ValidationResult Preprocessor::validarDados(const model::InstitutionData& data) {
    ValidationResult res;

    if (data.professores().empty()) {
        res.addError("Nenhum professor cadastrado no sistema.");
    }
    if (data.turmas().empty()) {
        res.addError("Nenhuma turma cadastrada no sistema.");
    }
    if (data.salas().empty()) {
        res.addError("Nenhuma sala cadastrada no sistema.");
    }
    if (data.disciplinas().empty()) {
        res.addError("Nenhuma disciplina cadastrada no sistema.");
    }

    // Validar turmas
    for (const auto& turma : data.turmas()) {
        const auto& jp = turma.jornadaPadrao();
        if (!jp.isValid()) {
            res.addError("Turma " + turma.id() + " (" + turma.nome() +
                         ") possui jornada ou intervalos inválidos. Condição: T_ini < b1 < e1 <= b2 < e2 < T_fim.");
        }

        // Verificar exceções por dia
        for (const auto& [dia, j_exc] : turma.excecoesPorDia()) {
            if (!j_exc.isValid()) {
                res.addError("Turma " + turma.id() + " possui exceção inválida para o dia " +
                             model::dayToString(dia) + ".");
            }
        }

        // Verificar sala teórica
        if (turma.temSalaTeoricaPredefinida()) {
            if (!data.findSala(turma.salaTeoricaId())) {
                res.addError("Turma " + turma.id() + " referencia sala teórica inexistente: " +
                             turma.salaTeoricaId());
            }
        } else {
            if (turma.salasTeoricasPermitidas().empty()) {
                res.addError("Turma " + turma.id() + " não possui sala teórica fixa nem salas teóricas permitidas cadastradas.");
            } else {
                for (const auto& s_id : turma.salasTeoricasPermitidas()) {
                    if (!data.findSala(s_id)) {
                        res.addError("Turma " + turma.id() + " referencia sala permitida inexistente: " + s_id);
                    }
                }
            }
        }
    }

    // Validar professores
    for (const auto& prof : data.professores()) {
        bool has_avail = false;
        for (auto d : model::allDays()) {
            if (!prof.disponibilidade(d).empty()) {
                has_avail = true;
                break;
            }
        }
        if (!has_avail) {
            res.addWarning("Professor " + prof.id() + " (" + prof.nome() +
                           ") não possui nenhuma disponibilidade cadastrada em nenhum dia.");
        }
    }

    // Validar disciplinas
    for (const auto& disc : data.disciplinas()) {
        if (disc.professores().empty()) {
            res.addError("Disciplina " + disc.id() + " (" + disc.nome() + ") não possui professor responsável.");
        } else {
            for (const auto& p_id : disc.professores()) {
                if (!data.findProfessor(p_id)) {
                    res.addError("Disciplina " + disc.id() + " referencia professor inexistente: " + p_id);
                }
            }
        }

        if (disc.turmas().empty()) {
            res.addError("Disciplina " + disc.id() + " (" + disc.nome() + ") não está associada a nenhuma turma.");
        } else {
            for (const auto& t_id : disc.turmas()) {
                if (!data.findTurma(t_id)) {
                    res.addError("Disciplina " + disc.id() + " referencia turma inexistente: " + t_id);
                }
            }
        }

        if (disc.duracaoMinutos() <= 0 || disc.duracaoMinutos() % model::SLOT_MINUTES != 0) {
            res.addError("Disciplina " + disc.id() + " possui duração inválida (" +
                         std::to_string(disc.duracaoMinutos()) + " min). Deve ser múltiplo de 5 min.");
        }
        if (disc.aulasSemanais() <= 0) {
            res.addError("Disciplina " + disc.id() + " possui número de aulas semanais <= 0.");
        }
    }

    // Validar carga horária vs capacidade da turma
    for (const auto& turma : data.turmas()) {
        int minutos_necessarios = 0;
        for (const auto& disc : data.disciplinas()) {
            if (std::find(disc.turmas().begin(), disc.turmas().end(), turma.id()) != disc.turmas().end()) {
                minutos_necessarios += disc.duracaoMinutos() * disc.aulasSemanais();
            }
        }

        int minutos_disponiveis = 0;
        for (auto dia : turma.diasLetivos()) {
            auto jornada = turma.jornadaParaDia(dia);
            auto blocos = jornada.blocosLetivos();
            for (const auto& b : blocos) {
                minutos_disponiveis += b.durationMinutes();
            }
        }

        if (minutos_necessarios > minutos_disponiveis) {
            res.addError("Turma " + turma.id() + " necessita de " + std::to_string(minutos_necessarios) +
                         " min de aula semanais, mas só possui " + std::to_string(minutos_disponiveis) +
                         " min disponíveis em sua jornada letiva.");
        } else if (data.config().preenchimento_integral_jornada && minutos_necessarios != minutos_disponiveis) {
            res.addWarning("Preenchimento integral ativado, mas Turma " + turma.id() + " tem " +
                           std::to_string(minutos_necessarios) + " min de aulas para " +
                           std::to_string(minutos_disponiveis) + " min de blocos letivos.");
        }
    }

    return res;
}

bool Preprocessor::turmaAcomodaAula(const model::Turma& turma, model::DayOfWeek dia,
                                    int start_slot, int duration_slots) {
    if (!turma.funcionaNoDia(dia)) return false;

    auto jornada = turma.jornadaParaDia(dia);
    auto blocos = jornada.blocosLetivos();
    model::TimeInterval target(start_slot, start_slot + duration_slots);

    // Deve caber INTEIRAMENTE dentro de um dos blocos contínuos da turma (seção 3.5)
    for (const auto& bloco : blocos) {
        if (bloco.contains(target)) {
            return true;
        }
    }
    return false;
}

bool Preprocessor::shiftAcomodaAula(model::ShiftType shift, int start_slot, int duration_slots) {
    int end_slot = start_slot + duration_slots;
    switch (shift) {
        case model::ShiftType::MANHA:
            // 08:00 (slot 84) a 12:30 (slot 150)
            return start_slot >= model::TimeInterval::timeStringToSlot("08:00") &&
                   end_slot <= model::TimeInterval::timeStringToSlot("12:30");
        case model::ShiftType::TARDE:
            // 13:00 (slot 156) a 18:30 (slot 222)
            return start_slot >= model::TimeInterval::timeStringToSlot("13:00") &&
                   end_slot <= model::TimeInterval::timeStringToSlot("18:30");
        case model::ShiftType::NOITE:
            // 18:00 a 22:00
            return start_slot >= model::TimeInterval::timeStringToSlot("18:00") &&
                   end_slot <= model::TimeInterval::timeStringToSlot("22:00");
        case model::ShiftType::INTEGRAL:
            return start_slot >= model::TimeInterval::timeStringToSlot("08:00") &&
                   end_slot <= model::TimeInterval::timeStringToSlot("22:00");
    }
    return true;
}

std::vector<CandidateAllocation> Preprocessor::gerarCandidatosParaAula(
    const model::InstitutionData& data,
    const model::Lesson& aula,
    std::vector<std::string>& diagnostico_aula) {

    std::vector<CandidateAllocation> candidatos;

    // Obter turmas da aula
    std::vector<const model::Turma*> turmas_aula;
    for (const auto& t_id : aula.turmas()) {
        const auto* t = data.findTurma(t_id);
        if (t) turmas_aula.push_back(t);
    }
    if (turmas_aula.empty()) {
        diagnostico_aula.push_back("Aula " + aula.id() + ": Nenhuma turma válida encontrada.");
        return candidatos;
    }

    // Obter professores da aula
    std::vector<const model::Professor*> profs_aula;
    for (const auto& p_id : aula.professores()) {
        const auto* p = data.findProfessor(p_id);
        if (p) profs_aula.push_back(p);
    }
    if (profs_aula.empty()) {
        diagnostico_aula.push_back("Aula " + aula.id() + ": Nenhum professor válido encontrado.");
        return candidatos;
    }

    int L_a = aula.duracaoSlots();

    // Determinar combinações de salas admissíveis (seção 2.5 e 3.6)
    std::vector<std::vector<std::string>> salas_admissiveis;
    if (aula.isTeorica()) {
        // Para aula teórica, verificar se a turma possui sala fixa definida ou salas permitidas
        // Para aulas de turma única (caso mais comum):
        const auto* primeira_turma = turmas_aula.front();
        if (primeira_turma->temSalaTeoricaPredefinida()) {
            salas_admissiveis.push_back({primeira_turma->salaTeoricaId()});
        } else {
            // Otimizador selecionará uma entre as permitidas (seção 3.6)
            for (const auto& s_id : primeira_turma->salasTeoricasPermitidas()) {
                salas_admissiveis.push_back({s_id});
            }
        }
    } else {
        // Laboratório ou especial
        if (!aula.salasAlternativas().empty()) {
            salas_admissiveis = aula.salasAlternativas();
        } else {
            // Se não especificou, busca salas compatíveis com o tipo
            for (const auto& sala : data.salas()) {
                if (sala.tipo() == aula.tipo() || !sala.isTeorica()) {
                    salas_admissiveis.push_back({sala.id()});
                }
            }
        }
    }

    if (salas_admissiveis.empty()) {
        diagnostico_aula.push_back("Aula " + aula.id() + ": Nenhuma sala admissível para o tipo '" +
                                   aula.tipo() + "'.");
        return candidatos;
    }

    // Se aula estiver fixada pelo usuário (pinned)
    if (aula.isPinned()) {
        CandidateAllocation c;
        c.aula_id = aula.id();
        c.dia = aula.pinnedDay();
        c.start_slot = aula.pinnedStartSlot();
        c.duration_slots = L_a;
        c.duration_minutes = aula.duracaoMinutos();
        c.salas = aula.pinnedRooms().empty() ? (salas_admissiveis.empty() ? std::vector<std::string>{} : salas_admissiveis.front())
                                            : aula.pinnedRooms();
        c.custo_preferencia = 0;
        candidatos.push_back(std::move(c));
        return candidatos;
    }

    // Iterar sobre todos os dias da semana
    for (auto dia : model::allDays()) {
        // Verificar se todas as turmas funcionam no dia
        bool turmas_ok = true;
        for (const auto* t : turmas_aula) {
            if (!t->funcionaNoDia(dia)) {
                turmas_ok = false;
                break;
            }
        }
        if (!turmas_ok) continue;

        // Iterar sobre os blocos letivos da primeira turma
        auto jornada1 = turmas_aula.front()->jornadaParaDia(dia);
        auto blocos = jornada1.blocosLetivos();

        for (const auto& bloco : blocos) {
            int max_start = bloco.end_slot - L_a;
            for (int t = bloco.start_slot; t <= max_start; ++t) {
                // 1. Verificar se cabe em todas as demais turmas no mesmo dia e slot
                bool todas_turmas_acomodam = true;
                for (size_t i = 1; i < turmas_aula.size(); ++i) {
                    if (!turmaAcomodaAula(*turmas_aula[i], dia, t, L_a)) {
                        todas_turmas_acomodam = false;
                        break;
                    }
                }
                if (!todas_turmas_acomodam) continue;

                // 2. Verificar turno permitido
                bool turno_ok = false;
                for (auto turno_perm : aula.turnosPermitidos()) {
                    if (shiftAcomodaAula(turno_perm, t, L_a)) {
                        turno_ok = true;
                        break;
                    }
                }
                if (!turno_ok) continue;

                // 3. Verificar disponibilidade de todos os professores
                bool todos_profs_disponiveis = true;
                for (const auto* p : profs_aula) {
                    if (!p->isDisponivelNoIntervalo(dia, t, t + L_a)) {
                        todos_profs_disponiveis = false;
                        break;
                    }
                }
                if (!todos_profs_disponiveis) continue;

                // 4. Calcular custo de preferência de horário (seção 4.3)
                int custo_pref = 0;
                // Exemplo: horários extremos da manhã (muito cedo ou perto do almoço)
                // podem ter leve penalidade de preferência horária
                if (t < model::TimeInterval::timeStringToSlot("07:30")) {
                    custo_pref += 1;
                } else if (t > model::TimeInterval::timeStringToSlot("11:00")) {
                    custo_pref += 1;
                }

                // 5. Gerar candidato para cada combinação de salas admissíveis
                for (const auto& combo_salas : salas_admissiveis) {
                    CandidateAllocation cand;
                    cand.aula_id = aula.id();
                    cand.dia = dia;
                    cand.start_slot = t;
                    cand.duration_slots = L_a;
                    cand.duration_minutes = aula.duracaoMinutos();
                    cand.salas = combo_salas;
                    cand.custo_preferencia = custo_pref;
                    candidatos.push_back(std::move(cand));
                }
            }
        }
    }

    if (candidatos.empty()) {
        diagnostico_aula.push_back("Aula " + aula.id() + " (" + aula.disciplinaNome() +
                                   "): Não foi possível gerar nenhum horário admissível dentro dos blocos das turmas e disponibilidade dos professores.");
    }

    return candidatos;
}

std::map<std::string, std::vector<CandidateAllocation>> Preprocessor::gerarTodosCandidatos(
    const model::InstitutionData& data,
    ValidationResult& resultado) {

    std::map<std::string, std::vector<CandidateAllocation>> mapa;

    for (const auto& aula : data.aulas()) {
        std::vector<std::string> diag;
        auto cands = gerarCandidatosParaAula(data, aula, diag);
        if (cands.empty()) {
            for (const auto& err : diag) {
                resultado.addError(err);
            }
        }
        mapa[aula.id()] = std::move(cands);
    }

    return mapa;
}

} // namespace scheduler::solver
