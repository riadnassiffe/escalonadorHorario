#include "model/InstitutionData.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cstdio>
#include <set>

/**
 * @file InstitutionData.cpp
 * @brief Implementação dos métodos da classe InstitutionData e funções auxiliares.
 */

namespace {

/**
 * @brief Extrai o maior sufixo numérico de uma lista de identificadores com determinado prefixo.
 *
 * Percorre a lista de IDs, converte prefixos e IDs para minúsculas para comparação insensível
 * a maiúsculas/minúsculas, localiza o prefixo e analisa os dígitos numéricos imediatamente
 * seguintes, retornando o maior valor numérico encontrado.
 *
 * @param ids Lista de identificadores existentes (ex: {"P01", "P02", "P10"}).
 * @param prefix Prefixo textual associado à entidade (ex: "p", "t", "s", "d").
 * @return Maior valor numérico inteiro identificado nos sufixos, ou 0 se nenhum ID corresponder.
 */
int extractMaxNumericSuffix(const std::vector<std::string>& ids, const std::string& prefix) {
    int max_val = 0;
    std::string lower_prefix = prefix;
    std::transform(lower_prefix.begin(), lower_prefix.end(), lower_prefix.begin(), ::tolower);

    for (const auto& id : ids) {
        std::string lower_id = id;
        std::transform(lower_id.begin(), lower_id.end(), lower_id.begin(), ::tolower);

        auto pos = lower_id.find(lower_prefix);
        if (pos != std::string::npos) {
            size_t num_start = pos + lower_prefix.size();
            size_t num_end = num_start;
            while (num_end < lower_id.size() && std::isdigit(static_cast<unsigned char>(lower_id[num_end]))) {
                num_end++;
            }
            if (num_end > num_start) {
                try {
                    int val = std::stoi(lower_id.substr(num_start, num_end - num_start));
                    if (val > max_val) max_val = val;
                } catch (...) {}
            }
        }
    }
    return max_val;
}

} // anonymous namespace

namespace scheduler::model {

/**
 * @brief Construtor padrão da classe InstitutionData.
 *
 * Inicializa uma instância vazia do repositório de dados institucionais com
 * as coleções de professores, salas, turmas, disciplinas e grade horária vazias.
 */
InstitutionData::InstitutionData() {
    slot_templates_[ShiftType::MANHA] = JornadaTurma::defaultSlotsForShift(ShiftType::MANHA);
    slot_templates_[ShiftType::TARDE] = JornadaTurma::defaultSlotsForShift(ShiftType::TARDE);
    slot_templates_[ShiftType::NOITE] = JornadaTurma::defaultSlotsForShift(ShiftType::NOITE);
    slot_templates_[ShiftType::INTEGRAL] = JornadaTurma::defaultSlotsForShift(ShiftType::INTEGRAL);
}

std::vector<TimeInterval> InstitutionData::slotTemplateForShift(ShiftType shift) const {
    auto it = slot_templates_.find(shift);
    if (it != slot_templates_.end() && !it->second.empty()) {
        return it->second;
    }
    return JornadaTurma::defaultSlotsForShift(shift);
}

void InstitutionData::setSlotTemplateForShift(ShiftType shift, std::vector<TimeInterval> slots) {
    std::sort(slots.begin(), slots.end());
    slot_templates_[shift] = std::move(slots);
}

/**
 * @brief Adiciona um novo professor ou substitui o registro existente de mesmo identificador.
 *
 * @param prof Objeto do tipo Professor a ser adicionado ou atualizado no cadastro.
 */
void InstitutionData::addProfessor(Professor prof) {
    auto it = std::find_if(professores_.begin(), professores_.end(),
                           [&](const Professor& p) { return p.id() == prof.id(); });
    if (it != professores_.end()) {
        *it = std::move(prof);
    } else {
        professores_.push_back(std::move(prof));
    }
}

/**
 * @brief Remove um professor do cadastro institucional a partir do seu identificador.
 *
 * @param id Identificador único (ID) do professor a ser removido.
 * @return true se o professor foi localizado e removido com sucesso, ou false caso contrário.
 */
bool InstitutionData::removeProfessor(const std::string& id) {
    auto it = std::remove_if(professores_.begin(), professores_.end(),
                             [&](const Professor& p) { return p.id() == id; });
    if (it != professores_.end()) {
        professores_.erase(it, professores_.end());
        return true;
    }
    return false;
}

/**
 * @brief Localiza um professor pelo seu identificador (somente leitura).
 *
 * @param id Identificador único (ID) do professor desejado.
 * @return Ponteiro constante para o Professor correspondente, ou nullptr se não encontrado.
 */
const Professor* InstitutionData::findProfessor(const std::string& id) const {
    for (const auto& p : professores_) {
        if (p.id() == id) return &p;
    }
    return nullptr;
}

/**
 * @brief Localiza um professor pelo seu identificador para leitura e modificação.
 *
 * @param id Identificador único (ID) do professor desejado.
 * @return Ponteiro para o Professor correspondente, ou nullptr se não encontrado.
 */
Professor* InstitutionData::findProfessor(const std::string& id) {
    for (auto& p : professores_) {
        if (p.id() == id) return &p;
    }
    return nullptr;
}

/**
 * @brief Adiciona uma nova sala ou substitui o registro existente de mesmo identificador.
 *
 * @param sala Objeto do tipo Room a ser adicionado ou atualizado no cadastro.
 */
void InstitutionData::addSala(Room sala) {
    auto it = std::find_if(salas_.begin(), salas_.end(),
                           [&](const Room& r) { return r.id() == sala.id(); });
    if (it != salas_.end()) {
        *it = std::move(sala);
    } else {
        salas_.push_back(std::move(sala));
    }
}

/**
 * @brief Remove uma sala do cadastro institucional a partir do seu identificador.
 *
 * @param id Identificador único (ID) da sala a ser removida.
 * @return true se a sala foi localizada e removida com sucesso, ou false caso contrário.
 */
bool InstitutionData::removeSala(const std::string& id) {
    auto it = std::remove_if(salas_.begin(), salas_.end(),
                             [&](const Room& r) { return r.id() == id; });
    if (it != salas_.end()) {
        salas_.erase(it, salas_.end());
        return true;
    }
    return false;
}

/**
 * @brief Localiza uma sala pelo seu identificador (somente leitura).
 *
 * @param id Identificador único (ID) da sala desejada.
 * @return Ponteiro constante para o objeto Room correspondente, ou nullptr se não encontrado.
 */
const Room* InstitutionData::findSala(const std::string& id) const {
    for (const auto& r : salas_) {
        if (r.id() == id) return &r;
    }
    return nullptr;
}

/**
 * @brief Localiza uma sala pelo seu identificador para leitura e modificação.
 *
 * @param id Identificador único (ID) da sala desejada.
 * @return Ponteiro para o objeto Room correspondente, ou nullptr se não encontrado.
 */
Room* InstitutionData::findSala(const std::string& id) {
    for (auto& r : salas_) {
        if (r.id() == id) return &r;
    }
    return nullptr;
}

/**
 * @brief Adiciona uma nova turma ou substitui o registro existente de mesmo identificador.
 *
 * @param turma Objeto do tipo Turma a ser adicionado ou atualizado no cadastro.
 */
void InstitutionData::addTurma(Turma turma) {
    auto it = std::find_if(turmas_.begin(), turmas_.end(),
                           [&](const Turma& t) { return t.id() == turma.id(); });
    if (it != turmas_.end()) {
        *it = std::move(turma);
    } else {
        turmas_.push_back(std::move(turma));
    }
}

/**
 * @brief Remove uma turma do cadastro institucional a partir do seu identificador.
 *
 * @param id Identificador único (ID) da turma a ser removida.
 * @return true se a turma foi localizada e removida com sucesso, ou false caso contrário.
 */
bool InstitutionData::removeTurma(const std::string& id) {
    auto it = std::remove_if(turmas_.begin(), turmas_.end(),
                             [&](const Turma& t) { return t.id() == id; });
    if (it != turmas_.end()) {
        turmas_.erase(it, turmas_.end());
        return true;
    }
    return false;
}

/**
 * @brief Localiza uma turma pelo seu identificador (somente leitura).
 *
 * @param id Identificador único (ID) da turma desejada.
 * @return Ponteiro constante para a Turma correspondente, ou nullptr se não encontrada.
 */
const Turma* InstitutionData::findTurma(const std::string& id) const {
    for (const auto& t : turmas_) {
        if (t.id() == id) return &t;
    }
    return nullptr;
}

/**
 * @brief Localiza uma turma pelo seu identificador para leitura e modificação.
 *
 * @param id Identificador único (ID) da turma desejada.
 * @return Ponteiro para a Turma correspondente, ou nullptr se não encontrada.
 */
Turma* InstitutionData::findTurma(const std::string& id) {
    for (auto& t : turmas_) {
        if (t.id() == id) return &t;
    }
    return nullptr;
}

/**
 * @brief Adiciona uma nova disciplina ou substitui o registro existente de mesmo identificador.
 *
 * @param disc Objeto do tipo Disciplina a ser adicionado ou atualizado no cadastro.
 */
void InstitutionData::addDisciplina(Disciplina disc) {
    auto it = std::find_if(disciplinas_.begin(), disciplinas_.end(),
                           [&](const Disciplina& d) { return d.id() == disc.id(); });
    if (it != disciplinas_.end()) {
        *it = std::move(disc);
    } else {
        disciplinas_.push_back(std::move(disc));
    }
}

/**
 * @brief Remove uma disciplina do cadastro institucional a partir do seu identificador.
 *
 * @param id Identificador único (ID) da disciplina a ser removida.
 * @return true se a disciplina foi localizada e removida com sucesso, ou false caso contrário.
 */
bool InstitutionData::removeDisciplina(const std::string& id) {
    auto it = std::remove_if(disciplinas_.begin(), disciplinas_.end(),
                             [&](const Disciplina& d) { return d.id() == id; });
    if (it != disciplinas_.end()) {
        disciplinas_.erase(it, disciplinas_.end());
        return true;
    }
    return false;
}

/**
 * @brief Localiza uma disciplina pelo seu identificador (somente leitura).
 *
 * @param id Identificador único (ID) da disciplina desejada.
 * @return Ponteiro constante para a Disciplina correspondente, ou nullptr se não encontrada.
 */
const Disciplina* InstitutionData::findDisciplina(const std::string& id) const {
    for (const auto& d : disciplinas_) {
        if (d.id() == id) return &d;
    }
    return nullptr;
}

/**
 * @brief Localiza uma disciplina pelo seu identificador para leitura e modificação.
 *
 * @param id Identificador único (ID) da disciplina desejada.
 * @return Ponteiro para a Disciplina correspondente, ou nullptr se não encontrada.
 */
Disciplina* InstitutionData::findDisciplina(const std::string& id) {
    for (auto& d : disciplinas_) {
        if (d.id() == id) return &d;
    }
    return nullptr;
}

/**
 * @brief Localiza uma ocorrência de aula pelo seu identificador (somente leitura).
 *
 * @param id Identificador único (ID) da ocorrência de aula (ex: "T01_MAT_1").
 * @return Ponteiro constante para o objeto Lesson correspondente, ou nullptr se não encontrado.
 */
const Lesson* InstitutionData::findAula(const std::string& id) const {
    for (const auto& a : aulas_) {
        if (a.id() == id) return &a;
    }
    return nullptr;
}

/**
 * @brief Localiza uma ocorrência de aula pelo seu identificador para leitura e modificação.
 *
 * @param id Identificador único (ID) da ocorrência de aula (ex: "T01_MAT_1").
 * @return Ponteiro para o objeto Lesson correspondente, ou nullptr se não encontrado.
 */
Lesson* InstitutionData::findAula(const std::string& id) {
    for (auto& a : aulas_) {
        if (a.id() == id) return &a;
    }
    return nullptr;
}

/**
 * @brief Gera a lista de ocorrências individuais de aulas (Lesson) a partir das disciplinas cadastradas.
 *
 * Percorre todas as disciplinas cadastradas, filtra conforme o escopo de semestre informado
 * (SEMESTRE_1, SEMESTRE_2 ou TODAS_SEM_FILTRO), cria instâncias de Lesson para cada aula semanal
 * necessária e opcionalmente preserva fixações manuais prévias (horário, dia e salas fixadas).
 *
 * @param preservar_fixadas Se true, preserva o estado de fixação (pinned) de ocorrências pré-existentes.
 * @param escopo Escopo de semestre letivo a ser considerado na geração.
 */
void InstitutionData::gerarOcorrenciasAulas(bool preservar_fixadas, EscopoSemestreOtimizacao escopo) {
    std::vector<Lesson> novas_aulas;

    // Mapa de aulas fixadas previamente (aulas_ + fonte canônica grade_.alocacoes())
    struct PinnedState {
        DayOfWeek dia{DayOfWeek::SEG};
        TimeInterval intervalo;
        std::vector<std::string> salas;
    };
    std::map<std::string, PinnedState> fixadas;
    if (preservar_fixadas) {
        for (const auto& a : aulas_) {
            if (a.isPinned()) {
                fixadas[a.id()] = {a.pinnedDay(), a.pinnedInterval(), a.pinnedRooms()};
            }
        }
        for (const auto& aloc : grade_.alocacoes()) {
            if (aloc.is_pinned) {
                fixadas[aloc.aula_id] = {aloc.dia, aloc.intervalo, aloc.salas};
            } else {
                fixadas.erase(aloc.aula_id);
            }
        }
    }

    for (const auto& disc : disciplinas_) {
        // Filtragem por escopo de semestre
        if (escopo == EscopoSemestreOtimizacao::SEMESTRE_1 && !disc.atendeSemestre(SemestreLetivo::SEMESTRE_1)) {
            continue;
        }
        if (escopo == EscopoSemestreOtimizacao::SEMESTRE_2 && !disc.atendeSemestre(SemestreLetivo::SEMESTRE_2)) {
            continue;
        }

        for (int i = 1; i <= disc.aulasSemanais(); ++i) {
            std::string aula_id = disc.id() + "_" + std::to_string(i);

            Lesson aula(aula_id, disc.id(), disc.nome(), disc.tipo(),
                        disc.duracaoMinutos(), disc.professores(), disc.turmas(),
                        disc.regime(), disc.semestre());
            aula.setSalasAlternativas(disc.salasAlternativas());
            int sem_ativo = 1;
            if (disc.isSemestral()) {
                sem_ativo = (disc.semestre() == SemestreLetivo::SEMESTRE_2) ? 2 : 1;
            } else {
                sem_ativo = (escopo == EscopoSemestreOtimizacao::SEMESTRE_2) ? 2 : 1;
            }
            aula.setSemestreAtivo(sem_ativo);

            auto it = fixadas.find(aula_id);
            if (it != fixadas.end()) {
                aula.setPinned(true);
                aula.setPinnedDay(it->second.dia);
                aula.setPinnedInterval(it->second.intervalo);
                aula.setPinnedRooms(it->second.salas);
            }

            novas_aulas.push_back(std::move(aula));
        }
    }

    aulas_ = std::move(novas_aulas);
}

bool InstitutionData::aplicarResultadoOtimizacao(Timetable novo_resultado,
                                                 EscopoSemestreOtimizacao escopo) {
    const std::string& st = novo_resultado.estatisticas().status;
    const bool has_valid_solution =
        (st == "OPTIMAL" || st == "FEASIBLE") && !novo_resultado.alocacoes().empty();

    if (!has_valid_solution) {
        if (grade_.alocacoes().empty()) {
            grade_ = std::move(novo_resultado);
        }
        return false;
    }

    std::set<std::string> novas_ids;
    for (const auto& aloc : novo_resultado.alocacoes()) {
        novas_ids.insert(aloc.aula_id);
    }

    bool mesclou_outro_semestre = false;
    for (const auto& prev_aloc : grade_.alocacoes()) {
        if (novas_ids.count(prev_aloc.aula_id) > 0) {
            continue;
        }
        const auto* disc = findDisciplina(prev_aloc.disciplina_id);
        if (!disc) {
            continue;
        }

        bool fora_do_escopo = false;
        if (escopo == EscopoSemestreOtimizacao::SEMESTRE_1) {
            fora_do_escopo =
                (disc->isSemestral() && disc->semestre() == SemestreLetivo::SEMESTRE_2) ||
                (prev_aloc.regime == RegimeDisciplina::SEMESTRAL &&
                 (prev_aloc.semestre == SemestreLetivo::SEMESTRE_2 || prev_aloc.semestre_alocado == 2));
        } else if (escopo == EscopoSemestreOtimizacao::SEMESTRE_2) {
            fora_do_escopo =
                (disc->isSemestral() && disc->semestre() == SemestreLetivo::SEMESTRE_1) ||
                (prev_aloc.regime == RegimeDisciplina::SEMESTRAL &&
                 (prev_aloc.semestre == SemestreLetivo::SEMESTRE_1 || prev_aloc.semestre_alocado == 1));
        }

        if (fora_do_escopo) {
            novo_resultado.addAllocatedLesson(prev_aloc);
            novas_ids.insert(prev_aloc.aula_id);
            mesclou_outro_semestre = true;
        } else if (prev_aloc.is_pinned) {
            novo_resultado.addAllocatedLesson(prev_aloc);
            novas_ids.insert(prev_aloc.aula_id);
            novo_resultado.addMensagemDiagnostico(
                "[PRESERVADA] Aula fixada '" + prev_aloc.disciplina_nome + "' (" + prev_aloc.aula_id +
                ") foi mantida na grade (não incluída na solução parcial do solver).");
        }
    }

    for (const auto& [turma_id, sala_id] : grade_.salasTeoricasDesignadas()) {
        if (novo_resultado.salaTeoricaTurma(turma_id).empty()) {
            novo_resultado.setSalaTeoricaDesignada(turma_id, sala_id);
        }
    }

    TimetableStatistics stats = novo_resultado.estatisticas();
    stats.total_aulas_alocadas = static_cast<int>(novo_resultado.alocacoes().size());
    if (mesclou_outro_semestre) {
        int total_esp = 0;
        for (const auto& d : disciplinas_) {
            total_esp += d.aulasSemanais();
        }
        stats.total_aulas_esperadas = total_esp;
    }
    novo_resultado.setEstatisticas(stats);

    grade_ = std::move(novo_resultado);
    gerarOcorrenciasAulas(true, EscopoSemestreOtimizacao::TODAS_SEM_FILTRO);
    return true;
}

/**
 * @brief Limpa todos os dados armazenados na instituição.
 *
 * Remove todos os registros de professores, salas, turmas, disciplinas, ocorrências de aulas e a grade horária.
 */
void InstitutionData::clear() {
    professores_.clear();
    salas_.clear();
    turmas_.clear();
    disciplinas_.clear();
    aulas_.clear();
    grade_.clear();
    slot_templates_[ShiftType::MANHA] = JornadaTurma::defaultSlotsForShift(ShiftType::MANHA);
    slot_templates_[ShiftType::TARDE] = JornadaTurma::defaultSlotsForShift(ShiftType::TARDE);
    slot_templates_[ShiftType::NOITE] = JornadaTurma::defaultSlotsForShift(ShiftType::NOITE);
    slot_templates_[ShiftType::INTEGRAL] = JornadaTurma::defaultSlotsForShift(ShiftType::INTEGRAL);
}

/**
 * @brief Gera o próximo identificador sequencial livre para novos professores no formato "P01", "P02", etc.
 *
 * @return String com o novo identificador único de professor disponível.
 */
std::string InstitutionData::nextProfessorId() const {
    std::vector<std::string> ids;
    ids.reserve(professores_.size());
    for (const auto& p : professores_) ids.push_back(p.id());

    int n = extractMaxNumericSuffix(ids, "p") + 1;
    while (true) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "P%02d", n);
        std::string cand(buf);
        if (!findProfessor(cand)) return cand;
        n++;
    }
}

/**
 * @brief Gera o próximo identificador sequencial livre para novas turmas no formato "T01", "T02", etc.
 *
 * @return String com o novo identificador único de turma disponível.
 */
std::string InstitutionData::nextTurmaId() const {
    std::vector<std::string> ids;
    ids.reserve(turmas_.size());
    for (const auto& t : turmas_) ids.push_back(t.id());

    int n = extractMaxNumericSuffix(ids, "t") + 1;
    while (true) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "T%02d", n);
        std::string cand(buf);
        if (!findTurma(cand)) return cand;
        n++;
    }
}

/**
 * @brief Gera o próximo identificador sequencial livre para novas salas no formato "S01", "S02", etc.
 *
 * @return String com o novo identificador único de sala disponível.
 */
std::string InstitutionData::nextSalaId() const {
    std::vector<std::string> ids;
    ids.reserve(salas_.size());
    for (const auto& s : salas_) ids.push_back(s.id());

    int n = extractMaxNumericSuffix(ids, "s") + 1;
    while (true) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "S%02d", n);
        std::string cand(buf);
        if (!findSala(cand)) return cand;
        n++;
    }
}

/**
 * @brief Gera o próximo identificador sequencial livre para novas disciplinas no formato "D01", "D02", etc.
 *
 * @return String com o novo identificador único de disciplina disponível.
 */
std::string InstitutionData::nextDisciplinaId() const {
    std::vector<std::string> ids;
    ids.reserve(disciplinas_.size());
    for (const auto& d : disciplinas_) ids.push_back(d.id());

    int n = extractMaxNumericSuffix(ids, "d") + 1;
    while (true) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "D%02d", n);
        std::string cand(buf);
        if (!findDisciplina(cand)) return cand;
        n++;
    }
}

/**
 * @brief Cria e inicializa uma instância de InstitutionData com o cenário acadêmico padrão.
 *
 * Carrega o conjunto completo de dados acadêmicos com 6 professores (com disponibilidades horárias
 * e pesos diários de preferência), 6 salas de aula (teóricas e laboratórios), 3 turmas do ensino
 * médio (com salas atribuídas e restrições) e diversas disciplinas anuais e semestrais.
 * Ao final, executa a geração automática de ocorrências de aulas.
 *
 * @return Instância completa e configurada de InstitutionData pronta para uso ou simulação.
 */
InstitutionData InstitutionData::criarExemploPadrao() {
    InstitutionData data;

    // 1. Professores
    // P01: Prof. Carlos Silva (Matemática) - Exemplo da seção 2.3
    Professor p1("P01", "Prof. Carlos Silva (Matemática)");
    p1.setPesoDia(DayOfWeek::SEG, 10); // Dia preferido
    p1.setPesoDia(DayOfWeek::TER, 7);
    p1.setPesoDia(DayOfWeek::QUA, 2);
    p1.setPesoDia(DayOfWeek::QUI, 8);
    p1.setPesoDia(DayOfWeek::SEX, 0);  // Menos desejado (não indisponível)
    for (auto d : allDays()) {
        p1.addIntervaloDisponivel(d, TimeInterval("08:00", "12:30"));
    }
    data.addProfessor(p1);

    // P02: Profa. Ana Souza (Química)
    Professor p2("P02", "Profa. Ana Souza (Química)");
    p2.setPesoDia(DayOfWeek::SEG, 5);
    p2.setPesoDia(DayOfWeek::TER, 10);
    p2.setPesoDia(DayOfWeek::QUA, 8);
    p2.setPesoDia(DayOfWeek::QUI, 6);
    p2.setPesoDia(DayOfWeek::SEX, 2);
    for (auto d : allDays()) {
        p2.addIntervaloDisponivel(d, TimeInterval("08:00", "12:30"));
    }
    data.addProfessor(p2);

    // P03: Prof. Marcos Lima (Física)
    Professor p3("P03", "Prof. Marcos Lima (Física)");
    p3.setPesoDia(DayOfWeek::SEG, 4);
    p3.setPesoDia(DayOfWeek::TER, 5);
    p3.setPesoDia(DayOfWeek::QUA, 10);
    p3.setPesoDia(DayOfWeek::QUI, 9);
    p3.setPesoDia(DayOfWeek::SEX, 6);
    for (auto d : allDays()) {
        p3.addIntervaloDisponivel(d, TimeInterval("08:00", "12:30"));
    }
    data.addProfessor(p3);

    // P04: Profa. Juliana Mendes (História & Geografia)
    Professor p4("P04", "Profa. Juliana Mendes (Humanas)");
    p4.setPesoDia(DayOfWeek::SEG, 8);
    p4.setPesoDia(DayOfWeek::TER, 6);
    p4.setPesoDia(DayOfWeek::QUA, 5);
    p4.setPesoDia(DayOfWeek::QUI, 10);
    p4.setPesoDia(DayOfWeek::SEX, 7);
    for (auto d : allDays()) {
        p4.addIntervaloDisponivel(d, TimeInterval("08:00", "12:30"));
    }
    data.addProfessor(p4);

    // P05: Prof. Roberto Castro (Português & Literatura)
    Professor p5("P05", "Prof. Roberto Castro (Linguagens)");
    p5.setPesoDia(DayOfWeek::SEG, 6);
    p5.setPesoDia(DayOfWeek::TER, 9);
    p5.setPesoDia(DayOfWeek::QUA, 7);
    p5.setPesoDia(DayOfWeek::QUI, 5);
    p5.setPesoDia(DayOfWeek::SEX, 8);
    for (auto d : allDays()) {
        p5.addIntervaloDisponivel(d, TimeInterval("08:00", "12:30"));
    }
    data.addProfessor(p5);

    // P06: Profa. Beatriz Costa (Biologia & Filosofia)
    Professor p6("P06", "Profa. Beatriz Costa (Biológicas)");
    p6.setPesoDia(DayOfWeek::SEG, 7);
    p6.setPesoDia(DayOfWeek::TER, 5);
    p6.setPesoDia(DayOfWeek::QUA, 8);
    p6.setPesoDia(DayOfWeek::QUI, 7);
    p6.setPesoDia(DayOfWeek::SEX, 9);
    for (auto d : allDays()) {
        p6.addIntervaloDisponivel(d, TimeInterval("08:00", "12:30"));
    }
    data.addProfessor(p6);

    // 2. Salas
    data.addSala(Room("S101", "Sala Teórica 101", "TEORICA", 40));
    data.addSala(Room("S102", "Sala Teórica 102", "TEORICA", 40));
    data.addSala(Room("S201", "Sala Teórica 201", "TEORICA", 45));
    data.addSala(Room("LAB01", "Laboratório de Química 1", "LABORATORIO", 35));
    data.addSala(Room("LAB02", "Laboratório Multidisciplinar", "LABORATORIO", 30));
    data.addSala(Room("LAB_INF", "Laboratório de Informática", "LABORATORIO", 35));

    // 3. Turmas com grade explícita de slots letivos (3x50m + 2x45m = 15x50m e 10x45m na semana)
    JornadaTurma jornada_exemplo({
        TimeInterval("08:00", "08:50"), // 50 min
        TimeInterval("08:50", "09:40"), // 50 min
        // Intervalo 1: 09:40 - 09:55
        TimeInterval("09:55", "10:40"), // 45 min
        TimeInterval("10:40", "11:25"), // 45 min
        // Intervalo 2: 11:25 - 11:40
        TimeInterval("11:40", "12:30")  // 50 min
    });

    Turma t1("T01", "1º Ano A - Ensino Médio", ShiftType::MANHA);
    t1.setJornadaPadrao(jornada_exemplo);
    t1.setSalaTeoricaId("S101");
    t1.setSalasTeoricasPermitidas({"S101"});
    data.addTurma(t1);

    Turma t2("T02", "2º Ano A - Ensino Médio", ShiftType::MANHA);
    t2.setJornadaPadrao(jornada_exemplo);
    t2.setSalaTeoricaId("S102");
    t2.setSalasTeoricasPermitidas({"S102"});
    data.addTurma(t2);

    Turma t3("T03", "3º Ano A - Ensino Médio", ShiftType::MANHA);
    t3.setJornadaPadrao(jornada_exemplo);
    t3.setSalaTeoricaId(""); // Otimizador escolherá a sala fixa da turma! (seção 3.6)
    t3.setSalasTeoricasPermitidas({"S201"});
    data.addTurma(t3);

    // 4. Disciplinas - com durações de 45 e 50 min e classificação Anual/Semestral
    // Turma T01
    {
        Disciplina d("T01_MAT", "Matemática", "TEORICA", 45, 5, RegimeDisciplina::ANUAL, SemestreLetivo::AMBOS);
        d.setProfessores({"P01"});
        d.setTurmas({"T01"});
        data.addDisciplina(d);
    }
    {
        Disciplina d("T01_PORT", "Português", "TEORICA", 50, 4, RegimeDisciplina::ANUAL, SemestreLetivo::AMBOS);
        d.setProfessores({"P05"});
        d.setTurmas({"T01"});
        data.addDisciplina(d);
    }
    {
        Disciplina d("T01_FIS", "Física", "TEORICA", 50, 3, RegimeDisciplina::ANUAL, SemestreLetivo::AMBOS);
        d.setProfessores({"P03"});
        d.setTurmas({"T01"});
        data.addDisciplina(d);
    }
    {
        Disciplina d("T01_QUI_EXP", "Química Experimental", "LABORATORIO", 50, 2, RegimeDisciplina::SEMESTRAL, SemestreLetivo::SEMESTRE_1);
        d.setProfessores({"P02"});
        d.setTurmas({"T01"});
        d.setSalasAlternativas({{"LAB01"}, {"LAB02"}});
        data.addDisciplina(d);
    }
    {
        Disciplina d("T01_HIST", "História", "TEORICA", 45, 3, RegimeDisciplina::SEMESTRAL, SemestreLetivo::SEMESTRE_1);
        d.setProfessores({"P04"});
        d.setTurmas({"T01"});
        data.addDisciplina(d);
    }
    {
        Disciplina d("T01_GEO", "Geografia", "TEORICA", 45, 3, RegimeDisciplina::SEMESTRAL, SemestreLetivo::SEMESTRE_2);
        d.setProfessores({"P04"});
        d.setTurmas({"T01"});
        data.addDisciplina(d);
    }
    {
        Disciplina d("T01_BIO", "Biologia", "TEORICA", 45, 2, RegimeDisciplina::ANUAL, SemestreLetivo::AMBOS);
        d.setProfessores({"P06"});
        d.setTurmas({"T01"});
        data.addDisciplina(d);
    }

    // Turma T02
    {
        Disciplina d("T02_MAT", "Matemática", "TEORICA", 50, 4, RegimeDisciplina::ANUAL, SemestreLetivo::AMBOS);
        d.setProfessores({"P01"});
        d.setTurmas({"T02"});
        data.addDisciplina(d);
    }
    {
        Disciplina d("T02_PORT", "Português", "TEORICA", 50, 4, RegimeDisciplina::ANUAL, SemestreLetivo::AMBOS);
        d.setProfessores({"P05"});
        d.setTurmas({"T02"});
        data.addDisciplina(d);
    }
    {
        Disciplina d("T02_FIS", "Física", "TEORICA", 50, 3, RegimeDisciplina::ANUAL, SemestreLetivo::AMBOS);
        d.setProfessores({"P03"});
        d.setTurmas({"T02"});
        data.addDisciplina(d);
    }
    {
        Disciplina d("T02_QUI", "Química Geral", "TEORICA", 45, 3, RegimeDisciplina::SEMESTRAL, SemestreLetivo::SEMESTRE_1);
        d.setProfessores({"P02"});
        d.setTurmas({"T02"});
        data.addDisciplina(d);
    }
    {
        Disciplina d("T02_HIST", "História", "TEORICA", 45, 3, RegimeDisciplina::SEMESTRAL, SemestreLetivo::SEMESTRE_1);
        d.setProfessores({"P04"});
        d.setTurmas({"T02"});
        data.addDisciplina(d);
    }
    {
        Disciplina d("T02_BIO", "Biologia", "TEORICA", 50, 3, RegimeDisciplina::SEMESTRAL, SemestreLetivo::SEMESTRE_2);
        d.setProfessores({"P06"});
        d.setTurmas({"T02"});
        data.addDisciplina(d);
    }

    // Turma T03
    {
        Disciplina d("T03_MAT", "Matemática Avançada", "TEORICA", 50, 4, RegimeDisciplina::ANUAL, SemestreLetivo::AMBOS);
        d.setProfessores({"P01"});
        d.setTurmas({"T03"});
        data.addDisciplina(d);
    }
    {
        Disciplina d("T03_FIS", "Física Moderna", "TEORICA", 50, 4, RegimeDisciplina::ANUAL, SemestreLetivo::AMBOS);
        d.setProfessores({"P03"});
        d.setTurmas({"T03"});
        data.addDisciplina(d);
    }
    {
        Disciplina d("T03_QUI_LAB", "Química Orgânica (Prática)", "LABORATORIO", 50, 3, RegimeDisciplina::SEMESTRAL, SemestreLetivo::SEMESTRE_1);
        d.setProfessores({"P02"});
        d.setTurmas({"T03"});
        d.setSalasAlternativas({{"LAB01"}, {"LAB02"}});
        data.addDisciplina(d);
    }
    {
        Disciplina d("T03_LIT", "Literatura & Redação", "TEORICA", 50, 4, RegimeDisciplina::ANUAL, SemestreLetivo::AMBOS);
        d.setProfessores({"P05"});
        d.setTurmas({"T03"});
        data.addDisciplina(d);
    }
    {
        Disciplina d("T03_BIO", "Genética & Biologia", "TEORICA", 45, 3, RegimeDisciplina::SEMESTRAL, SemestreLetivo::SEMESTRE_2);
        d.setProfessores({"P06"});
        d.setTurmas({"T03"});
        data.addDisciplina(d);
    }

    // Gera as ocorrências das aulas
    data.gerarOcorrenciasAulas(false, data.config().escopo_semestre);

    return data;
}

namespace {

bool timeIntervalsOverlap(const scheduler::model::TimeInterval& t1,
                          const scheduler::model::TimeInterval& t2) {
    return std::max(t1.start_min, t2.start_min) < std::min(t1.end_min, t2.end_min);
}

bool semestersConflict(const scheduler::model::AllocatedLesson& a1,
                       const scheduler::model::AllocatedLesson& a2) {
    using namespace scheduler::model;
    if (a1.regime == RegimeDisciplina::ANUAL || a2.regime == RegimeDisciplina::ANUAL) return true;
    int s1 = (a1.semestre == SemestreLetivo::SEMESTRE_2 || a1.semestre_alocado == 2) ? 2 : 1;
    int s2 = (a2.semestre == SemestreLetivo::SEMESTRE_2 || a2.semestre_alocado == 2) ? 2 : 1;
    return s1 == s2;
}

std::string formatTurmaNames(const scheduler::model::InstitutionData& data,
                             const scheduler::model::AllocatedLesson& lesson) {
    std::string out;
    for (const auto& t_id : lesson.turmas) {
        const auto* t = data.findTurma(t_id);
        if (!out.empty()) out += ", ";
        out += t ? t->nome() : t_id;
    }
    return out.empty() ? lesson.disciplina_nome : out;
}

std::vector<scheduler::model::SlotConflictInfo> evaluateLessonPlacement(
    const scheduler::model::InstitutionData& data,
    const scheduler::model::AllocatedLesson& target,
    const std::vector<std::string>& ignore_aula_ids) {

    using namespace scheduler::model;
    std::vector<SlotConflictInfo> result;
    const auto& alocs = data.grade().alocacoes();

    auto is_ignored = [&](const std::string& aid) {
        return std::find(ignore_aula_ids.begin(), ignore_aula_ids.end(), aid) != ignore_aula_ids.end();
    };

    // 1. Restrições de Turmas + choques de turma + limite diário
    for (const auto& t_id : target.turmas) {
        const auto* t = data.findTurma(t_id);
        std::string t_nome = t ? t->nome() : t_id;
        if (t) {
            if (!t->funcionaNoDia(target.dia)) {
                result.push_back({
                    SlotConflictType::TURMA_INATIVA,
                    "Turma inativa",
                    "Turma inativa: A Turma '" + t_nome + "' não funciona na " + dayToLongString(target.dia) + ".",
                    ""
                });
            }
            auto j = t->jornadaParaDia(target.dia);
            auto j_total = j.jornadaTotal();
            for (const auto& inter : j.intervalosAtivos()) {
                if (timeIntervalsOverlap(inter, target.intervalo)) {
                    result.push_back({
                        SlotConflictType::INTERVALO_RECREIO,
                        "Intervalo",
                        "Intervalo de recreio: Sobrepõe o recreio (" + inter.toString() + ") da Turma '" + t_nome + "'.",
                        ""
                    });
                }
            }
            if (j.slots_letivos.empty() || target.intervalo.start_min < j_total.start_min || target.intervalo.end_min > j_total.end_min) {
                result.push_back({
                    SlotConflictType::FORA_JORNADA,
                    "Fora da jornada",
                    "Fora da jornada: O horário ultrapassa a jornada letiva da Turma '" + t_nome + "' (" + j_total.toString() + ").",
                    ""
                });
            } else if (!j.contemSlotExato(target.intervalo)) {
                result.push_back({
                    SlotConflictType::SLOT_INCOMPATIVEL,
                    "Slot incompatível",
                    "Slot incompatível: O horário (" + target.intervalo.toString() + ") não corresponde a um slot cadastrado na Turma '" + t_nome + "'.",
                    ""
                });
            }
        }

        // Choque com outras aulas da mesma turma
        for (const auto& other : alocs) {
            if (is_ignored(other.aula_id)) continue;
            if (other.dia == target.dia && semestersConflict(target, other) && timeIntervalsOverlap(target.intervalo, other.intervalo)) {
                if (std::find(other.turmas.begin(), other.turmas.end(), t_id) != other.turmas.end()) {
                    result.push_back({
                        SlotConflictType::CHOQUE_TURMA,
                        "Turma ocupada (" + t_nome + ")",
                        "Choque de Turma: Conflita com a aula '" + other.disciplina_nome + "' (" + other.aula_id + ") da Turma '" + t_nome + "' neste mesmo horário.",
                        other.aula_id
                    });
                }
            }
        }

        // Limite diário de aulas da mesma disciplina na turma
        if (data.config().distribuir_disciplinas_na_semana) {
            const auto* disc = data.findDisciplina(target.disciplina_id);
            int max_lim = disc ? disc->maxAulasPorDiaTurma() : 0;
            if (max_lim > 0) {
                int other_count = 0;
                for (const auto& other : alocs) {
                    if (is_ignored(other.aula_id)) continue;
                    if (other.dia == target.dia && other.disciplina_id == target.disciplina_id && semestersConflict(target, other)) {
                        if (std::find(other.turmas.begin(), other.turmas.end(), t_id) != other.turmas.end()) {
                            other_count++;
                        }
                    }
                }
                if (other_count + 1 > max_lim) {
                    result.push_back({
                        SlotConflictType::LIMITE_DIARIO_DISCIPLINA,
                        "Limite diário (" + t_nome + ")",
                        "Limite diário excedido: " + std::to_string(other_count + 1) + " aulas da disciplina alocadas hoje para a Turma '" +
                            t_nome + "' (máximo permitido: " + std::to_string(max_lim) + ").",
                        ""
                    });
                }
            }
        }
    }

    // 2. Restrições de Professores + choques de professor
    for (const auto& p_id : target.professores) {
        const auto* p = data.findProfessor(p_id);
        std::string p_nome = p ? p->nome() : p_id;
        if (p && !p->isDisponivelNoIntervalo(target.dia, target.intervalo.start_min, target.intervalo.end_min)) {
            result.push_back({
                SlotConflictType::INDISPONIBILIDADE_PROFESSOR,
                "Prof. indisponível",
                "Indisponibilidade docente: O Prof. '" + p_nome + "' possui indisponibilidade cadastrada neste horário.",
                ""
            });
        }

        for (const auto& other : alocs) {
            if (is_ignored(other.aula_id)) continue;
            if (other.dia == target.dia && semestersConflict(target, other) && timeIntervalsOverlap(target.intervalo, other.intervalo)) {
                if (std::find(other.professores.begin(), other.professores.end(), p_id) != other.professores.end()) {
                    std::string ctx = formatTurmaNames(data, other);
                    result.push_back({
                        SlotConflictType::CHOQUE_PROFESSOR,
                        "Prof. ocupado (" + ctx + ")",
                        "Choque de Professor: O Prof. '" + p_nome + "' já ministra a aula '" + other.disciplina_nome +
                            "' (" + ctx + ", " + other.aula_id + ") neste mesmo horário.",
                        other.aula_id
                    });
                }
            }
        }
    }

    // 3. Restrições de Salas + choques de sala
    for (const auto& s_id : target.salas) {
        if (s_id.empty()) continue;
        const auto* s = data.findSala(s_id);
        std::string s_nome = s ? s->nome() : s_id;
        if (target.tipo == "LABORATORIO" && s && s->isTeorica()) {
            result.push_back({
                SlotConflictType::INCOMPATIBILIDADE_SALA,
                "Sala incompatível (Lab)",
                "Incompatibilidade de sala: A aula requer Laboratório, mas a Sala '" + s_nome + "' é Teórica.",
                ""
            });
        }

        for (const auto& other : alocs) {
            if (is_ignored(other.aula_id)) continue;
            if (other.dia == target.dia && semestersConflict(target, other) && timeIntervalsOverlap(target.intervalo, other.intervalo)) {
                if (std::find(other.salas.begin(), other.salas.end(), s_id) != other.salas.end()) {
                    std::string ctx = formatTurmaNames(data, other);
                    result.push_back({
                        SlotConflictType::CHOQUE_SALA,
                        "Sala ocupada (" + ctx + ")",
                        "Choque de Sala: A Sala '" + s_nome + "' já está ocupada pela aula '" + other.disciplina_nome +
                            "' (" + ctx + ", " + other.aula_id + ") neste mesmo horário.",
                        other.aula_id
                    });
                }
            }
        }
    }

    return result;
}

} // namespace

std::map<std::string, std::vector<SlotConflictInfo>> InstitutionData::verificarConflitosGradeDetalhado() const {
    std::map<std::string, std::vector<SlotConflictInfo>> conflitos;
    const auto& alocs = grade_.alocacoes();
    if (alocs.empty()) return conflitos;

    for (const auto& a : alocs) {
        auto list = evaluateLessonPlacement(*this, a, {a.aula_id});
        if (!list.empty()) {
            conflitos[a.aula_id] = std::move(list);
        }
    }
    return conflitos;
}

std::map<std::string, std::vector<std::string>> InstitutionData::verificarConflitosGrade() const {
    std::map<std::string, std::vector<std::string>> conflitos;
    auto detalhado = verificarConflitosGradeDetalhado();
    for (const auto& [aid, items] : detalhado) {
        auto& vec = conflitos[aid];
        for (const auto& item : items) {
            vec.push_back(item.detail);
        }
    }
    return conflitos;
}

std::vector<SlotConflictInfo> InstitutionData::avaliarAulaNoHorario(
    const std::string& aula_id,
    DayOfWeek dia,
    const TimeInterval& intervalo,
    const std::vector<std::string>& ignore_aula_ids,
    const std::vector<std::string>& salas_override) const {

    AllocatedLesson target;
    if (const auto* aloc = grade_.findAula(aula_id)) {
        target = *aloc;
    } else if (const auto* lesson = findAula(aula_id)) {
        target.aula_id = lesson->id();
        target.disciplina_id = lesson->disciplinaId();
        target.disciplina_nome = lesson->disciplinaNome();
        target.tipo = lesson->tipo();
        target.professores = lesson->professores();
        target.turmas = lesson->turmas();
        target.salas = lesson->pinnedRooms();
        target.regime = lesson->regime();
        target.semestre = lesson->semestre();
        target.semestre_alocado = lesson->semestreAtivo();
    } else {
        return {};
    }

    target.dia = dia;
    target.intervalo = intervalo;
    if (!salas_override.empty()) {
        target.salas = salas_override;
    }

    std::vector<std::string> ignores = ignore_aula_ids;
    if (std::find(ignores.begin(), ignores.end(), aula_id) == ignores.end()) {
        ignores.push_back(aula_id);
    }
    return evaluateLessonPlacement(*this, target, ignores);
}

std::vector<SlotConflictInfo> InstitutionData::avaliarTrocaAulas(
    const std::string& aula_a_id,
    const std::string& aula_b_id) const {

    std::vector<SlotConflictInfo> conflitos;
    const auto* aloc_a = grade_.findAula(aula_a_id);
    const auto* aloc_b = grade_.findAula(aula_b_id);
    if (!aloc_a || !aloc_b || aula_a_id == aula_b_id) return conflitos;

    if (aloc_a->intervalo.durationMinutes() != aloc_b->intervalo.durationMinutes()) {
        conflitos.push_back({
            SlotConflictType::DURACAO_INCOMPATIVEL,
            "Duração diferente",
            "Duração incompatível: '" + aloc_a->disciplina_nome + "' (" +
                std::to_string(aloc_a->intervalo.durationMinutes()) + " min) vs '" +
                aloc_b->disciplina_nome + "' (" + std::to_string(aloc_b->intervalo.durationMinutes()) + " min).",
            aula_b_id
        });
    }

    std::vector<std::string> ignores = {aula_a_id, aula_b_id};
    TimeInterval inter_a_at_b(aloc_b->intervalo.start_min,
                              aloc_b->intervalo.start_min + aloc_a->intervalo.durationMinutes());
    TimeInterval inter_b_at_a(aloc_a->intervalo.start_min,
                              aloc_a->intervalo.start_min + aloc_b->intervalo.durationMinutes());

    auto conf_a = avaliarAulaNoHorario(aula_a_id, aloc_b->dia, inter_a_at_b, ignores);
    for (auto& c : conf_a) {
        conflitos.push_back(std::move(c));
    }

    auto conf_b = avaliarAulaNoHorario(aula_b_id, aloc_a->dia, inter_b_at_a, ignores);
    for (auto& c : conf_b) {
        c.detail = "[Na origem] " + aloc_b->disciplina_nome + ": " + c.detail;
        conflitos.push_back(std::move(c));
    }

    return conflitos;
}

namespace {

std::string formatTurmasResumo(const InstitutionData& data, const std::vector<std::string>& turma_ids) {
    std::string out;
    for (size_t i = 0; i < turma_ids.size(); ++i) {
        if (i > 0) out += ", ";
        const auto* t = data.findTurma(turma_ids[i]);
        out += (t && !t->nome().empty()) ? t->nome() : turma_ids[i];
    }
    return out;
}

bool alocacaoAtendeSemestre(const AllocatedLesson& aloc, SemestreLetivo sem) {
    if (aloc.regime == RegimeDisciplina::ANUAL) {
        return true;
    }
    if (sem == SemestreLetivo::SEMESTRE_1) {
        return aloc.semestre == SemestreLetivo::SEMESTRE_1 ||
               (aloc.semestre == SemestreLetivo::AMBOS && aloc.semestre_alocado != 2);
    }
    if (sem == SemestreLetivo::SEMESTRE_2) {
        return aloc.semestre == SemestreLetivo::SEMESTRE_2 ||
               (aloc.semestre == SemestreLetivo::AMBOS && aloc.semestre_alocado == 2);
    }
    return true;
}

} // namespace

ProfessorWorkloadSummary InstitutionData::calcularCargaProfessor(const std::string& prof_id) const {
    ProfessorWorkloadSummary summary;

    if (const auto* prof = findProfessor(prof_id)) {
        for (const auto& [dia, raw_intervals] : prof->todasDisponibilidades()) {
            (void)dia;
            std::vector<TimeInterval> intervals;
            for (const auto& inter : raw_intervals) {
                if (inter.durationMinutes() > 0) {
                    intervals.push_back(inter);
                }
            }
            if (intervals.empty()) continue;

            std::sort(intervals.begin(), intervals.end());
            int day_minutes = 0;
            TimeInterval current = intervals.front();
            for (size_t i = 1; i < intervals.size(); ++i) {
                if (intervals[i].start_min <= current.end_min) {
                    current.end_min = std::max(current.end_min, intervals[i].end_min);
                } else {
                    day_minutes += current.durationMinutes();
                    current = intervals[i];
                }
            }
            day_minutes += current.durationMinutes();
            if (day_minutes > 0) {
                summary.minutos_disponiveis += day_minutes;
                summary.dias_com_disponibilidade++;
            }
        }
    }

    for (const auto& d : disciplinas_) {
        const auto& profs = d.professores();
        if (std::find(profs.begin(), profs.end(), prof_id) == profs.end()) {
            continue;
        }

        const int aulas = std::max(0, d.aulasSemanais());
        const int dur = std::max(0, d.duracaoMinutos());
        const int mins = aulas * dur;
        const bool s1 = d.atendeSemestre(SemestreLetivo::SEMESTRE_1);
        const bool s2 = d.atendeSemestre(SemestreLetivo::SEMESTRE_2);

        if (s1) {
            summary.minutos_s1 += mins;
            summary.aulas_s1 += aulas;
        }
        if (s2) {
            summary.minutos_s2 += mins;
            summary.aulas_s2 += aulas;
        }

        DisciplinaCargaItem item;
        item.disciplina_id = d.id();
        item.disciplina_nome = d.nome();
        item.turmas_resumo = formatTurmasResumo(*this, d.turmas());
        item.regime_semestre = d.regimeSemestreFormatado();
        item.aulas_semanais = aulas;
        item.duracao_minutos = dur;
        item.minutos_semanais = mins;
        item.atende_s1 = s1;
        item.atende_s2 = s2;
        item.is_alternativa = false;
        summary.disciplinas.push_back(std::move(item));
    }

    for (const auto& aloc : grade_.alocacoes()) {
        if (std::find(aloc.professores.begin(), aloc.professores.end(), prof_id) == aloc.professores.end()) {
            continue;
        }
        const int dur = std::max(0, aloc.intervalo.durationMinutes());
        if (alocacaoAtendeSemestre(aloc, SemestreLetivo::SEMESTRE_1)) {
            summary.alocado_minutos_s1 += dur;
            summary.alocado_aulas_s1 += 1;
        }
        if (alocacaoAtendeSemestre(aloc, SemestreLetivo::SEMESTRE_2)) {
            summary.alocado_minutos_s2 += dur;
            summary.alocado_aulas_s2 += 1;
        }
    }

    return summary;
}

RoomWorkloadSummary InstitutionData::calcularCargaSala(const std::string& sala_id) const {
    RoomWorkloadSummary summary;
    const auto* sala_ptr = findSala(sala_id);

    enum class RoomLinkKind { NENHUM, FIXO, ALTERNATIVO };

    for (const auto& d : disciplinas_) {
        RoomLinkKind link = RoomLinkKind::NENHUM;

        // Regra 1: salas explicitas na disciplina (mesma precedencia de Preprocessor::gerarCandidatosParaAula)
        std::vector<std::vector<std::string>> combos_validos;
        for (const auto& combo : d.salasAlternativas()) {
            std::vector<std::string> clean_combo;
            for (const auto& sid : combo) {
                if (!sid.empty()) clean_combo.push_back(sid);
            }
            if (!clean_combo.empty()) {
                combos_validos.push_back(std::move(clean_combo));
            }
        }

        if (!combos_validos.empty()) {
            bool in_all = true;
            bool in_any = false;
            for (const auto& combo : combos_validos) {
                const bool found = std::find(combo.begin(), combo.end(), sala_id) != combo.end();
                if (found) {
                    in_any = true;
                } else {
                    in_all = false;
                }
            }
            if (in_all) {
                link = RoomLinkKind::FIXO;
            } else if (in_any) {
                link = RoomLinkKind::ALTERNATIVO;
            }
        } else if (d.isTeorica()) {
            // Regra 2: disciplina teorica sem salas explicitas -> sala fixa ou permitidas da turma principal
            if (!d.turmas().empty()) {
                if (const auto* turma = findTurma(d.turmas().front())) {
                    if (turma->temSalaTeoricaPredefinida()) {
                        if (turma->salaTeoricaId() == sala_id) {
                            link = RoomLinkKind::FIXO;
                        }
                    } else {
                        std::vector<std::string> valid_perm;
                        for (const auto& sid : turma->salasTeoricasPermitidas()) {
                            if (!sid.empty() && findSala(sid) &&
                                std::find(valid_perm.begin(), valid_perm.end(), sid) == valid_perm.end()) {
                                valid_perm.push_back(sid);
                            }
                        }
                        if (valid_perm.empty()) {
                            for (const auto& sid : turma->salasTeoricasPermitidas()) {
                                if (!sid.empty() &&
                                    std::find(valid_perm.begin(), valid_perm.end(), sid) == valid_perm.end()) {
                                    valid_perm.push_back(sid);
                                }
                            }
                        }
                        if (valid_perm.size() == 1 && valid_perm.front() == sala_id) {
                            link = RoomLinkKind::FIXO;
                        } else if (std::find(valid_perm.begin(), valid_perm.end(), sala_id) != valid_perm.end()) {
                            link = RoomLinkKind::ALTERNATIVO;
                        }
                    }
                }
            }
        } else {
            // Regra 3: atividade nao teorica sem salas explicitas -> todas as salas compativeis
            if (sala_ptr && (sala_ptr->tipo() == d.tipo() || !sala_ptr->isTeorica())) {
                int compativeis = 0;
                for (const auto& r : salas_) {
                    if (r.tipo() == d.tipo() || !r.isTeorica()) {
                        compativeis++;
                    }
                }
                link = (compativeis <= 1) ? RoomLinkKind::FIXO : RoomLinkKind::ALTERNATIVO;
            }
        }

        if (link == RoomLinkKind::NENHUM) {
            continue;
        }

        const int aulas = std::max(0, d.aulasSemanais());
        const int dur = std::max(0, d.duracaoMinutos());
        const int mins = aulas * dur;
        const bool s1 = d.atendeSemestre(SemestreLetivo::SEMESTRE_1);
        const bool s2 = d.atendeSemestre(SemestreLetivo::SEMESTRE_2);

        DisciplinaCargaItem item;
        item.disciplina_id = d.id();
        item.disciplina_nome = d.nome();
        item.turmas_resumo = formatTurmasResumo(*this, d.turmas());
        item.regime_semestre = d.regimeSemestreFormatado();
        item.aulas_semanais = aulas;
        item.duracao_minutos = dur;
        item.minutos_semanais = mins;
        item.atende_s1 = s1;
        item.atende_s2 = s2;
        item.is_alternativa = (link == RoomLinkKind::ALTERNATIVO);

        if (link == RoomLinkKind::FIXO) {
            if (s1) {
                summary.fixo_minutos_s1 += mins;
                summary.fixo_aulas_s1 += aulas;
            }
            if (s2) {
                summary.fixo_minutos_s2 += mins;
                summary.fixo_aulas_s2 += aulas;
            }
            summary.disciplinas_fixas.push_back(std::move(item));
        } else {
            if (s1) {
                summary.alt_minutos_s1 += mins;
                summary.alt_aulas_s1 += aulas;
            }
            if (s2) {
                summary.alt_minutos_s2 += mins;
                summary.alt_aulas_s2 += aulas;
            }
            summary.disciplinas_alternativas.push_back(std::move(item));
        }
    }

    for (const auto& aloc : grade_.alocacoes()) {
        if (std::find(aloc.salas.begin(), aloc.salas.end(), sala_id) == aloc.salas.end()) {
            continue;
        }
        const int dur = std::max(0, aloc.intervalo.durationMinutes());
        if (alocacaoAtendeSemestre(aloc, SemestreLetivo::SEMESTRE_1)) {
            summary.alocado_minutos_s1 += dur;
            summary.alocado_aulas_s1 += 1;
        }
        if (alocacaoAtendeSemestre(aloc, SemestreLetivo::SEMESTRE_2)) {
            summary.alocado_minutos_s2 += dur;
            summary.alocado_aulas_s2 += 1;
        }
    }

    return summary;
}

} // namespace scheduler::model
