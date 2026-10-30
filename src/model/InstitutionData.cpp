#include "model/InstitutionData.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cstdio>

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
InstitutionData::InstitutionData() = default;

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

    // Mapa de aulas fixadas previamente
    std::map<std::string, Lesson> fixadas;
    if (preservar_fixadas) {
        for (const auto& a : aulas_) {
            if (a.isPinned()) {
                fixadas[a.id()] = a;
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
            aula.setTurnosPermitidos(disc.turnosPermitidos());
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
                aula.setPinnedDay(it->second.pinnedDay());
                aula.setPinnedStartSlot(it->second.pinnedStartSlot());
                aula.setPinnedRooms(it->second.pinnedRooms());
            }

            novas_aulas.push_back(std::move(aula));
        }
    }

    aulas_ = std::move(novas_aulas);
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

    // 3. Turmas com jornada e dois intervalos de 15 minutos (seção 2.4)
    Turma t1("T01", "1º Ano A - Ensino Médio", ShiftType::MANHA);
    t1.setSalaTeoricaId("S101");
    t1.setSalasTeoricasPermitidas({"S101"});
    data.addTurma(t1);

    Turma t2("T02", "2º Ano A - Ensino Médio", ShiftType::MANHA);
    t2.setSalaTeoricaId("S102");
    t2.setSalasTeoricasPermitidas({"S102"});
    data.addTurma(t2);

    Turma t3("T03", "3º Ano A - Ensino Médio", ShiftType::MANHA);
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

} // namespace scheduler::model
