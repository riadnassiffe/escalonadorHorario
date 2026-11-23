from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from app.services.time_utils import DAY_LONG_NAMES, compute_implicit_breaks, minutes_to_time_str, time_str_to_minutes


class SlotConflictType:
    TURMA_INATIVA = "TURMA_INATIVA"
    INTERVALO_RECREIO = "INTERVALO_RECREIO"
    FORA_JORNADA = "FORA_JORNADA"
    SLOT_INCOMPATIVEL = "SLOT_INCOMPATIVEL"
    INDISPONIBILIDADE_PROFESSOR = "INDISPONIBILIDADE_PROFESSOR"
    INCOMPATIBILIDADE_SALA = "INCOMPATIBILIDADE_SALA"
    LIMITE_DIARIO_DISCIPLINA = "LIMITE_DIARIO_DISCIPLINA"
    CHOQUE_TURMA = "CHOQUE_TURMA"
    CHOQUE_PROFESSOR = "CHOQUE_PROFESSOR"
    CHOQUE_SALA = "CHOQUE_SALA"
    DURACAO_INCOMPATIVEL = "DURACAO_INCOMPATIVEL"


@dataclass
class ConflictInfo:
    type: str
    short_label: str
    detail: str
    related_aula_id: str = ""

    def to_dict(self) -> dict[str, str]:
        return {
            "type": self.type,
            "short_label": self.short_label,
            "detail": self.detail,
            "related_aula_id": self.related_aula_id,
        }


@dataclass
class ProjectSnapshot:
    professores: dict[str, dict[str, Any]] = field(default_factory=dict)
    salas: dict[str, dict[str, Any]] = field(default_factory=dict)
    turmas: dict[str, dict[str, Any]] = field(default_factory=dict)
    disciplinas: dict[str, dict[str, Any]] = field(default_factory=dict)
    config: dict[str, Any] = field(default_factory=dict)
    alocacoes: list[dict[str, Any]] = field(default_factory=list)


def _overlaps(s1: int, e1: int, s2: int, e2: int) -> bool:
    return max(s1, s2) < min(e1, e2)


def _semesters_conflict(a1: dict[str, Any], a2: dict[str, Any]) -> bool:
    if a1.get("regime", "ANUAL") == "ANUAL" or a2.get("regime", "ANUAL") == "ANUAL":
        return True
    s1 = 2 if (a1.get("semestre") == "2" or a1.get("semestre_alocado") == 2) else 1
    s2 = 2 if (a2.get("semestre") == "2" or a2.get("semestre_alocado") == 2) else 1
    return s1 == s2


def _format_turma_names(snap: ProjectSnapshot, lesson: dict[str, Any]) -> str:
    names = []
    for t_code in lesson.get("turmas", []):
        t = snap.turmas.get(t_code)
        names.append(t["nome"] if t else t_code)
    return ", ".join(names) if names else lesson.get("disciplina_nome", "")


def _turma_jornada_para_dia(turma: dict[str, Any], dia: str) -> list[dict[str, str]]:
    exc = turma.get("excecoes_por_dia") or {}
    if dia in exc and exc[dia].get("slots") is not None:
        return exc[dia]["slots"]
    return (turma.get("jornada_padrao") or {}).get("slots", [])


def _prof_disponivel_no_intervalo(prof: dict[str, Any], dia: str, start_min: int, end_min: int) -> bool:
    disp_dia = (prof.get("disponibilidade") or {}).get(dia, [])
    for inter in disp_dia:
        s = time_str_to_minutes(inter["inicio"])
        e = time_str_to_minutes(inter["fim"])
        if start_min >= s and end_min <= e:
            return True
    return False


def evaluate_lesson_placement(
    snap: ProjectSnapshot,
    target: dict[str, Any],
    ignore_aula_ids: set[str] | None = None,
) -> list[ConflictInfo]:
    result: list[ConflictInfo] = []
    ignores = ignore_aula_ids or set()
    t_dia = target["dia"]
    t_start = int(target["start_min"])
    t_end = int(target["end_min"])
    t_str = f"{minutes_to_time_str(t_start)} - {minutes_to_time_str(t_end)}"
    dia_long = DAY_LONG_NAMES.get(t_dia, t_dia)

    # 1. Turmas, jornada, intervalos, choques de turma, limite diario
    for t_id in target.get("turmas", []):
        t = snap.turmas.get(t_id)
        t_nome = t["nome"] if t else t_id
        if t is not None:
            if t_dia not in (t.get("dias_letivos") or []):
                result.append(
                    ConflictInfo(
                        type=SlotConflictType.TURMA_INATIVA,
                        short_label="Turma inativa",
                        detail=f"Turma inativa: A Turma '{t_nome}' não funciona na {dia_long}.",
                    )
                )
            slots = _turma_jornada_para_dia(t, t_dia)
            for b_s, b_e in compute_implicit_breaks(slots):
                if _overlaps(b_s, b_e, t_start, t_end):
                    result.append(
                        ConflictInfo(
                            type=SlotConflictType.INTERVALO_RECREIO,
                            short_label="Intervalo",
                            detail=(
                                f"Intervalo de recreio: Sobrepõe o recreio "
                                f"({minutes_to_time_str(b_s)} - {minutes_to_time_str(b_e)}) da Turma '{t_nome}'."
                            ),
                        )
                    )
            if not slots:
                result.append(
                    ConflictInfo(
                        type=SlotConflictType.FORA_JORNADA,
                        short_label="Fora da jornada",
                        detail=f"Fora da jornada: A Turma '{t_nome}' não possui slots cadastrados na {dia_long}.",
                    )
                )
            else:
                ordered = sorted(
                    [(time_str_to_minutes(s["inicio"]), time_str_to_minutes(s["fim"])) for s in slots]
                )
                j_start, j_end = ordered[0][0], ordered[-1][1]
                if t_start < j_start or t_end > j_end:
                    result.append(
                        ConflictInfo(
                            type=SlotConflictType.FORA_JORNADA,
                            short_label="Fora da jornada",
                            detail=(
                                f"Fora da jornada: O horário ultrapassa a jornada letiva da Turma '{t_nome}' "
                                f"({minutes_to_time_str(j_start)} - {minutes_to_time_str(j_end)})."
                            ),
                        )
                    )
                elif (t_start, t_end) not in ordered:
                    result.append(
                        ConflictInfo(
                            type=SlotConflictType.SLOT_INCOMPATIVEL,
                            short_label="Slot incompatível",
                            detail=(
                                f"Slot incompatível: O horário ({t_str}) não corresponde a um slot "
                                f"cadastrado na Turma '{t_nome}'."
                            ),
                        )
                    )

        for other in snap.alocacoes:
            if other["aula_id"] in ignores:
                continue
            if (
                other["dia"] == t_dia
                and _semesters_conflict(target, other)
                and _overlaps(t_start, t_end, int(other["start_min"]), int(other["end_min"]))
                and t_id in other.get("turmas", [])
            ):
                result.append(
                    ConflictInfo(
                        type=SlotConflictType.CHOQUE_TURMA,
                        short_label=f"Turma ocupada ({t_nome})",
                        detail=(
                            f"Choque de Turma: Conflita com a aula '{other['disciplina_nome']}' "
                            f"({other['aula_id']}) da Turma '{t_nome}' neste mesmo horário."
                        ),
                        related_aula_id=other["aula_id"],
                    )
                )

        if snap.config.get("distribuir_disciplinas_na_semana", True):
            disc = snap.disciplinas.get(target.get("disciplina_id", ""))
            disc_max = int(disc.get("max_aulas_por_dia_turma", 0)) if disc else 0
            cfg_max = int(snap.config.get("max_aulas_disciplina_dia_turma", 2))
            max_lim = disc_max if disc_max > 0 else cfg_max
            if max_lim > 0:
                other_count = 0
                for other in snap.alocacoes:
                    if other["aula_id"] in ignores:
                        continue
                    if (
                        other["dia"] == t_dia
                        and other.get("disciplina_id") == target.get("disciplina_id")
                        and _semesters_conflict(target, other)
                        and t_id in other.get("turmas", [])
                    ):
                        other_count += 1
                if other_count + 1 > max_lim:
                    result.append(
                        ConflictInfo(
                            type=SlotConflictType.LIMITE_DIARIO_DISCIPLINA,
                            short_label=f"Limite diário ({t_nome})",
                            detail=(
                                f"Limite diário excedido: {other_count + 1} aulas da disciplina alocadas hoje "
                                f"para a Turma '{t_nome}' (máximo permitido: {max_lim})."
                            ),
                        )
                    )

    # 2. Professores + choques
    for p_id in target.get("professores", []):
        p = snap.professores.get(p_id)
        p_nome = p["nome"] if p else p_id
        if p is not None and not _prof_disponivel_no_intervalo(p, t_dia, t_start, t_end):
            result.append(
                ConflictInfo(
                    type=SlotConflictType.INDISPONIBILIDADE_PROFESSOR,
                    short_label="Prof. indisponível",
                    detail=f"Indisponibilidade docente: O Prof. '{p_nome}' possui indisponibilidade cadastrada neste horário.",
                )
            )
        for other in snap.alocacoes:
            if other["aula_id"] in ignores:
                continue
            if (
                other["dia"] == t_dia
                and _semesters_conflict(target, other)
                and _overlaps(t_start, t_end, int(other["start_min"]), int(other["end_min"]))
                and p_id in other.get("professores", [])
            ):
                ctx = _format_turma_names(snap, other)
                result.append(
                    ConflictInfo(
                        type=SlotConflictType.CHOQUE_PROFESSOR,
                        short_label=f"Prof. ocupado ({ctx})",
                        detail=(
                            f"Choque de Professor: O Prof. '{p_nome}' já ministra a aula '{other['disciplina_nome']}' "
                            f"({ctx}, {other['aula_id']}) neste mesmo horário."
                        ),
                        related_aula_id=other["aula_id"],
                    )
                )

    # 3. Salas + choques
    for s_id in target.get("salas", []):
        if not s_id:
            continue
        s = snap.salas.get(s_id)
        s_nome = s["nome"] if s else s_id
        if target.get("tipo") == "LABORATORIO" and s is not None and s.get("tipo", "").upper() == "TEORICA":
            result.append(
                ConflictInfo(
                    type=SlotConflictType.INCOMPATIBILIDADE_SALA,
                    short_label="Sala incompatível (Lab)",
                    detail=f"Incompatibilidade de sala: A aula requer Laboratório, mas a Sala '{s_nome}' é Teórica.",
                )
            )
        for other in snap.alocacoes:
            if other["aula_id"] in ignores:
                continue
            if (
                other["dia"] == t_dia
                and _semesters_conflict(target, other)
                and _overlaps(t_start, t_end, int(other["start_min"]), int(other["end_min"]))
                and s_id in other.get("salas", [])
            ):
                ctx = _format_turma_names(snap, other)
                result.append(
                    ConflictInfo(
                        type=SlotConflictType.CHOQUE_SALA,
                        short_label=f"Sala ocupada ({ctx})",
                        detail=(
                            f"Choque de Sala: A Sala '{s_nome}' já está ocupada pela aula '{other['disciplina_nome']}' "
                            f"({ctx}, {other['aula_id']}) neste mesmo horário."
                        ),
                        related_aula_id=other["aula_id"],
                    )
                )

    return result


def verify_timetable_conflicts(snap: ProjectSnapshot) -> dict[str, list[ConflictInfo]]:
    conflitos: dict[str, list[ConflictInfo]] = {}
    for aloc in snap.alocacoes:
        items = evaluate_lesson_placement(snap, aloc, {aloc["aula_id"]})
        if items:
            conflitos[aloc["aula_id"]] = items
    return conflitos


def evaluate_lesson_at_slot(
    snap: ProjectSnapshot,
    aula_id: str,
    dia: str,
    start_min: int,
    end_min: int,
    ignore_aula_ids: list[str] | None = None,
    salas_override: list[str] | None = None,
    fallback_target: dict[str, Any] | None = None,
) -> list[ConflictInfo]:
    existing = next((a for a in snap.alocacoes if a["aula_id"] == aula_id), None)
    if existing is not None:
        target = dict(existing)
    elif fallback_target is not None:
        target = dict(fallback_target)
    else:
        return []

    target["dia"] = dia
    target["start_min"] = start_min
    target["end_min"] = end_min
    if salas_override:
        target["salas"] = list(salas_override)

    ignores = set(ignore_aula_ids or [])
    ignores.add(aula_id)
    return evaluate_lesson_placement(snap, target, ignores)


def evaluate_lesson_swap(snap: ProjectSnapshot, aula_a_id: str, aula_b_id: str) -> list[ConflictInfo]:
    if aula_a_id == aula_b_id:
        return []
    aloc_a = next((a for a in snap.alocacoes if a["aula_id"] == aula_a_id), None)
    aloc_b = next((a for a in snap.alocacoes if a["aula_id"] == aula_b_id), None)
    if aloc_a is None or aloc_b is None:
        return []

    conflitos: list[ConflictInfo] = []
    dur_a = int(aloc_a["end_min"]) - int(aloc_a["start_min"])
    dur_b = int(aloc_b["end_min"]) - int(aloc_b["start_min"])
    if dur_a != dur_b:
        conflitos.append(
            ConflictInfo(
                type=SlotConflictType.DURACAO_INCOMPATIVEL,
                short_label="Duração diferente",
                detail=(
                    f"Duração incompatível: '{aloc_a['disciplina_nome']}' ({dur_a} min) vs "
                    f"'{aloc_b['disciplina_nome']}' ({dur_b} min)."
                ),
                related_aula_id=aula_b_id,
            )
        )

    ignores = [aula_a_id, aula_b_id]
    conf_a = evaluate_lesson_at_slot(
        snap,
        aula_a_id,
        aloc_b["dia"],
        int(aloc_b["start_min"]),
        int(aloc_b["start_min"]) + dur_a,
        ignore_aula_ids=ignores,
    )
    conflitos.extend(conf_a)

    conf_b = evaluate_lesson_at_slot(
        snap,
        aula_b_id,
        aloc_a["dia"],
        int(aloc_a["start_min"]),
        int(aloc_a["start_min"]) + dur_b,
        ignore_aula_ids=ignores,
    )
    for c in conf_b:
        c.detail = f"[Na origem] {aloc_b['disciplina_nome']}: {c.detail}"
        conflitos.append(c)

    return conflitos
