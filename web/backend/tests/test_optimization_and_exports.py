from __future__ import annotations

import json
import types
import fakeredis
import pytest
from httpx import AsyncClient

from app.workers import tasks as worker_tasks
from app.workers.tasks import execute_optimization_job_sync
from tests.conftest import register_and_login


@pytest.mark.asyncio
async def test_celery_solver_cli_integration_and_exports(
    client: AsyncClient, db_engines, monkeypatch: pytest.MonkeyPatch
):
    _, sync_sf = db_engines
    monkeypatch.setattr(
        worker_tasks.run_optimization_job,
        "delay",
        lambda job_id: types.SimpleNamespace(id=f"mock-task-{job_id}"),
    )

    auth = await register_and_login(client, email="solver@inst.br")
    headers = {"Authorization": f"Bearer {auth['access_token']}"}

    proj = (
        await client.post(
            "/api/v1/projects",
            headers=headers,
            json={"name": "Solver E2E", "load_default_example": True},
        )
    ).json()
    pid = proj["id"]

    job_resp = await client.post(
        f"/api/v1/projects/{pid}/optimize",
        headers=headers,
        json={"algoritmo": "CP_SAT", "tempo_limite_segundos": 10.0},
    )
    assert job_resp.status_code == 202
    job_id = job_resp.json()["id"]

    fake_redis = fakeredis.FakeRedis(decode_responses=True)
    pubsub = fake_redis.pubsub()
    pubsub.subscribe(f"job:{job_id}:events")
    # Consume initial subscribe confirmation message
    pubsub.get_message(timeout=0.05)

    result = execute_optimization_job_sync(job_id, session_factory=sync_sf, redis_client=fake_redis)
    assert result["status"] == "COMPLETED"
    assert result["solver_status"] in ("OPTIMAL", "FEASIBLE")

    events = []
    for _ in range(100):
        msg = pubsub.get_message(ignore_subscribe_messages=True, timeout=0.01)
        if msg is None:
            break
        events.append(json.loads(msg["data"]))
    assert any(e.get("type") == "progress" for e in events)
    assert any(e.get("type") == "done" and e.get("status") == "COMPLETED" for e in events)

    tt = (await client.get(f"/api/v1/projects/{pid}/timetable", headers=headers)).json()
    assert len(tt["alocacoes"]) > 0
    assert tt["conflitos"] == {}

    csv_resp = await client.get(f"/api/v1/projects/{pid}/export/csv", headers=headers)
    assert csv_resp.status_code == 200
    lines = csv_resp.text.strip().splitlines()
    assert len(lines) == len(tt["alocacoes"]) + 1
    assert len(lines[0].split(";")) == 13

    html_resp = await client.get(f"/api/v1/projects/{pid}/export/html", headers=headers)
    assert html_resp.status_code == 200
    assert '<table class="timetable">' in html_resp.text


@pytest.mark.asyncio
async def test_teorica_disciplina_with_explicit_lab_rooms_is_respected_by_solvers(
    client: AsyncClient, db_engines, monkeypatch: pytest.MonkeyPatch
):
    _, sync_sf = db_engines
    monkeypatch.setattr(
        worker_tasks.run_optimization_job,
        "delay",
        lambda job_id: types.SimpleNamespace(id=f"mock-task-{job_id}"),
    )

    auth = await register_and_login(client, email="explicit_rooms@inst.br")
    headers = {"Authorization": f"Bearer {auth['access_token']}"}

    proj = (
        await client.post(
            "/api/v1/projects",
            headers=headers,
            json={"name": "Explicit Lab Rooms", "load_default_example": True},
        )
    ).json()
    pid = proj["id"]

    discs = (await client.get(f"/api/v1/projects/{pid}/disciplinas", headers=headers)).json()
    fis = next(d for d in discs if d["code"] == "T01_FIS")
    assert fis["tipo"] == "TEORICA"

    upd = await client.put(
        f"/api/v1/projects/{pid}/disciplinas/T01_FIS",
        headers=headers,
        json={**fis, "salas_alternativas": [["LAB_INF"], ["  ", ""]]},
    )
    assert upd.status_code == 200
    assert upd.json()["salas_alternativas"] == [["LAB_INF"]]

    fake_redis = fakeredis.FakeRedis(decode_responses=True)

    for algo in ("CP_SAT", "HEURISTICA_GULOSA"):
        job_resp = await client.post(
            f"/api/v1/projects/{pid}/optimize",
            headers=headers,
            json={"algoritmo": algo, "tempo_limite_segundos": 10.0},
        )
        assert job_resp.status_code == 202
        job_id = job_resp.json()["id"]

        result = execute_optimization_job_sync(job_id, session_factory=sync_sf, redis_client=fake_redis)
        assert result["status"] == "COMPLETED"
        assert result["solver_status"] in ("OPTIMAL", "FEASIBLE")

        tt = (await client.get(f"/api/v1/projects/{pid}/timetable", headers=headers)).json()
        assert tt["conflitos"] == {}

        fis_allocs = [a for a in tt["alocacoes"] if a["disciplina_id"] == "T01_FIS"]
        mat_allocs = [a for a in tt["alocacoes"] if a["disciplina_id"] == "T01_MAT"]
        assert len(fis_allocs) == fis["aulas_semanais"]
        assert all(a["salas"] == ["LAB_INF"] for a in fis_allocs)
        assert len(mat_allocs) > 0
        assert all(a["salas"] == ["S101"] for a in mat_allocs)


@pytest.mark.asyncio
async def test_pinned_lessons_are_preserved_across_solvers(
    client: AsyncClient, db_engines, monkeypatch: pytest.MonkeyPatch
):
    _, sync_sf = db_engines
    monkeypatch.setattr(
        worker_tasks.run_optimization_job,
        "delay",
        lambda job_id: types.SimpleNamespace(id=f"mock-task-{job_id}"),
    )

    auth = await register_and_login(client, email="pinned_keep@inst.br")
    headers = {"Authorization": f"Bearer {auth['access_token']}"}

    proj = (
        await client.post(
            "/api/v1/projects",
            headers=headers,
            json={"name": "Pinned Preservation", "load_default_example": True},
        )
    ).json()
    pid = proj["id"]

    fake_redis = fakeredis.FakeRedis(decode_responses=True)

    # 1. Generate initial timetable
    init_job = (
        await client.post(
            f"/api/v1/projects/{pid}/optimize",
            headers=headers,
            json={"algoritmo": "CP_SAT", "tempo_limite_segundos": 15.0},
        )
    ).json()
    init_res = execute_optimization_job_sync(init_job["id"], session_factory=sync_sf, redis_client=fake_redis)
    assert init_res["status"] == "COMPLETED"

    tt_init = (await client.get(f"/api/v1/projects/{pid}/timetable", headers=headers)).json()
    t01_allocs = [a for a in tt_init["alocacoes"] if "T01" in a["turmas"]]
    assert len(t01_allocs) > 0

    # Pin all lessons of turma T01
    expected_pinned: dict[str, tuple[str, str, str, tuple[str, ...]]] = {}
    for aloc in t01_allocs:
        pin_resp = await client.patch(
            f"/api/v1/projects/{pid}/timetable/lessons/{aloc['aula_id']}/pin",
            headers=headers,
            json={"pinned": True},
        )
        assert pin_resp.status_code == 200
        expected_pinned[aloc["aula_id"]] = (
            aloc["dia"],
            aloc["inicio"],
            aloc["fim"],
            tuple(aloc["salas"]),
        )

    # Re-optimize with warm start disabled across multiple solvers: pinned lessons must not change
    for algo in ("CP_SAT", "HIGHS", "SCIP", "GREEDY_HEURISTIC"):
        job_resp = await client.post(
            f"/api/v1/projects/{pid}/optimize",
            headers=headers,
            json={
                "algoritmo": algo,
                "tempo_limite_segundos": 20.0,
                "usar_distribuicao_como_entrada": False,
            },
        )
        assert job_resp.status_code == 202
        res = execute_optimization_job_sync(job_resp.json()["id"], session_factory=sync_sf, redis_client=fake_redis)
        assert res["status"] == "COMPLETED", f"Solver {algo} failed: {res}"

        tt = (await client.get(f"/api/v1/projects/{pid}/timetable", headers=headers)).json()
        assert tt["conflitos"] == {}, f"Conflicts found for {algo}: {tt['conflitos']}"

        by_id = {a["aula_id"]: a for a in tt["alocacoes"]}
        for aula_id, expected in expected_pinned.items():
            assert aula_id in by_id, f"{aula_id} missing after {algo}"
            actual = by_id[aula_id]
            assert actual["is_pinned"] is True, f"{aula_id} lost is_pinned after {algo}"
            assert (actual["dia"], actual["inicio"], actual["fim"], tuple(actual["salas"])) == expected, (
                f"{aula_id} altered by {algo}: got {(actual['dia'], actual['inicio'], actual['fim'], tuple(actual['salas']))}, expected {expected}"
            )


@pytest.mark.asyncio
async def test_pinned_turma_multiple_theoretical_rooms_and_no_r9_first_slot(
    client: AsyncClient, db_engines, monkeypatch: pytest.MonkeyPatch
):
    _, sync_sf = db_engines
    monkeypatch.setattr(
        worker_tasks.run_optimization_job,
        "delay",
        lambda job_id: types.SimpleNamespace(id=f"mock-task-{job_id}"),
    )

    auth = await register_and_login(client, email="pinned_r5_r9@inst.br")
    headers = {"Authorization": f"Bearer {auth['access_token']}"}

    proj = (
        await client.post(
            "/api/v1/projects",
            headers=headers,
            json={"name": "Pinned Multiple Rooms No R9", "load_default_example": True},
        )
    ).json()
    pid = proj["id"]

    # Add an extra theoretical room S202
    room_resp = await client.post(
        f"/api/v1/projects/{pid}/salas",
        headers=headers,
        json={"code": "S202", "nome": "Sala Teórica 202", "tipo": "TEORICA", "capacidade": 45, "recursos": []},
    )
    assert room_resp.status_code in (200, 201)

    # Update T03: allow S201 and S202, and on SEG set 1st slot to 35 min (so no 45/50m lesson can start at 1st slot)
    turmas = (await client.get(f"/api/v1/projects/{pid}/turmas", headers=headers)).json()
    t03 = next(t for t in turmas if t["code"] == "T03")
    upd_turma = await client.put(
        f"/api/v1/projects/{pid}/turmas/T03",
        headers=headers,
        json={
            **t03,
            "sala_teorica_id": None,
            "salas_teoricas_permitidas": ["S201", "S202"],
            "excecoes_por_dia": {
                **t03.get("excecoes_por_dia", {}),
                "SEG": {
                    "slots": [
                        {"inicio": "08:00", "fim": "08:35"},
                        {"inicio": "08:50", "fim": "09:40"},
                        {"inicio": "09:55", "fim": "10:40"},
                        {"inicio": "10:40", "fim": "11:25"},
                        {"inicio": "11:40", "fim": "12:30"},
                    ]
                },
            },
        },
    )
    assert upd_turma.status_code == 200

    # Pin two T03 theoretical lessons on SEG after the 1st slot, in two distinct theoretical rooms (S201 and S202)
    pinned_specs = [
        {
            "aula_id": "T03_MAT_1",
            "disciplina_id": "T03_MAT",
            "dia": "SEG",
            "inicio": "08:50",
            "fim": "09:40",
            "salas": ["S201"],
            "semestre_alocado": 3,
            "is_pinned": True,
        },
        {
            "aula_id": "T03_FIS_1",
            "disciplina_id": "T03_FIS",
            "dia": "SEG",
            "inicio": "11:40",
            "fim": "12:30",
            "salas": ["S202"],
            "semestre_alocado": 3,
            "is_pinned": True,
        },
    ]
    for spec in pinned_specs:
        alloc_resp = await client.post(
            f"/api/v1/projects/{pid}/timetable/allocate-manual",
            headers=headers,
            json=spec,
        )
        assert alloc_resp.status_code == 200

    fake_redis = fakeredis.FakeRedis(decode_responses=True)

    for algo in ("CP_SAT", "HIGHS", "GREEDY_HEURISTIC"):
        job_resp = await client.post(
            f"/api/v1/projects/{pid}/optimize",
            headers=headers,
            json={
                "algoritmo": algo,
                "tempo_limite_segundos": 20.0,
                "usar_distribuicao_como_entrada": False,
            },
        )
        assert job_resp.status_code == 202
        res = execute_optimization_job_sync(job_resp.json()["id"], session_factory=sync_sf, redis_client=fake_redis)
        assert res["status"] == "COMPLETED", f"Solver {algo} failed: {res}"

        tt = (await client.get(f"/api/v1/projects/{pid}/timetable", headers=headers)).json()
        assert tt["conflitos"] == {}, f"Conflicts found for {algo}: {tt['conflitos']}"

        by_id = {a["aula_id"]: a for a in tt["alocacoes"]}
        for spec in pinned_specs:
            actual = by_id[spec["aula_id"]]
            assert actual["is_pinned"] is True
            assert actual["dia"] == spec["dia"]
            assert actual["inicio"] == spec["inicio"]
            assert actual["fim"] == spec["fim"]
            assert actual["salas"] == spec["salas"]
