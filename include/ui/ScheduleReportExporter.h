#pragma once

#include "model/InstitutionData.h"
#include <string>

namespace scheduler::ui {

class ScheduleReportExporter {
public:
    static bool exportarHtml(const std::string& filepath,
                             const model::InstitutionData& data,
                             std::string& err);

    static bool exportarCsv(const std::string& filepath,
                            const model::InstitutionData& data,
                            std::string& err);

private:
    static std::string gerarTabelaHtmlTurma(const model::InstitutionData& data,
                                            const model::Turma& turma);
    static std::string gerarTabelaHtmlProfessor(const model::InstitutionData& data,
                                                const model::Professor& prof);
    static std::string gerarTabelaHtmlSala(const model::InstitutionData& data,
                                           const model::Room& sala);
};

} // namespace scheduler::ui
