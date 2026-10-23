#include "serialization/JsonSerialization.h"
#include <fstream>
#include <iostream>

namespace scheduler::serialization {

nlohmann::json JsonSerialization::professorToJson(const model::Professor& prof) {
    nlohmann::json j;
    j["id"] = prof.id();
    j["nome"] = prof.nome();

    nlohmann::json pref_dias;
    for (auto d : model::allDays()) {
        pref_dias[model::dayToString(d)] = prof.pesoDia(d);
    }
    j["preferencia_dias"] = pref_dias;

    nlohmann::json disp_json;
    for (const auto& [dia, intervalos] : prof.todasDisponibilidades()) {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& inter : intervalos) {
            arr.push_back({
                {"inicio", inter.startString()},
                {"fim", inter.endString()}
            });
        }
        disp_json[model::dayToString(dia)] = arr;
    }
    j["disponibilidade"] = disp_json;

    j["carga_maxima_diaria_minutos"] = prof.cargaMaximaDiariaMinutos();
    j["max_aulas_por_dia"] = prof.maxAulasPorDia();
    j["peso_janela"] = prof.pesoJanela();

    nlohmann::json turnos_json;
    for (auto d : model::allDays()) {
        nlohmann::json arr = nlohmann::json::array();
        for (auto s : prof.turnosPretendidos(d)) {
            arr.push_back(model::shiftToString(s));
        }
        turnos_json[model::dayToString(d)] = arr;
    }
    j["turnos_pretendidos"] = turnos_json;

    return j;
}

model::Professor JsonSerialization::professorFromJson(const nlohmann::json& j) {
    model::Professor prof;
    if (j.contains("id")) prof.setId(j["id"].get<std::string>());
    if (j.contains("nome")) prof.setNome(j["nome"].get<std::string>());

    if (j.contains("peso_janela")) {
        prof.setPesoJanela(j["peso_janela"].get<int>());
    }

    if (j.contains("preferencia_dias") && j["preferencia_dias"].is_object()) {
        for (auto d : model::allDays()) {
            std::string d_str = model::dayToString(d);
            if (j["preferencia_dias"].contains(d_str)) {
                prof.setPesoDia(d, j["preferencia_dias"][d_str].get<int>());
            }
        }
    }

    if (j.contains("turnos_pretendidos") && j["turnos_pretendidos"].is_object()) {
        for (auto d : model::allDays()) {
            std::string d_str = model::dayToString(d);
            if (j["turnos_pretendidos"].contains(d_str) && j["turnos_pretendidos"][d_str].is_array()) {
                std::vector<model::ShiftType> turnos;
                for (const auto& item : j["turnos_pretendidos"][d_str]) {
                    turnos.push_back(model::stringToShift(item.get<std::string>()));
                }
                prof.setTurnosPretendidos(d, std::move(turnos));
            }
        }
    }

    if (j.contains("disponibilidade") && j["disponibilidade"].is_object()) {
        for (auto d : model::allDays()) {
            std::string d_str = model::dayToString(d);
            if (j["disponibilidade"].contains(d_str) && j["disponibilidade"][d_str].is_array()) {
                std::vector<model::TimeInterval> inters;
                for (const auto& item : j["disponibilidade"][d_str]) {
                    if (item.contains("inicio") && item.contains("fim")) {
                        inters.emplace_back(item["inicio"].get<std::string>(), item["fim"].get<std::string>());
                    }
                }
                prof.setDisponibilidade(d, std::move(inters));
            }
        }
    }

    if (j.contains("carga_maxima_diaria_minutos")) {
        prof.setCargaMaximaDiariaMinutos(j["carga_maxima_diaria_minutos"].get<int>());
    }
    if (j.contains("max_aulas_por_dia")) {
        prof.setMaxAulasPorDia(j["max_aulas_por_dia"].get<int>());
    }
    return prof;
}

nlohmann::json JsonSerialization::roomToJson(const model::Room& room) {
    nlohmann::json j;
    j["id"] = room.id();
    j["nome"] = room.nome();
    j["tipo"] = room.tipo();
    j["capacidade"] = room.capacidade();
    j["recursos"] = room.recursos();
    return j;
}

model::Room JsonSerialization::roomFromJson(const nlohmann::json& j) {
    model::Room r;
    if (j.contains("id")) r.setId(j["id"].get<std::string>());
    if (j.contains("nome")) r.setNome(j["nome"].get<std::string>());
    if (j.contains("tipo")) r.setTipo(j["tipo"].get<std::string>());
    if (j.contains("capacidade")) r.setCapacidade(j["capacidade"].get<int>());
    if (j.contains("recursos") && j["recursos"].is_array()) {
        r.setRecursos(j["recursos"].get<std::vector<std::string>>());
    }
    return r;
}

nlohmann::json JsonSerialization::turmaToJson(const model::Turma& turma) {
    nlohmann::json j;
    j["id"] = turma.id();
    j["nome"] = turma.nome();
    j["turno"] = model::shiftToString(turma.turno());

    nlohmann::json dias_arr = nlohmann::json::array();
    for (auto d : turma.diasLetivos()) {
        dias_arr.push_back(model::dayToString(d));
    }
    j["dias_letivos"] = dias_arr;

    const auto& jp = turma.jornadaPadrao();
    nlohmann::json int_arr = nlohmann::json::array();
    if (jp.int1_ativo || !jp.intervalo_1.startString().empty()) {
        int_arr.push_back({
            {"inicio", jp.intervalo_1.startString()},
            {"fim", jp.intervalo_1.endString()},
            {"ativo", jp.int1_ativo}
        });
    }
    if (jp.int2_ativo || !jp.intervalo_2.startString().empty()) {
        int_arr.push_back({
            {"inicio", jp.intervalo_2.startString()},
            {"fim", jp.intervalo_2.endString()},
            {"ativo", jp.int2_ativo}
        });
    }
    if (jp.int3_ativo || !jp.intervalo_3.startString().empty()) {
        int_arr.push_back({
            {"inicio", jp.intervalo_3.startString()},
            {"fim", jp.intervalo_3.endString()},
            {"ativo", jp.int3_ativo}
        });
    }

    j["jornada_padrao"] = {
        {"inicio", jp.jornada_total.startString()},
        {"fim", jp.jornada_total.endString()},
        {"intervalos", int_arr}
    };

    nlohmann::json exc_obj = nlohmann::json::object();
    for (const auto& [dia, j_exc] : turma.excecoesPorDia()) {
        nlohmann::json exc_int_arr = nlohmann::json::array();
        if (j_exc.int1_ativo || !j_exc.intervalo_1.startString().empty()) {
            exc_int_arr.push_back({
                {"inicio", j_exc.intervalo_1.startString()},
                {"fim", j_exc.intervalo_1.endString()},
                {"ativo", j_exc.int1_ativo}
            });
        }
        if (j_exc.int2_ativo || !j_exc.intervalo_2.startString().empty()) {
            exc_int_arr.push_back({
                {"inicio", j_exc.intervalo_2.startString()},
                {"fim", j_exc.intervalo_2.endString()},
                {"ativo", j_exc.int2_ativo}
            });
        }
        if (j_exc.int3_ativo || !j_exc.intervalo_3.startString().empty()) {
            exc_int_arr.push_back({
                {"inicio", j_exc.intervalo_3.startString()},
                {"fim", j_exc.intervalo_3.endString()},
                {"ativo", j_exc.int3_ativo}
            });
        }
        exc_obj[model::dayToString(dia)] = {
            {"inicio", j_exc.jornada_total.startString()},
            {"fim", j_exc.jornada_total.endString()},
            {"intervalos", exc_int_arr}
        };
    }
    j["excecoes_por_dia"] = exc_obj;

    if (turma.temSalaTeoricaPredefinida()) {
        j["sala_teorica_id"] = turma.salaTeoricaId();
    } else {
        j["sala_teorica_id"] = nullptr;
    }
    j["salas_teoricas_permitidas"] = turma.salasTeoricasPermitidas();

    return j;
}

model::Turma JsonSerialization::turmaFromJson(const nlohmann::json& j) {
    model::Turma t;
    if (j.contains("id")) t.setId(j["id"].get<std::string>());
    if (j.contains("nome")) t.setNome(j["nome"].get<std::string>());
    if (j.contains("turno")) t.setTurno(model::stringToShift(j["turno"].get<std::string>()));

    if (j.contains("dias_letivos") && j["dias_letivos"].is_array()) {
        std::vector<model::DayOfWeek> dias;
        for (const auto& d_val : j["dias_letivos"]) {
            dias.push_back(model::stringToDay(d_val.get<std::string>()));
        }
        t.setDiasLetivos(dias);
    }

    if (j.contains("jornada_padrao") && j["jornada_padrao"].is_object()) {
        const auto& jp_json = j["jornada_padrao"];
        std::string ini = jp_json.value("inicio", "08:00");
        std::string fim = jp_json.value("fim", "11:30");
        std::string int1_i = "08:40", int1_f = "08:55";
        std::string int2_i = "10:25", int2_f = "10:40";
        std::string int3_i = "11:00", int3_f = "11:15";
        bool a1 = false, a2 = false, a3 = false;

        if (jp_json.contains("intervalos") && jp_json["intervalos"].is_array()) {
            const auto& arr = jp_json["intervalos"];
            if (arr.size() >= 1) {
                int1_i = arr[0].value("inicio", int1_i);
                int1_f = arr[0].value("fim", int1_f);
                a1 = arr[0].value("ativo", true);
            }
            if (arr.size() >= 2) {
                int2_i = arr[1].value("inicio", int2_i);
                int2_f = arr[1].value("fim", int2_f);
                a2 = arr[1].value("ativo", true);
            }
            if (arr.size() >= 3) {
                int3_i = arr[2].value("inicio", int3_i);
                int3_f = arr[2].value("fim", int3_f);
                a3 = arr[2].value("ativo", true);
            }
        }
        t.setJornadaPadrao(model::JornadaTurma(ini, fim,
                                               int1_i, int1_f, a1,
                                               int2_i, int2_f, a2,
                                               int3_i, int3_f, a3));
    }

    if (j.contains("excecoes_por_dia") && j["excecoes_por_dia"].is_object()) {
        for (auto it = j["excecoes_por_dia"].begin(); it != j["excecoes_por_dia"].end(); ++it) {
            auto dia = model::stringToDay(it.key());
            const auto& val = it.value();
            std::string ini = val.value("inicio", "08:00");
            std::string fim = val.value("fim", "11:30");
            std::string int1_i = "08:40", int1_f = "08:55";
            std::string int2_i = "10:25", int2_f = "10:40";
            std::string int3_i = "11:00", int3_f = "11:15";
            bool a1 = false, a2 = false, a3 = false;
            if (val.contains("intervalos") && val["intervalos"].is_array()) {
                const auto& arr = val["intervalos"];
                if (arr.size() >= 1) {
                    int1_i = arr[0].value("inicio", int1_i);
                    int1_f = arr[0].value("fim", int1_f);
                    a1 = arr[0].value("ativo", true);
                }
                if (arr.size() >= 2) {
                    int2_i = arr[1].value("inicio", int2_i);
                    int2_f = arr[1].value("fim", int2_f);
                    a2 = arr[1].value("ativo", true);
                }
                if (arr.size() >= 3) {
                    int3_i = arr[2].value("inicio", int3_i);
                    int3_f = arr[2].value("fim", int3_f);
                    a3 = arr[2].value("ativo", true);
                }
            }
            t.setExcecaoDia(dia, model::JornadaTurma(ini, fim,
                                                     int1_i, int1_f, a1,
                                                     int2_i, int2_f, a2,
                                                     int3_i, int3_f, a3));
        }
    }

    if (j.contains("sala_teorica_id") && !j["sala_teorica_id"].is_null()) {
        t.setSalaTeoricaId(j["sala_teorica_id"].get<std::string>());
    } else {
        t.setSalaTeoricaId("");
    }

    if (j.contains("salas_teoricas_permitidas") && j["salas_teoricas_permitidas"].is_array()) {
        t.setSalasTeoricasPermitidas(j["salas_teoricas_permitidas"].get<std::vector<std::string>>());
    }

    return t;
}

nlohmann::json JsonSerialization::disciplinaToJson(const model::Disciplina& disc) {
    nlohmann::json j;
    j["id"] = disc.id();
    j["nome"] = disc.nome();
    j["tipo"] = disc.tipo();
    j["duracao_minutos"] = disc.duracaoMinutos();
    j["aulas_semanais"] = disc.aulasSemanais();
    j["regime"] = model::regimeToString(disc.regime());
    j["semestre"] = model::semestreToString(disc.semestre());
    j["professores"] = disc.professores();
    j["turmas"] = disc.turmas();

    nlohmann::json turnos_arr = nlohmann::json::array();
    for (auto t : disc.turnosPermitidos()) {
        turnos_arr.push_back(model::shiftToString(t));
    }
    j["turnos_permitidos"] = turnos_arr;

    j["salas_alternativas"] = disc.salasAlternativas();
    j["exigir_aulas_consecutivas"] = disc.exigirAulasConsecutivas();
    j["max_aulas_por_dia_turma"] = disc.maxAulasPorDiaTurma();
    return j;
}

model::Disciplina JsonSerialization::disciplinaFromJson(const nlohmann::json& j) {
    model::Disciplina d;
    if (j.contains("id")) d.setId(j["id"].get<std::string>());
    if (j.contains("nome")) d.setNome(j["nome"].get<std::string>());
    if (j.contains("tipo")) d.setTipo(j["tipo"].get<std::string>());
    if (j.contains("duracao_minutos")) d.setDuracaoMinutos(j["duracao_minutos"].get<int>());
    if (j.contains("aulas_semanais")) d.setAulasSemanais(j["aulas_semanais"].get<int>());
    if (j.contains("regime")) {
        d.setRegime(model::stringToRegime(j["regime"].get<std::string>()));
    }
    if (j.contains("semestre")) {
        d.setSemestre(model::stringToSemestre(j["semestre"].get<std::string>()));
    }
    if (j.contains("professores") && j["professores"].is_array()) {
        d.setProfessores(j["professores"].get<std::vector<std::string>>());
    }
    if (j.contains("turmas") && j["turmas"].is_array()) {
        d.setTurmas(j["turmas"].get<std::vector<std::string>>());
    }
    if (j.contains("turnos_permitidos") && j["turnos_permitidos"].is_array()) {
        std::vector<model::ShiftType> turnos;
        for (const auto& item : j["turnos_permitidos"]) {
            turnos.push_back(model::stringToShift(item.get<std::string>()));
        }
        d.setTurnosPermitidos(turnos);
    }
    if (j.contains("salas_alternativas") && j["salas_alternativas"].is_array()) {
        d.setSalasAlternativas(j["salas_alternativas"].get<std::vector<std::vector<std::string>>>());
    }
    if (j.contains("exigir_aulas_consecutivas")) {
        d.setExigirAulasConsecutivas(j["exigir_aulas_consecutivas"].get<bool>());
    }
    if (j.contains("max_aulas_por_dia_turma")) {
        d.setMaxAulasPorDiaTurma(j["max_aulas_por_dia_turma"].get<int>());
    }
    return d;
}

nlohmann::json JsonSerialization::configToJson(const model::OptimizationConfig& cfg) {
    nlohmann::json j;
    j["lambda_janelas"] = cfg.lambda_janelas;
    j["lambda_dias"] = cfg.lambda_dias;
    j["lambda_preferencias"] = cfg.lambda_preferencias;
    j["lambda_lacunas_turma"] = cfg.lambda_lacunas_turma;
    j["lambda_preferencia_dias"] = cfg.lambda_preferencia_dias;
    j["lambda_preferencia_turnos"] = cfg.lambda_preferencia_turnos;
    j["modelo_preferencia_dias"] = model::modeloPreferenciaDiasToString(cfg.modelo_preferencia_dias);
    j["preenchimento_integral_jornada"] = cfg.preenchimento_integral_jornada;
    j["distribuir_disciplinas_na_semana"] = cfg.distribuir_disciplinas_na_semana;
    j["max_aulas_disciplina_dia_turma"] = cfg.max_aulas_disciplina_dia_turma;
    j["escopo_semestre"] = model::escopoSemestreToString(cfg.escopo_semestre);
    j["sincronizar_anuais_entre_semestres"] = cfg.sincronizar_anuais_entre_semestres;
    j["algoritmo"] = model::solverAlgorithmToString(cfg.algoritmo);
    j["tempo_limite_segundos"] = cfg.tempo_limite_segundos;
    j["num_threads"] = cfg.num_threads;
    j["usar_distribuicao_como_entrada"] = cfg.usar_distribuicao_como_entrada;
    return j;
}

model::OptimizationConfig JsonSerialization::configFromJson(const nlohmann::json& j) {
    model::OptimizationConfig cfg;
    if (j.contains("algoritmo")) {
        cfg.algoritmo = model::stringToSolverAlgorithm(j["algoritmo"].get<std::string>());
    }
    if (j.contains("escopo_semestre")) {
        cfg.escopo_semestre = model::stringToEscopoSemestre(j["escopo_semestre"].get<std::string>());
    }
    if (j.contains("sincronizar_anuais_entre_semestres")) {
        cfg.sincronizar_anuais_entre_semestres = j["sincronizar_anuais_entre_semestres"].get<bool>();
    }
    if (j.contains("lambda_janelas")) cfg.lambda_janelas = j["lambda_janelas"].get<int>();
    if (j.contains("lambda_dias")) cfg.lambda_dias = j["lambda_dias"].get<int>();
    if (j.contains("lambda_preferencias")) cfg.lambda_preferencias = j["lambda_preferencias"].get<int>();
    if (j.contains("lambda_lacunas_turma")) cfg.lambda_lacunas_turma = j["lambda_lacunas_turma"].get<int>();
    if (j.contains("lambda_preferencia_dias")) cfg.lambda_preferencia_dias = j["lambda_preferencia_dias"].get<int>();
    if (j.contains("lambda_preferencia_turnos")) cfg.lambda_preferencia_turnos = j["lambda_preferencia_turnos"].get<int>();
    if (j.contains("modelo_preferencia_dias")) {
        cfg.modelo_preferencia_dias = model::stringToModeloPreferenciaDias(j["modelo_preferencia_dias"].get<std::string>());
    }
    if (j.contains("preenchimento_integral_jornada")) {
        cfg.preenchimento_integral_jornada = j["preenchimento_integral_jornada"].get<bool>();
    }
    if (j.contains("distribuir_disciplinas_na_semana")) {
        cfg.distribuir_disciplinas_na_semana = j["distribuir_disciplinas_na_semana"].get<bool>();
    }
    if (j.contains("max_aulas_disciplina_dia_turma")) {
        cfg.max_aulas_disciplina_dia_turma = j["max_aulas_disciplina_dia_turma"].get<int>();
    }
    if (j.contains("tempo_limite_segundos")) {
        cfg.tempo_limite_segundos = j["tempo_limite_segundos"].get<double>();
    }
    if (j.contains("num_threads")) {
        cfg.num_threads = j["num_threads"].get<int>();
    }
    if (j.contains("usar_distribuicao_como_entrada")) {
        cfg.usar_distribuicao_como_entrada = j["usar_distribuicao_como_entrada"].get<bool>();
    }
    return cfg;
}

nlohmann::json JsonSerialization::timetableToJson(const model::Timetable& tt) {
    nlohmann::json j;
    nlohmann::json alocs = nlohmann::json::array();
    for (const auto& a : tt.alocacoes()) {
        alocs.push_back({
            {"aula_id", a.aula_id},
            {"disciplina_id", a.disciplina_id},
            {"disciplina_nome", a.disciplina_nome},
            {"tipo", a.tipo},
            {"dia", model::dayToString(a.dia)},
            {"inicio", a.intervalo.startString()},
            {"fim", a.intervalo.endString()},
            {"professores", a.professores},
            {"turmas", a.turmas},
            {"salas", a.salas},
            {"is_pinned", a.is_pinned},
            {"regime", model::regimeToString(a.regime)},
            {"semestre", model::semestreToString(a.semestre)},
            {"semestre_alocado", a.semestre_alocado}
        });
    }
    j["alocacoes"] = alocs;
    j["salas_teoricas_designadas"] = tt.salasTeoricasDesignadas();

    const auto& s = tt.estatisticas();
    j["estatisticas"] = {
        {"status", s.status},
        {"tempo_execucao_ms", s.tempo_execucao_ms},
        {"valor_objetivo", s.valor_objetivo},
        {"penalidade_janelas_J", s.penalidade_janelas_J},
        {"penalidade_dias_D", s.penalidade_dias_D},
        {"penalidade_preferencias_P", s.penalidade_preferencias_P},
        {"penalidade_preferencia_dias_W", s.penalidade_preferencia_dias_W},
        {"penalidade_lacunas_turma_JG", s.penalidade_lacunas_turma_JG},
        {"total_aulas_alocadas", s.total_aulas_alocadas},
        {"total_aulas_esperadas", s.total_aulas_esperadas}
    };
    return j;
}

model::Timetable JsonSerialization::timetableFromJson(const nlohmann::json& j) {
    model::Timetable tt;
    if (j.contains("alocacoes") && j["alocacoes"].is_array()) {
        for (const auto& item : j["alocacoes"]) {
            model::AllocatedLesson aloc;
            aloc.aula_id = item.value("aula_id", "");
            aloc.disciplina_id = item.value("disciplina_id", "");
            aloc.disciplina_nome = item.value("disciplina_nome", "");
            aloc.tipo = item.value("tipo", "TEORICA");
            aloc.dia = model::stringToDay(item.value("dia", "SEG"));
            std::string ini = item.value("inicio", "08:00");
            std::string fim = item.value("fim", "07:50");
            aloc.intervalo = model::TimeInterval(ini, fim);
            if (item.contains("professores") && item["professores"].is_array()) {
                aloc.professores = item["professores"].get<std::vector<std::string>>();
            }
            if (item.contains("turmas") && item["turmas"].is_array()) {
                aloc.turmas = item["turmas"].get<std::vector<std::string>>();
            }
            if (item.contains("salas") && item["salas"].is_array()) {
                aloc.salas = item["salas"].get<std::vector<std::string>>();
            }
            aloc.is_pinned = item.value("is_pinned", false);
            if (item.contains("regime")) {
                aloc.regime = model::stringToRegime(item["regime"].get<std::string>());
            }
            if (item.contains("semestre")) {
                aloc.semestre = model::stringToSemestre(item["semestre"].get<std::string>());
            }
            aloc.semestre_alocado = item.value("semestre_alocado", 1);
            tt.addAllocatedLesson(std::move(aloc));
        }
    }

    if (j.contains("salas_teoricas_designadas") && j["salas_teoricas_designadas"].is_object()) {
        for (auto& [t_id, r_val] : j["salas_teoricas_designadas"].items()) {
            tt.setSalaTeoricaDesignada(t_id, r_val.get<std::string>());
        }
    }

    if (j.contains("estatisticas") && j["estatisticas"].is_object()) {
        model::TimetableStatistics s;
        const auto& s_json = j["estatisticas"];
        s.status = s_json.value("status", "NAO_EXECUTADO");
        s.tempo_execucao_ms = s_json.value("tempo_execucao_ms", 0.0);
        s.valor_objetivo = s_json.value("valor_objetivo", 0.0);
        s.penalidade_janelas_J = s_json.value("penalidade_janelas_J", 0);
        s.penalidade_dias_D = s_json.value("penalidade_dias_D", 0);
        s.penalidade_preferencias_P = s_json.value("penalidade_preferencias_P", 0);
        s.penalidade_preferencia_dias_W = s_json.value("penalidade_preferencia_dias_W", 0);
        s.penalidade_lacunas_turma_JG = s_json.value("penalidade_lacunas_turma_JG", 0);
        s.total_aulas_alocadas = s_json.value("total_aulas_alocadas", 0);
        s.total_aulas_esperadas = s_json.value("total_aulas_esperadas", 0);
        tt.setEstatisticas(s);
    }
    return tt;
}

nlohmann::json JsonSerialization::toJson(const model::InstitutionData& data) {
    nlohmann::json j;

    nlohmann::json profs = nlohmann::json::array();
    for (const auto& p : data.professores()) {
        profs.push_back(professorToJson(p));
    }
    j["professores"] = profs;

    nlohmann::json salas = nlohmann::json::array();
    for (const auto& s : data.salas()) {
        salas.push_back(roomToJson(s));
    }
    j["salas"] = salas;

    nlohmann::json turmas = nlohmann::json::array();
    for (const auto& t : data.turmas()) {
        turmas.push_back(turmaToJson(t));
    }
    j["turmas"] = turmas;

    nlohmann::json discs = nlohmann::json::array();
    for (const auto& d : data.disciplinas()) {
        discs.push_back(disciplinaToJson(d));
    }
    j["disciplinas"] = discs;

    j["config"] = configToJson(data.config());
    j["grade"] = timetableToJson(data.grade());

    return j;
}

model::InstitutionData JsonSerialization::fromJson(const nlohmann::json& j) {
    model::InstitutionData data;

    if (j.contains("professores") && j["professores"].is_array()) {
        for (const auto& item : j["professores"]) {
            data.addProfessor(professorFromJson(item));
        }
    }
    if (j.contains("salas") && j["salas"].is_array()) {
        for (const auto& item : j["salas"]) {
            data.addSala(roomFromJson(item));
        }
    }
    if (j.contains("turmas") && j["turmas"].is_array()) {
        for (const auto& item : j["turmas"]) {
            data.addTurma(turmaFromJson(item));
        }
    }
    if (j.contains("disciplinas") && j["disciplinas"].is_array()) {
        for (const auto& item : j["disciplinas"]) {
            data.addDisciplina(disciplinaFromJson(item));
        }
    }
    if (j.contains("config") && j["config"].is_object()) {
        data.setConfig(configFromJson(j["config"]));
    }
    if (j.contains("grade") && j["grade"].is_object()) {
        data.setGrade(timetableFromJson(j["grade"]));
    }

    data.gerarOcorrenciasAulas(true);
    return data;
}

bool JsonSerialization::salvarArquivo(const std::string& filepath,
                                      const model::InstitutionData& data,
                                      std::string& err) {
    try {
        std::ofstream ofs(filepath);
        if (!ofs.is_open()) {
            err = "Não foi possível abrir o arquivo para escrita: " + filepath;
            return false;
        }
        nlohmann::json j = toJson(data);
        ofs << j.dump(2);
        return true;
    } catch (const std::exception& e) {
        err = std::string("Erro ao salvar JSON: ") + e.what();
        return false;
    }
}

bool JsonSerialization::carregarArquivo(const std::string& filepath,
                                        model::InstitutionData& data,
                                        std::string& err) {
    try {
        std::ifstream ifs(filepath);
        if (!ifs.is_open()) {
            err = "Não foi possível abrir o arquivo para leitura: " + filepath;
            return false;
        }
        nlohmann::json j;
        ifs >> j;
        data = fromJson(j);
        return true;
    } catch (const std::exception& e) {
        err = std::string("Erro ao carregar JSON: ") + e.what();
        return false;
    }
}

} // namespace scheduler::serialization
