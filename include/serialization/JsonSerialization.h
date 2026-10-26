#pragma once

/**
 * @file JsonSerialization.h
 * @brief Serialização e desserialização de entidades e arquivos do sistema em formato JSON.
 */

#include "model/InstitutionData.h"
#include <nlohmann/json.hpp>
#include <string>

namespace scheduler::serialization {

/**
 * @class JsonSerialization
 * @brief Utilitário estático para persistência, importação e exportação de dados em JSON.
 *
 * Oferece métodos para converter instâncias de InstitutionData, Professor, Room, Turma,
 * Disciplina, OptimizationConfig e Timetable em objetos JSON e vice-versa, além de persistência em disco.
 */
class JsonSerialization {
public:
    /**
     * @brief Converte todos os dados da instituição em um objeto JSON completo.
     * @param data Instância de InstitutionData contendo todas as entidades e a grade.
     * @return Objeto nlohmann::json serializado.
     */
    static nlohmann::json toJson(const model::InstitutionData& data);

    /**
     * @brief Reconstrói uma instância de InstitutionData a partir de um objeto JSON.
     * @param j Objeto JSON contendo os dados da instituição.
     * @return Instância restaurada de InstitutionData.
     */
    static model::InstitutionData fromJson(const nlohmann::json& j);

    /**
     * @brief Salva os dados institucionais em um arquivo JSON no disco.
     * @param filepath Caminho completo ou relativo do arquivo destino.
     * @param data Dados da instituição a serem gravados.
     * @param err String que receberá mensagem descritiva de erro em caso de falha.
     * @return true se a escrita for concluída com sucesso, false caso contrário.
     */
    static bool salvarArquivo(const std::string& filepath, const model::InstitutionData& data, std::string& err);

    /**
     * @brief Carrega os dados institucionais a partir de um arquivo JSON do disco.
     * @param filepath Caminho do arquivo a ser lido.
     * @param data Instância de InstitutionData que receberá os dados carregados.
     * @param err String que receberá mensagem descritiva de erro em caso de falha.
     * @return true se o arquivo for lido e desserializado com sucesso, false caso contrário.
     */
    static bool carregarArquivo(const std::string& filepath, model::InstitutionData& data, std::string& err);

    // ==========================================
    // Métodos para entidades individuais
    // ==========================================

    /**
     * @brief Converte um objeto Professor em representação JSON.
     * @param prof Instância de Professor.
     * @return Objeto JSON contendo dados cadastrais, disponibilidades e preferências.
     */
    static nlohmann::json professorToJson(const model::Professor& prof);

    /**
     * @brief Constrói um objeto Professor a partir de um nó JSON.
     * @param j Objeto JSON com as informações do docente.
     * @return Instância desserializada de Professor.
     */
    static model::Professor professorFromJson(const nlohmann::json& j);

    /**
     * @brief Converte um objeto Room em representação JSON.
     * @param room Instância de Room.
     * @return Objeto JSON com identificador, nome, tipo, capacidade e recursos.
     */
    static nlohmann::json roomToJson(const model::Room& room);

    /**
     * @brief Constrói um objeto Room a partir de um nó JSON.
     * @param j Objeto JSON com os dados da sala.
     * @return Instância desserializada de Room.
     */
    static model::Room roomFromJson(const nlohmann::json& j);

    /**
     * @brief Converte um objeto Turma em representação JSON.
     * @param turma Instância de Turma.
     * @return Objeto JSON com jornada padrão, exceções, turnos e salas teóricas.
     */
    static nlohmann::json turmaToJson(const model::Turma& turma);

    /**
     * @brief Constrói um objeto Turma a partir de um nó JSON.
     * @param j Objeto JSON com as configurações da turma.
     * @return Instância desserializada de Turma.
     */
    static model::Turma turmaFromJson(const nlohmann::json& j);

    /**
     * @brief Converte um objeto Disciplina em representação JSON.
     * @param disc Instância de Disciplina.
     * @return Objeto JSON com dados da disciplina, professores, turmas e requisitos de salas.
     */
    static nlohmann::json disciplinaToJson(const model::Disciplina& disc);

    /**
     * @brief Constrói um objeto Disciplina a partir de um nó JSON.
     * @param j Objeto JSON com os parâmetros da disciplina.
     * @return Instância desserializada de Disciplina.
     */
    static model::Disciplina disciplinaFromJson(const nlohmann::json& j);

    /**
     * @brief Converte os parâmetros de OptimizationConfig em representação JSON.
     * @param cfg Parâmetros e pesos de otimização.
     * @return Objeto JSON com a configuração.
     */
    static nlohmann::json configToJson(const model::OptimizationConfig& cfg);

    /**
     * @brief Constrói um objeto OptimizationConfig a partir de um nó JSON.
     * @param j Objeto JSON com as configurações do solver.
     * @return Instância desserializada de OptimizationConfig.
     */
    static model::OptimizationConfig configFromJson(const nlohmann::json& j);

    /**
     * @brief Converte uma grade horária Timetable e suas alocações em JSON.
     * @param tt Instância de Timetable.
     * @return Objeto JSON com as aulas alocadas, salas fixas e métricas.
     */
    static nlohmann::json timetableToJson(const model::Timetable& tt);

    /**
     * @brief Constrói um objeto Timetable a partir de um nó JSON.
     * @param j Objeto JSON com a grade horária alocada.
     * @return Instância desserializada de Timetable.
     */
    static model::Timetable timetableFromJson(const nlohmann::json& j);
};

} // namespace scheduler::serialization
