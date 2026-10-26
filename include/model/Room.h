#pragma once

/**
 * @file Room.h
 * @brief Definição da classe Room, que representa uma sala de aula ou laboratório.
 */

#include <string>
#include <vector>

namespace scheduler::model {

/**
 * @class Room
 * @brief Representa um espaço físico de ensino (sala de aula, laboratório, auditório, etc.).
 *
 * Contém informações sobre identificador, nome descritivo, tipo de sala (ex: "TEORICA", "LABORATORIO"),
 * capacidade máxima de alunos e recursos ou equipamentos específicos disponíveis.
 */
class Room {
public:
    /**
     * @brief Construtor padrão da classe Room.
     */
    Room() = default;

    /**
     * @brief Construtor parametrizado para inicialização de uma sala de aula.
     * @param id Identificador único da sala (ex: "S101", "LAB01").
     * @param nome Nome descritivo da sala (ex: "Sala Teórica 101").
     * @param tipo Tipo de sala ("TEORICA", "LABORATORIO", etc.). Padrão: "TEORICA".
     * @param capacidade Capacidade máxima de estudantes suportada. Padrão: 40.
     */
    Room(std::string id, std::string nome, std::string tipo = "TEORICA", int capacidade = 40)
        : id_(std::move(id)), nome_(std::move(nome)), tipo_(std::move(tipo)), capacidade_(capacidade) {}

    /**
     * @brief Obtém o identificador único da sala.
     * @return Referência constante para a string do ID.
     */
    const std::string& id() const noexcept { return id_; }

    /**
     * @brief Define o identificador único da sala.
     * @param id Novo identificador da sala.
     */
    void setId(std::string id) { id_ = std::move(id); }

    /**
     * @brief Obtém o nome descritivo da sala.
     * @return Referência constante para a string do nome.
     */
    const std::string& nome() const noexcept { return nome_; }

    /**
     * @brief Define o nome descritivo da sala.
     * @param nome Novo nome descritivo da sala.
     */
    void setNome(std::string nome) { nome_ = std::move(nome); }

    /**
     * @brief Obtém o tipo da sala.
     * @return Referência constante para a string do tipo (ex: "TEORICA", "LABORATORIO").
     */
    const std::string& tipo() const noexcept { return tipo_; }

    /**
     * @brief Define o tipo da sala.
     * @param tipo Tipo da sala (ex: "TEORICA", "LABORATORIO").
     */
    void setTipo(std::string tipo) { tipo_ = std::move(tipo); }

    /**
     * @brief Obtém a capacidade máxima de alunos da sala.
     * @return Inteiro com a capacidade máxima.
     */
    int capacidade() const noexcept { return capacidade_; }

    /**
     * @brief Define a capacidade máxima de alunos da sala.
     * @param capacidade Novo valor de capacidade máxima.
     */
    void setCapacidade(int capacidade) noexcept { capacidade_ = capacidade; }

    /**
     * @brief Obtém a lista de recursos/equipamentos disponíveis na sala.
     * @return Referência constante para o vetor de nomes de recursos.
     */
    const std::vector<std::string>& recursos() const noexcept { return recursos_; }

    /**
     * @brief Define a lista de recursos/equipamentos da sala.
     * @param recursos Vetor de strings contendo os recursos disponíveis.
     */
    void setRecursos(std::vector<std::string> recursos) { recursos_ = std::move(recursos); }

    /**
     * @brief Adiciona um novo recurso/equipamento à sala.
     * @param recurso Nome do recurso a ser adicionado (ex: "Projetor", "Bancada Química").
     */
    void addRecurso(std::string recurso) { recursos_.push_back(std::move(recurso)); }

    /**
     * @brief Verifica se a sala é de natureza puramente teórica.
     * @return true se o tipo for "TEORICA" (ou "Teorica"), false caso contrário.
     */
    bool isTeorica() const noexcept { return tipo_ == "TEORICA" || tipo_ == "Teorica"; }

private:
    std::string id_;
    std::string nome_;
    std::string tipo_{"TEORICA"};
    int capacidade_{40};
    std::vector<std::string> recursos_;
};

} // namespace scheduler::model
