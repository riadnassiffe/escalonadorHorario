#pragma once

#include <string>
#include <vector>

namespace scheduler::model {

class Room {
public:
    Room() = default;
    Room(std::string id, std::string nome, std::string tipo = "TEORICA", int capacidade = 40)
        : id_(std::move(id)), nome_(std::move(nome)), tipo_(std::move(tipo)), capacidade_(capacidade) {}

    const std::string& id() const noexcept { return id_; }
    void setId(std::string id) { id_ = std::move(id); }

    const std::string& nome() const noexcept { return nome_; }
    void setNome(std::string nome) { nome_ = std::move(nome); }

    const std::string& tipo() const noexcept { return tipo_; }
    void setTipo(std::string tipo) { tipo_ = std::move(tipo); }

    int capacidade() const noexcept { return capacidade_; }
    void setCapacidade(int capacidade) noexcept { capacidade_ = capacidade; }

    const std::vector<std::string>& recursos() const noexcept { return recursos_; }
    void setRecursos(std::vector<std::string> recursos) { recursos_ = std::move(recursos); }
    void addRecurso(std::string recurso) { recursos_.push_back(std::move(recurso)); }

    bool isTeorica() const noexcept { return tipo_ == "TEORICA" || tipo_ == "Teorica"; }

private:
    std::string id_;
    std::string nome_;
    std::string tipo_{"TEORICA"};
    int capacidade_{40};
    std::vector<std::string> recursos_;
};

} // namespace scheduler::model
