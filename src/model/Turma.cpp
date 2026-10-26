/**
 * @file Turma.cpp
 * @brief Implementação dos métodos das classes e estruturas JornadaTurma e Turma.
 */

#include "model/Turma.h"
#include <algorithm>

namespace scheduler::model {

/**
 * @brief Função estática auxiliar para criar TimeInterval de forma segura tratando exceções.
 * @param ini String de horário inicial (ex: "08:00").
 * @param fim String de horário final (ex: "09:40").
 * @return TimeInterval válido ou [0, 0) caso haja falha no parsing.
 */
static TimeInterval makeSafeInterval(const std::string& ini, const std::string& fim) {
    try {
        int s = TimeInterval::timeStringToSlot(ini);
        int e = TimeInterval::timeStringToSlot(fim);
        TimeInterval ti;
        ti.start_slot = s;
        ti.end_slot = e;
        return ti;
    } catch (...) {
        return TimeInterval();
    }
}

/**
 * @brief Construtor de conveniência de JornadaTurma com dois intervalos de recreio ativos.
 * @param ini Início da jornada.
 * @param fim Fim da jornada.
 * @param int1_ini Início do primeiro intervalo.
 * @param int1_fim Fim do primeiro intervalo.
 * @param int2_ini Início do segundo intervalo.
 * @param int2_fim Fim do segundo intervalo.
 */
JornadaTurma::JornadaTurma(const std::string& ini, const std::string& fim,
                           const std::string& int1_ini, const std::string& int1_fim,
                           const std::string& int2_ini, const std::string& int2_fim)
    : jornada_total(makeSafeInterval(ini, fim)),
      intervalo_1(makeSafeInterval(int1_ini, int1_fim)),
      int1_ativo(true),
      intervalo_2(makeSafeInterval(int2_ini, int2_fim)),
      int2_ativo(true),
      intervalo_3(makeSafeInterval("11:00", "11:15")),
      int3_ativo(false) {
}

/**
 * @brief Construtor completo de JornadaTurma com controle individual de até 3 intervalos.
 * @param ini Início da jornada.
 * @param fim Fim da jornada.
 * @param int1_ini Início do 1º intervalo.
 * @param int1_fim Fim do 1º intervalo.
 * @param a1 Flag de ativação do 1º intervalo.
 * @param int2_ini Início do 2º intervalo.
 * @param int2_fim Fim do 2º intervalo.
 * @param a2 Flag de ativação do 2º intervalo.
 * @param int3_ini Início do 3º intervalo.
 * @param int3_fim Fim do 3º intervalo.
 * @param a3 Flag de ativação do 3º intervalo.
 */
JornadaTurma::JornadaTurma(const std::string& ini, const std::string& fim,
                           const std::string& int1_ini, const std::string& int1_fim, bool a1,
                           const std::string& int2_ini, const std::string& int2_fim, bool a2,
                           const std::string& int3_ini, const std::string& int3_fim, bool a3)
    : jornada_total(makeSafeInterval(ini, fim)),
      intervalo_1(makeSafeInterval(int1_ini, int1_fim)),
      int1_ativo(a1),
      intervalo_2(makeSafeInterval(int2_ini, int2_fim)),
      int2_ativo(a2),
      intervalo_3(makeSafeInterval(int3_ini, int3_fim)),
      int3_ativo(a3) {
}

/**
 * @brief Retorna os intervalos de recreio que estão ativos ordenados cronologicamente.
 * @return Vetor contendo os TimeIntervals ativos.
 */
std::vector<TimeInterval> JornadaTurma::intervalosAtivos() const {
    std::vector<TimeInterval> ativos;
    if (int1_ativo && intervalo_1.start_slot < intervalo_1.end_slot) {
        ativos.push_back(intervalo_1);
    }
    if (int2_ativo && intervalo_2.start_slot < intervalo_2.end_slot) {
        ativos.push_back(intervalo_2);
    }
    if (int3_ativo && intervalo_3.start_slot < intervalo_3.end_slot) {
        ativos.push_back(intervalo_3);
    }
    std::sort(ativos.begin(), ativos.end(), [](const TimeInterval& a, const TimeInterval& b) {
        return a.start_slot < b.start_slot;
    });
    return ativos;
}

/**
 * @brief Valida se a jornada e seus intervalos respeitam ordem cronológica e limites.
 * @return true se a jornada for válida, false se houver sobreposição ou extrapolação.
 */
bool JornadaTurma::isValid() const noexcept {
    if (jornada_total.start_slot >= jornada_total.end_slot) return false;

    std::vector<const TimeInterval*> ativos;
    if (int1_ativo) ativos.push_back(&intervalo_1);
    if (int2_ativo) ativos.push_back(&intervalo_2);
    if (int3_ativo) ativos.push_back(&intervalo_3);

    if (ativos.empty()) return true;

    int prev_end = jornada_total.start_slot;
    for (size_t i = 0; i < ativos.size(); ++i) {
        const auto& inter = *ativos[i];
        if (inter.start_slot >= inter.end_slot) return false;
        if (i == 0) {
            if (inter.start_slot <= prev_end) return false;
        } else {
            if (inter.start_slot < prev_end) return false;
        }
        prev_end = inter.end_slot;
    }
    if (prev_end >= jornada_total.end_slot) return false;

    return true;
}

/**
 * @brief Divide a jornada total nos blocos contínuos disponíveis para aulas (excluindo os intervalos).
 * @return Vetor com os blocos de horário letivo da jornada.
 */
std::vector<TimeInterval> JornadaTurma::blocosLetivos() const {
    std::vector<TimeInterval> blocos;
    if (!isValid()) return blocos;

    auto ativos = intervalosAtivos();
    if (ativos.empty()) {
        if (jornada_total.start_slot < jornada_total.end_slot) {
            blocos.emplace_back(jornada_total.start_slot, jornada_total.end_slot);
        }
        return blocos;
    }

    int current = jornada_total.start_slot;
    for (const auto& inter : ativos) {
        if (inter.start_slot > current) {
            blocos.emplace_back(current, inter.start_slot);
        }
        current = std::max(current, inter.end_slot);
    }
    if (jornada_total.end_slot > current) {
        blocos.emplace_back(current, jornada_total.end_slot);
    }
    return blocos;
}

/**
 * @brief Verifica se um slot de tempo coincide com qualquer intervalo de recreio ativo.
 * @param slot Índice do slot (0 a 287).
 * @return true se o slot estiver dentro de um recreio ativo, false caso contrário.
 */
bool JornadaTurma::isSlotEmIntervalo(int slot) const noexcept {
    if (int1_ativo && intervalo_1.contains(slot)) return true;
    if (int2_ativo && intervalo_2.contains(slot)) return true;
    if (int3_ativo && intervalo_3.contains(slot)) return true;
    return false;
}

/**
 * @brief Verifica igualdade estrutural entre duas instâncias de JornadaTurma.
 * @param o Outra JornadaTurma para comparação.
 * @return true se todos os horários e flags de ativação forem idênticos, false caso contrário.
 */
bool JornadaTurma::operator==(const JornadaTurma& o) const noexcept {
    if (jornada_total.start_slot != o.jornada_total.start_slot ||
        jornada_total.end_slot != o.jornada_total.end_slot) {
        return false;
    }
    if (int1_ativo != o.int1_ativo) return false;
    if (int1_ativo && (intervalo_1.start_slot != o.intervalo_1.start_slot ||
                       intervalo_1.end_slot != o.intervalo_1.end_slot)) {
        return false;
    }
    if (int2_ativo != o.int2_ativo) return false;
    if (int2_ativo && (intervalo_2.start_slot != o.intervalo_2.start_slot ||
                       intervalo_2.end_slot != o.intervalo_2.end_slot)) {
        return false;
    }
    if (int3_ativo != o.int3_ativo) return false;
    if (int3_ativo && (intervalo_3.start_slot != o.intervalo_3.start_slot ||
                       intervalo_3.end_slot != o.intervalo_3.end_slot)) {
        return false;
    }
    return true;
}

/**
 * @brief Construtor da classe Turma com inicialização de jornada padrão matutina.
 * @param id Identificador único da turma.
 * @param nome Nome ou identificação da turma.
 * @param turno Turno de estudos da turma.
 */
Turma::Turma(std::string id, std::string nome, ShiftType turno)
    : id_(std::move(id)), nome_(std::move(nome)), turno_(turno) {
    // Configuração padrão: 08:00 as 12:30 (240 min letivos + 2 intervalos de 15 min)
    // Bloco 1: 08:00 - 09:40 (100 min -> 2x50m)
    // Intervalo 1: 09:40 - 09:55
    // Bloco 2: 09:55 - 11:25 (90 min -> 2x45m)
    // Intervalo 2: 11:25 - 11:40
    // Bloco 3: 11:40 - 12:30 (50 min -> 1x50m)
    jornada_padrao_ = JornadaTurma("08:00", "12:30",
                                   "09:40", "09:55", true,
                                   "11:25", "11:40", true,
                                   "12:00", "12:15", false);
}

/**
 * @brief Verifica se a turma tem atividades letivas no dia especificado.
 * @param d Dia da semana avaliado.
 * @return true se d estiver na lista de dias letivos, false caso contrário.
 */
bool Turma::funcionaNoDia(DayOfWeek d) const noexcept {
    return std::find(dias_letivos_.begin(), dias_letivos_.end(), d) != dias_letivos_.end();
}

/**
 * @brief Retorna a jornada correspondente para o dia (exceção se houver ou jornada padrão).
 * @param d Dia da semana consultado.
 * @return JornadaTurma ativa para o dia informado.
 */
JornadaTurma Turma::jornadaParaDia(DayOfWeek d) const {
    auto it = excecoes_por_dia_.find(d);
    if (it != excecoes_por_dia_.end()) {
        return it->second;
    }
    return jornada_padrao_;
}

} // namespace scheduler::model
