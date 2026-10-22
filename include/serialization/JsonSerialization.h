#pragma once

#include "model/InstitutionData.h"
#include <nlohmann/json.hpp>
#include <string>

namespace scheduler::serialization {

class JsonSerialization {
public:
    static nlohmann::json toJson(const model::InstitutionData& data);
    static model::InstitutionData fromJson(const nlohmann::json& j);

    static bool salvarArquivo(const std::string& filepath, const model::InstitutionData& data, std::string& err);
    static bool carregarArquivo(const std::string& filepath, model::InstitutionData& data, std::string& err);

    // Métodos para entidades individuais
    static nlohmann::json professorToJson(const model::Professor& prof);
    static model::Professor professorFromJson(const nlohmann::json& j);

    static nlohmann::json roomToJson(const model::Room& room);
    static model::Room roomFromJson(const nlohmann::json& j);

    static nlohmann::json turmaToJson(const model::Turma& turma);
    static model::Turma turmaFromJson(const nlohmann::json& j);

    static nlohmann::json disciplinaToJson(const model::Disciplina& disc);
    static model::Disciplina disciplinaFromJson(const nlohmann::json& j);

    static nlohmann::json configToJson(const model::OptimizationConfig& cfg);
    static model::OptimizationConfig configFromJson(const nlohmann::json& j);

    static nlohmann::json timetableToJson(const model::Timetable& tt);
    static model::Timetable timetableFromJson(const nlohmann::json& j);
};

} // namespace scheduler::serialization
