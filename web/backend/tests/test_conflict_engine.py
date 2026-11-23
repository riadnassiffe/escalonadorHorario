from __future__ import annotations

import pytest
from httpx import AsyncClient

from app.services.conflict_engine import (
    ProjectSnapshot,
    SlotConflictType,
    evaluate_lesson_at_slot,
    evaluate_lesson_swap,
)
from tests.conftest import register_and_login


def _make_snapshot() -> ProjectSnapshot:
    slots = [
        {"inicio": "08:00", "fim": "08:50"},
        {"inicio": "08:50", "fim": "09:40"},
        {"inicio": "09:55", "fim": "10:40"},  # implicit break 09:40 - 09:55
    ]
    return ProjectSnapshot(
        professores={
            "P01": {
                "id": "P01",
                "nome": "Carlos",
                "disponibilidade": {"SEG": [{"inicio": "08:00", "fim": "11:00"}], "TER": []},
            },
            "P02": {
                "id": "P02",
                "nome": "Ana",
                "disponibilidade": {"SEG": [{"inicio": "08:00", "fim": "11:00"}]},
            },
        },
        salas={
            "S101": {"id": "S101", "nome": "Sala 101", "tipo": "TEORICA", "capacidade": 40},
            "LAB01": {"id": "LAB01", "nome": "Lab Quimica", "tipo": "LABORATORIO", "capacidade": 30},
        },
        turmas={
            "T01": {
                "id": "T01",
                "nome": "1A",
                "dias_letivos": ["SEG", "TER"],
                "jornada_padrao": {"slots": slots},
                "excecoes_por_dia": {},
            },
            "T02": {
                "id": "T02",
                "nome": "2A",
                "dias_letivos": ["SEG"],
                "jornada_padrao": {"slots": slots},
                "excecoes_por_dia": {},
            },
        },
        disciplinas={
            "D_MAT": {"id": "D_MAT", "nome": "Matemática", "max_aulas_por_dia_turma": 1},
            "D_QUI": {"id": "D_QUI", "nome": "Química Lab", "max_aulas_por_dia_turma": 2},
        },
        config={"distribuir_disciplinas_na_semana": True, "max_aulas_disciplina_dia_turma": 2},
        alocacoes=[
            {
                "aula_id": "D_MAT_1",
                "disciplina_id": "D_MAT",
                "disciplina_nome": "Matemática",
                "tipo": "TEORICA",
                "dia": "SEG",
                "start_min": 480,  # 08:00
                "end_min": 530,    # 08:50
                "professores": ["P01"],
                "turmas": ["T01"],
                "salas": ["S101"],
                "is_pinned": False,
                "regime": "ANUAL",
                "semestre": "AMBOS",
                "semestre_alocado": 1,
            },
            {
                "aula_id": "D_QUI_1",
                "disciplina_id": "D_QUI",
                "disciplina_nome": "Química Lab",
                "tipo": "LABORATORIO",
                "dia": "SEG",
                "start_min": 530,  # 08:50
                "end_min": 580,    # 09:40
                "professores": ["P02"],
                "turmas": ["T02"],
                "salas": ["LAB01"],
                "is_pinned": False,
                "regime": "SEMESTRAL",
                "semestre": "1",
                "semestre_alocado": 1,
            },
        ],
    )


def test_all_11_conflict_types_and_semester_compatibility():
    snap = _make_snapshot()

    # 1. TURMA_INATIVA + INDISPONIBILIDADE_PROFESSOR (T02 inactive on QUA)
    c_inativa = evaluate_lesson_at_slot(snap, "D_QUI_1", "QUA", 480, 530)
    types_inativa = {c.type for c in c_inativa}
    assert SlotConflictType.TURMA_INATIVA in types_inativa
    assert SlotConflictType.INDISPONIBILIDADE_PROFESSOR in types_inativa

    # 2. INTERVALO_RECREIO + SLOT_INCOMPATIVEL (09:40 - 10:30 overlaps 09:40-09:55 break)
    c_brk = evaluate_lesson_at_slot(snap, "D_MAT_1", "SEG", 580, 630)
    types_brk = {c.type for c in c_brk}
    assert SlotConflictType.INTERVALO_RECREIO in types_brk

    # 3. FORA_JORNADA (07:00 - 07:50)
    c_fora = evaluate_lesson_at_slot(snap, "D_MAT_1", "SEG", 420, 470)
    assert SlotConflictType.FORA_JORNADA in {c.type for c in c_fora}

    # 4. SLOT_INCOMPATIVEL (08:00 - 08:45 inside jornada, not exact slot)
    c_slot = evaluate_lesson_at_slot(snap, "D_MAT_1", "SEG", 480, 525)
    assert SlotConflictType.SLOT_INCOMPATIVEL in {c.type for c in c_slot}

    # 5. INCOMPATIBILIDADE_SALA (LAB lesson placed in TEORICA S101)
    c_lab = evaluate_lesson_at_slot(snap, "D_QUI_1", "SEG", 530, 580, salas_override=["S101"])
    assert SlotConflictType.INCOMPATIBILIDADE_SALA in {c.type for c in c_lab}

    # 6. LIMITE_DIARIO_DISCIPLINA (D_MAT_2 on SEG where D_MAT_1 already exists and max=1)
    snap.alocacoes.append({
        **snap.alocacoes[0],
        "aula_id": "D_MAT_2",
        "dia": "TER",
    })
    c_lim = evaluate_lesson_at_slot(snap, "D_MAT_2", "SEG", 530, 580)
    assert SlotConflictType.LIMITE_DIARIO_DISCIPLINA in {c.type for c in c_lim}

    # 7. CHOQUE_TURMA + CHOQUE_PROFESSOR + CHOQUE_SALA
    c_clash = evaluate_lesson_at_slot(snap, "D_MAT_2", "SEG", 480, 530)
    clash_types = {c.type for c in c_clash}
    assert SlotConflictType.CHOQUE_TURMA in clash_types
    assert SlotConflictType.CHOQUE_PROFESSOR in clash_types
    assert SlotConflictType.CHOQUE_SALA in clash_types

    # 8. DURACAO_INCOMPATIVEL on swap (45 min vs 50 min)
    snap.alocacoes[1]["start_min"] = 595
    snap.alocacoes[1]["end_min"] = 640  # 45 min
    c_swap = evaluate_lesson_swap(snap, "D_MAT_1", "D_QUI_1")
    assert SlotConflictType.DURACAO_INCOMPATIVEL in {c.type for c in c_swap}

    # 9. Semester 1 vs Semester 2 should NOT clash on professor/room/turma
    snap.alocacoes.append({
        "aula_id": "D_SEM2_1",
        "disciplina_id": "D_QUI",
        "disciplina_nome": "Química Sem2",
        "tipo": "LABORATORIO",
        "dia": "SEG",
        "start_min": 595,
        "end_min": 640,
        "professores": ["P02"],
        "turmas": ["T02"],
        "salas": ["LAB01"],
        "is_pinned": False,
        "regime": "SEMESTRAL",
        "semestre": "2",
        "semestre_alocado": 2,
    })
    c_sem = evaluate_lesson_at_slot(snap, "D_SEM2_1", "SEG", 595, 640)
    assert not any(
        c.type in (SlotConflictType.CHOQUE_TURMA, SlotConflictType.CHOQUE_PROFESSOR, SlotConflictType.CHOQUE_SALA)
        for c in c_sem
    )


@pytest.mark.asyncio
async def test_timetable_mutations_api(client: AsyncClient):
    auth = await register_and_login(client, email="mutations@inst.br")
    headers = {"Authorization": f"Bearer {auth['access_token']}"}
    p = (await client.post("/api/v1/projects", headers=headers, json={"name": "Mut", "load_default_example": True})).json()
    pid = p["id"]

    # Manual allocate two lessons
    r1 = await client.post(
        f"/api/v1/projects/{pid}/timetable/allocate-manual",
        headers=headers,
        json={
            "aula_id": "T01_PORT_1",
            "disciplina_id": "T01_PORT",
            "dia": "SEG",
            "inicio": "08:00",
            "fim": "08:50",
            "salas": ["S101"],
            "is_pinned": False,
        },
    )
    assert r1.status_code == 200
    r2 = await client.post(
        f"/api/v1/projects/{pid}/timetable/allocate-manual",
        headers=headers,
        json={
            "aula_id": "T01_FIS_1",
            "disciplina_id": "T01_FIS",
            "dia": "TER",
            "inicio": "08:50",
            "fim": "09:40",
            "salas": ["S101"],
            "is_pinned": False,
        },
    )
    assert r2.status_code == 200

    # Swap
    sw = await client.post(
        f"/api/v1/projects/{pid}/timetable/swap",
        headers=headers,
        json={"aula_a_id": "T01_PORT_1", "aula_b_id": "T01_FIS_1", "pin_both": True},
    )
    assert sw.status_code == 200

    tt = (await client.get(f"/api/v1/projects/{pid}/timetable", headers=headers)).json()
    by_id = {a["aula_id"]: a for a in tt["alocacoes"]}
    assert by_id["T01_PORT_1"]["dia"] == "TER"
    assert by_id["T01_PORT_1"]["is_pinned"] is True
    assert by_id["T01_FIS_1"]["dia"] == "SEG"
