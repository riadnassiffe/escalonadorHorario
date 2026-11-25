/**
 * @file Preprocessor.cpp
 * @brief Implementação dos algoritmos de validação prévia e geração de candidatos viáveis sobre slots configuráveis.
 */

#include "solver/Preprocessor.h"
#include <algorithm>
#include <map>
#include <set>
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

    // Validar turmas e seus slots letivos
    for (const auto& turma : data.turmas()) {
        if (turma.diasLetivos().empty()) {
            res.addError("Turma " + turma.id() + " (" + turma.nome() + ") não possui dias letivos configurados.");
        }

        for (auto dia : turma.diasLetivos()) {
            auto j = turma.jornadaParaDia(dia);
            if (!j.isValid()) {
                res.addError("Turma " + turma.id() + " (" + turma.nome() + ") possui slots inválidos ou sobrepostos em " +
                             model::dayToLongString(dia) + ". Cada slot deve ter início < fim e não sobrepor os demais.");
            }
        }

        for (const auto& [dia, j_exc] : turma.excecoesPorDia()) {
            if (turma.funcionaNoDia(dia) && !j_exc.isValid()) {
                res.addError("Turma " + turma.id() + " possui exceção de slots inválida para " +
                             model::dayToLongString(dia) + ".");
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

        if (disc.duracaoMinutos() <= 0) {
            res.addError("Disciplina " + disc.id() + " possui duração inválida (" +
                         std::to_string(disc.duracaoMinutos()) + " min). Deve ser maior que zero.");
        }
        if (disc.aulasSemanais() <= 0) {
            res.addError("Disciplina " + disc.id() + " possui número de aulas semanais <= 0.");
        }

        for (const auto& combo : disc.salasAlternativas()) {
            for (const auto& s_id : combo) {
                if (!s_id.empty() && !data.findSala(s_id)) {
                    res.addError("Disciplina " + disc.id() + " referencia sala alternativa inexistente: " + s_id);
                }
            }
        }
    }

    // Validar capacidade por quantidade de slots de cada duração e por semestre letivo
    for (const auto& turma : data.turmas()) {
        std::map<int, int> slots_por_dur;
        int total_slots = 0;
        int minutos_disponiveis = 0;

        for (auto dia : turma.diasLetivos()) {
            auto jornada = turma.jornadaParaDia(dia);
            for (const auto& s : jornada.slots_letivos) {
                int dur = s.durationMinutes();
                if (dur > 0) {
                    slots_por_dur[dur]++;
                    total_slots++;
                    minutos_disponiveis += dur;
                }
            }
        }

        std::map<int, int> req_s1;
        std::map<int, int> req_s2;
        int aulas_s1 = 0, aulas_s2 = 0;
        int minutos_s1 = 0, minutos_s2 = 0;
        bool tem_s1 = false, tem_s2 = false;

        for (const auto& disc : data.disciplinas()) {
            if (std::find(disc.turmas().begin(), disc.turmas().end(), turma.id()) != disc.turmas().end()) {
                int dur = disc.duracaoMinutos();
                int cnt = disc.aulasSemanais();
                if (disc.atendeSemestre(model::SemestreLetivo::SEMESTRE_1)) {
                    req_s1[dur] += cnt;
                    aulas_s1 += cnt;
                    minutos_s1 += dur * cnt;
                    tem_s1 = true;
                }
                if (disc.atendeSemestre(model::SemestreLetivo::SEMESTRE_2)) {
                    req_s2[dur] += cnt;
                    aulas_s2 += cnt;
                    minutos_s2 += dur * cnt;
                    tem_s2 = true;
                }
            }
        }

        auto escopo = data.config().escopo_semestre;
        bool validar_s1 = (escopo != model::EscopoSemestreOtimizacao::SEMESTRE_2);
        bool validar_s2 = (escopo != model::EscopoSemestreOtimizacao::SEMESTRE_1);

        auto checar_semestre = [&](const std::map<int, int>& req_map, int total_req_aulas, int total_req_min, const std::string& sem_label) {
            for (const auto& [dur, qtd_req] : req_map) {
                int qtd_disp = slots_por_dur.count(dur) ? slots_por_dur[dur] : 0;
                if (qtd_req > qtd_disp) {
                    res.addError("Turma " + turma.id() + " necessita de " + std::to_string(qtd_req) +
                                 " aula(s) de " + std::to_string(dur) + " min" + sem_label +
                                 ", mas possui apenas " + std::to_string(qtd_disp) +
                                 " slot(s) de " + std::to_string(dur) + " min cadastrados na semana.");
                }
            }
            if (total_req_min > minutos_disponiveis) {
                res.addError("Turma " + turma.id() + " necessita de " + std::to_string(total_req_min) +
                             " min de aula semanais" + sem_label + ", mas só possui " +
                             std::to_string(minutos_disponiveis) + " min nos slots cadastrados.");
            } else if (data.config().preenchimento_integral_jornada && total_req_aulas != total_slots) {
                res.addWarning("Preenchimento integral ativado, mas Turma " + turma.id() + " tem " +
                               std::to_string(total_req_aulas) + " aula(s)" + sem_label + " para " +
                               std::to_string(total_slots) + " slot(s) letivos cadastrados.");
            }

            // Validar compatibilidade com R9 (1º slot do dia precisa ter duração compatível com alguma aula da turma)
            if (total_req_aulas > 0) {
                std::map<int, int> slots_por_dur_dias_validos_r9;
                for (auto dia : turma.diasLetivos()) {
                    auto jornada = turma.jornadaParaDia(dia);
                    if (jornada.slots_letivos.empty()) continue;
                    int first_dur = jornada.slots_letivos.front().durationMinutes();
                    bool first_slot_ok = (req_map.find(first_dur) != req_map.end());
                    if (first_slot_ok) {
                        for (const auto& s : jornada.slots_letivos) {
                            slots_por_dur_dias_validos_r9[s.durationMinutes()]++;
                        }
                    } else {
                        bool has_later_compatible = false;
                        for (size_t k = 1; k < jornada.slots_letivos.size(); ++k) {
                            if (req_map.find(jornada.slots_letivos[k].durationMinutes()) != req_map.end()) {
                                has_later_compatible = true;
                                break;
                            }
                        }
                        if (has_later_compatible) {
                            res.addWarning("Turma " + turma.id() + " em " + model::dayToLongString(dia) +
                                           " possui 1º slot de " + std::to_string(first_dur) +
                                           " min sem nenhuma disciplina dessa duração" + sem_label +
                                           " (a restrição R9 de início no 1º slot bloqueará alocações neste dia).");
                        }
                    }
                }
                for (const auto& [dur, qtd_req] : req_map) {
                    int qtd_disp_r9 = slots_por_dur_dias_validos_r9.count(dur) ? slots_por_dur_dias_validos_r9[dur] : 0;
                    if (qtd_req <= (slots_por_dur.count(dur) ? slots_por_dur[dur] : 0) && qtd_req > qtd_disp_r9) {
                        res.addError("Turma " + turma.id() + " necessita de " + std::to_string(qtd_req) +
                                     " aula(s) de " + std::to_string(dur) + " min" + sem_label +
                                     ", mas apenas " + std::to_string(qtd_disp_r9) +
                                     " slot(s) estão em dias cujo 1º slot da jornada pode ser ocupado (Restrição R9).");
                    }
                }
            }
        };

        if (tem_s1 && tem_s2 && req_s1 == req_s2) {
            checar_semestre(req_s1, aulas_s1, minutos_s1, "");
        } else {
            if (validar_s1 && tem_s1) {
                checar_semestre(req_s1, aulas_s1, minutos_s1, " no 1º Semestre");
            }
            if (validar_s2 && tem_s2) {
                checar_semestre(req_s2, aulas_s2, minutos_s2, " no 2º Semestre");
            }
        }
    }

    return res;
}

bool Preprocessor::turmaAcomodaSlot(const model::Turma& turma, model::DayOfWeek dia,
                                    const model::TimeInterval& slot) {
    if (!turma.funcionaNoDia(dia)) return false;
    return turma.jornadaParaDia(dia).contemSlotExato(slot);
}

std::vector<CandidateAllocation> Preprocessor::gerarCandidatosParaAula(
    const model::InstitutionData& data,
    const model::Lesson& aula,
    std::vector<std::string>& diagnostico_aula) {

    std::vector<CandidateAllocation> candidatos;

    std::string prof_nomes;
    for (size_t i = 0; i < aula.professores().size(); ++i) {
        if (i > 0) prof_nomes += ", ";
        const auto* p = data.findProfessor(aula.professores()[i]);
        prof_nomes += (p && !p->nome().empty()) ? p->nome() : aula.professores()[i];
    }
    if (prof_nomes.empty()) prof_nomes = "Não informado";

    std::string turma_nomes;
    for (size_t i = 0; i < aula.turmas().size(); ++i) {
        if (i > 0) turma_nomes += ", ";
        const auto* t = data.findTurma(aula.turmas()[i]);
        turma_nomes += (t && !t->nome().empty()) ? t->nome() : aula.turmas()[i];
    }
    if (turma_nomes.empty()) turma_nomes = "Não informada";

    // Obter turmas da aula
    std::vector<const model::Turma*> turmas_aula;
    for (const auto& t_id : aula.turmas()) {
        const auto* t = data.findTurma(t_id);
        if (t) turmas_aula.push_back(t);
    }
    if (turmas_aula.empty()) {
        diagnostico_aula.push_back("Não foi possível alocar a aula: " + aula.id() + " (" + aula.disciplinaNome() + ")"
            + " | Professor: " + prof_nomes
            + " | Turma: " + turma_nomes
            + " | Restrições quebradas: Nenhuma turma válida encontrada para a aula.");
        return candidatos;
    }

    // Obter professores da aula
    std::vector<const model::Professor*> profs_aula;
    for (const auto& p_id : aula.professores()) {
        const auto* p = data.findProfessor(p_id);
        if (p) profs_aula.push_back(p);
    }
    if (profs_aula.empty()) {
        diagnostico_aula.push_back("Não foi possível alocar a aula: " + aula.id() + " (" + aula.disciplinaNome() + ")"
            + " | Professor: " + prof_nomes
            + " | Turma: " + turma_nomes
            + " | Restrições quebradas: Nenhum professor válido encontrado para a aula.");
        return candidatos;
    }

    int dur_min = aula.duracaoMinutos();

    // Determinar combinações de salas admissíveis
    std::vector<std::vector<std::string>> salas_admissiveis;
    for (const auto& combo : aula.salasAlternativas()) {
        if (!combo.empty()) {
            salas_admissiveis.push_back(combo);
        }
    }

    if (salas_admissiveis.empty()) {
        if (aula.isTeorica()) {
            const auto* primeira_turma = turmas_aula.front();
            if (primeira_turma->temSalaTeoricaPredefinida()) {
                salas_admissiveis.push_back({primeira_turma->salaTeoricaId()});
            } else {
                for (const auto& s_id : primeira_turma->salasTeoricasPermitidas()) {
                    salas_admissiveis.push_back({s_id});
                }
            }
        } else {
            for (const auto& sala : data.salas()) {
                if (sala.tipo() == aula.tipo() || !sala.isTeorica()) {
                    salas_admissiveis.push_back({sala.id()});
                }
            }
        }
    }

    if (salas_admissiveis.empty()) {
        diagnostico_aula.push_back("Não foi possível alocar a aula: " + aula.id() + " (" + aula.disciplinaNome() + ")"
            + " | Professor: " + prof_nomes
            + " | Turma: " + turma_nomes
            + " | Restrições quebradas: Nenhuma sala física admissível cadastrada para o tipo '" + aula.tipo() + "'.");
        return candidatos;
    }

    // Se aula estiver fixada pelo usuário (pinned)
    if (aula.isPinned()) {
        CandidateAllocation c;
        c.aula_id = aula.id();
        c.dia = aula.pinnedDay();
        c.start_min = aula.pinnedInterval().start_min;
        c.duration_minutes = aula.pinnedInterval().durationMinutes() > 0
                                 ? aula.pinnedInterval().durationMinutes()
                                 : dur_min;
        c.salas = aula.pinnedRooms().empty() ? salas_admissiveis.front() : aula.pinnedRooms();
        c.custo_preferencia = 0;
        candidatos.push_back(std::move(c));
        return candidatos;
    }

    bool turmas_sem_dia = false;
    bool nenhum_slot_mesma_duracao = true;
    bool prof_sem_disponibilidade = false;
    bool conflito_outra_turma = false;
    int slots_avaliados = 0;

    const int min_0730 = model::TimeInterval::timeStringToMinutes("07:30");
    const int min_1100 = model::TimeInterval::timeStringToMinutes("11:00");

    // Iterar sobre todos os dias da semana
    for (auto dia : model::allDays()) {
        bool turmas_ok = true;
        for (const auto* t : turmas_aula) {
            if (!t->funcionaNoDia(dia)) {
                turmas_ok = false;
                turmas_sem_dia = true;
                break;
            }
        }
        if (!turmas_ok) continue;

        auto jornada1 = turmas_aula.front()->jornadaParaDia(dia);

        for (const auto& slot : jornada1.slots_letivos) {
            slots_avaliados++;
            if (slot.durationMinutes() != dur_min) {
                continue;
            }
            nenhum_slot_mesma_duracao = false;

            // 1. Verificar se todas as demais turmas possuem exatamente o mesmo slot no dia
            bool todas_turmas_acomodam = true;
            for (size_t i = 1; i < turmas_aula.size(); ++i) {
                if (!turmaAcomodaSlot(*turmas_aula[i], dia, slot)) {
                    todas_turmas_acomodam = false;
                    conflito_outra_turma = true;
                    break;
                }
            }
            if (!todas_turmas_acomodam) continue;

            // 2. Verificar disponibilidade de todos os professores
            bool todos_profs_disponiveis = true;
            for (const auto* p : profs_aula) {
                if (!p->isDisponivelNoIntervalo(dia, slot.start_min, slot.end_min)) {
                    todos_profs_disponiveis = false;
                    prof_sem_disponibilidade = true;
                    break;
                }
            }
            if (!todos_profs_disponiveis) continue;

            // 3. Calcular custo de preferência de horário
            int custo_pref = 0;
            if (slot.start_min < min_0730) {
                custo_pref += 1;
            } else if (slot.start_min > min_1100) {
                custo_pref += 1;
            }

            // 4. Gerar candidato para cada combinação de salas admissíveis
            for (const auto& combo_salas : salas_admissiveis) {
                CandidateAllocation cand;
                cand.aula_id = aula.id();
                cand.dia = dia;
                cand.start_min = slot.start_min;
                cand.duration_minutes = dur_min;
                cand.salas = combo_salas;
                cand.custo_preferencia = custo_pref;
                candidatos.push_back(std::move(cand));
            }
        }
    }

    if (candidatos.empty()) {
        std::vector<std::string> restricoes;
        if (nenhum_slot_mesma_duracao && slots_avaliados > 0) {
            restricoes.push_back("Nenhum slot cadastrado na turma possui duração exata de " +
                                 std::to_string(dur_min) + " min");
        }
        if (prof_sem_disponibilidade) {
            restricoes.push_back("Professor sem disponibilidade nos slots compatíveis da turma");
        }
        if (conflito_outra_turma) {
            restricoes.push_back("Incompatibilidade de slots entre turmas agrupadas");
        }
        if (turmas_sem_dia && slots_avaliados == 0) {
            restricoes.push_back("Turma não possui dias letivos ou slots configurados");
        }
        if (restricoes.empty()) {
            restricoes.push_back("Nenhum slot admissível encontrado para a aula");
        }

        std::string restricoes_str;
        for (size_t r = 0; r < restricoes.size(); ++r) {
            if (r > 0) restricoes_str += "; ";
            restricoes_str += restricoes[r];
        }

        diagnostico_aula.push_back("Não foi possível alocar a aula: " + aula.id() + " (" + aula.disciplinaNome() + ")"
            + " | Professor: " + prof_nomes
            + " | Turma: " + turma_nomes
            + " | Restrições quebradas: " + restricoes_str);
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
