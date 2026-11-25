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
