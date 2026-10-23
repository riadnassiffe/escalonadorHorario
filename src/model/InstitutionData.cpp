#include "model/InstitutionData.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cstdio>

namespace {

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

InstitutionData::InstitutionData() = default;

void InstitutionData::addProfessor(Professor prof) {
    auto it = std::find_if(professores_.begin(), professores_.end(),
                           [&](const Professor& p) { return p.id() == prof.id(); });
    if (it != professores_.end()) {
        *it = std::move(prof);
    } else {
        professores_.push_back(std::move(prof));
    }
}

bool InstitutionData::removeProfessor(const std::string& id) {
    auto it = std::remove_if(professores_.begin(), professores_.end(),
                             [&](const Professor& p) { return p.id() == id; });
    if (it != professores_.end()) {
        professores_.erase(it, professores_.end());
        return true;
    }
    return false;
}

const Professor* InstitutionData::findProfessor(const std::string& id) const {
    for (const auto& p : professores_) {
        if (p.id() == id) return &p;
    }
    return nullptr;
}

Professor* InstitutionData::findProfessor(const std::string& id) {
    for (auto& p : professores_) {
        if (p.id() == id) return &p;
    }
    return nullptr;
}

void InstitutionData::addSala(Room sala) {
    auto it = std::find_if(salas_.begin(), salas_.end(),
                           [&](const Room& r) { return r.id() == sala.id(); });
    if (it != salas_.end()) {
        *it = std::move(sala);
    } else {
        salas_.push_back(std::move(sala));
    }
}

bool InstitutionData::removeSala(const std::string& id) {
    auto it = std::remove_if(salas_.begin(), salas_.end(),
                             [&](const Room& r) { return r.id() == id; });
    if (it != salas_.end()) {
        salas_.erase(it, salas_.end());
        return true;
    }
    return false;
}

const Room* InstitutionData::findSala(const std::string& id) const {
    for (const auto& r : salas_) {
        if (r.id() == id) return &r;
    }
    return nullptr;
}

Room* InstitutionData::findSala(const std::string& id) {
    for (auto& r : salas_) {
        if (r.id() == id) return &r;
    }
    return nullptr;
}

void InstitutionData::addTurma(Turma turma) {
    auto it = std::find_if(turmas_.begin(), turmas_.end(),
                           [&](const Turma& t) { return t.id() == turma.id(); });
    if (it != turmas_.end()) {
        *it = std::move(turma);
    } else {
        turmas_.push_back(std::move(turma));
    }
}

bool InstitutionData::removeTurma(const std::string& id) {
    auto it = std::remove_if(turmas_.begin(), turmas_.end(),
                             [&](const Turma& t) { return t.id() == id; });
    if (it != turmas_.end()) {
        turmas_.erase(it, turmas_.end());
        return true;
    }
    return false;
}

const Turma* InstitutionData::findTurma(const std::string& id) const {
    for (const auto& t : turmas_) {
        if (t.id() == id) return &t;
    }
    return nullptr;
}

Turma* InstitutionData::findTurma(const std::string& id) {
    for (auto& t : turmas_) {
        if (t.id() == id) return &t;
    }
    return nullptr;
}

void InstitutionData::addDisciplina(Disciplina disc) {
    auto it = std::find_if(disciplinas_.begin(), disciplinas_.end(),
                           [&](const Disciplina& d) { return d.id() == disc.id(); });
    if (it != disciplinas_.end()) {
        *it = std::move(disc);
    } else {
        disciplinas_.push_back(std::move(disc));
    }
}

bool InstitutionData::removeDisciplina(const std::string& id) {
    auto it = std::remove_if(disciplinas_.begin(), disciplinas_.end(),
                             [&](const Disciplina& d) { return d.id() == id; });
    if (it != disciplinas_.end()) {
        disciplinas_.erase(it, disciplinas_.end());
        return true;
    }
    return false;
}

const Disciplina* InstitutionData::findDisciplina(const std::string& id) const {
    for (const auto& d : disciplinas_) {
        if (d.id() == id) return &d;
    }
    return nullptr;
}

Disciplina* InstitutionData::findDisciplina(const std::string& id) {
    for (auto& d : disciplinas_) {
        if (d.id() == id) return &d;
    }
    return nullptr;
}

const Lesson* InstitutionData::findAula(const std::string& id) const {
    for (const auto& a : aulas_) {
        if (a.id() == id) return &a;
    }
    return nullptr;
}

Lesson* InstitutionData::findAula(const std::string& id) {
    for (auto& a : aulas_) {
        if (a.id() == id) return &a;
    }
    return nullptr;
}

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
            if (escopo == EscopoSemestreOtimizacao::SEMESTRE_2) {
                aula.setSemestreAtivo(2);
            } else {
                aula.setSemestreAtivo(1);
            }

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

void InstitutionData::clear() {
    professores_.clear();
    salas_.clear();
    turmas_.clear();
    disciplinas_.clear();
    aulas_.clear();
    grade_.clear();
}

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
