/**
 * @file ScheduleReportExporter.h
 * @brief Utilitário de exportação de relatórios da grade horária em formatos HTML e CSV.
 */

#pragma once

#include "model/InstitutionData.h"
#include <string>

namespace scheduler::ui {

/**
 * @class ScheduleReportExporter
 * @brief Exportador de relatórios completos de grade escolar em formatos HTML responsivo e CSV tabular.
 *
 * Gera relatórios com formatação visual moderna, estatísticas de otimização (KPIs),
 * e visualizações segmentadas por turmas, professores e salas.
 */
class ScheduleReportExporter {
public:
    /**
     * @brief Exporta a grade horária e estatísticas em relatório HTML estilizado.
     * @param filepath Caminho absoluto ou relativo do arquivo .html a ser gerado.
     * @param data Conjunto de dados institucionais e grade horária gerada.
     * @param err Mensagem de erro retornada em caso de falha.
     * @return true se o arquivo HTML foi escrito com sucesso; false em caso de erro.
     */
    static bool exportarHtml(const std::string& filepath,
                             const model::InstitutionData& data,
                             std::string& err);

    /**
     * @brief Exporta todas as alocações da grade em formato tabular CSV delimitado por ponto e vírgula.
     * @param filepath Caminho do arquivo .csv a ser gerado.
     * @param data Dados institucionais com as alocações.
     * @param err Mensagem de erro em caso de exceção ou falha de escrita.
     * @return true se a gravação foi bem-sucedida; false caso contrário.
     */
    static bool exportarCsv(const std::string& filepath,
                            const model::InstitutionData& data,
                            std::string& err);

private:
    /**
     * @brief Constrói a tabela HTML de horários para uma turma específica.
     * @param data Dados institucionais.
     * @param turma Turma cujos horários serão gerados.
     * @return String contendo o fragmento HTML da tabela da turma.
     */
    static std::string gerarTabelaHtmlTurma(const model::InstitutionData& data,
                                            const model::Turma& turma);

    /**
     * @brief Constrói a tabela HTML de horários para um professor específico.
     * @param data Dados institucionais.
     * @param prof Professor cujos horários serão gerados.
     * @return String contendo o fragmento HTML da tabela do docente.
     */
    static std::string gerarTabelaHtmlProfessor(const model::InstitutionData& data,
                                                const model::Professor& prof);

    /**
     * @brief Constrói a tabela HTML de horários e ocupação de uma sala específica.
     * @param data Dados institucionais.
     * @param sala Sala ou laboratório.
     * @return String contendo o fragmento HTML da tabela da sala.
     */
    static std::string gerarTabelaHtmlSala(const model::InstitutionData& data,
                                           const model::Room& sala);
};

} // namespace scheduler::ui

