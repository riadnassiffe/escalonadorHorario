from __future__ import annotations

import types
import fakeredis
import pytest
from httpx import AsyncClient

from app.workers import tasks as worker_tasks
from app.workers.tasks import execute_optimization_job_sync
from tests.conftest import register_and_login


@pytest.mark.asyncio
async def test_turma_per_day_schedules_and_export(client: AsyncClient):
    auth = await register_and_login(client, email="turma-days@inst.br")
    headers = {"Authorization": f"Bearer {auth['access_token']}"}

    resp_p = await client.post("/api/v1/projects", headers=headers, json={"name": "Projeto Horários"})
    assert resp_p.status_code == 201
    pid = resp_p.json()["id"]

    payload = {
        "code": "T01",
        "nome": "1º Ano A",
        "turno": "INTEGRAL",
        "dias_letivos": ["SEG", "TER", "QUA", "QUI"],
        "jornada_padrao": {
            "slots": [
                {"inicio": "07:30", "fim": "08:20"},
                {"inicio": "08:20", "fim": "09:10"},
            ]
        },
        "excecoes_por_dia": {
            "TER": {
                "slots": [
                    {"inicio": "07:30", "fim": "08:20"},
                    {"inicio": "13:30", "fim": "14:20"},
                ]
            }
        },
        "sala_teorica_id": None,
        "salas_teoricas_permitidas": [],
    }
    created = await client.post(f"/api/v1/projects/{pid}/turmas", headers=headers, json=payload)
    assert created.status_code == 201
    data = created.json()
    assert data["dias_letivos"] == ["SEG", "TER", "QUA", "QUI"]
    assert "TER" in data["excecoes_por_dia"]
    assert len(data["excecoes_por_dia"]["TER"]["slots"]) == 2

    # Update per-day exceptions
    payload["excecoes_por_dia"]["QUI"] = {"slots": [{"inicio": "08:00", "fim": "08:50"}]}
    updated = await client.put(f"/api/v1/projects/{pid}/turmas/T01", headers=headers, json=payload)
    assert updated.status_code == 200
    assert "QUI" in updated.json()["excecoes_por_dia"]

    listed = await client.get(f"/api/v1/projects/{pid}/turmas", headers=headers)
    assert listed.status_code == 200
    assert listed.json()[0]["excecoes_por_dia"]["QUI"]["slots"] == [{"inicio": "08:00", "fim": "08:50"}]

    exported = await client.get(f"/api/v1/projects/{pid}/export-json", headers=headers)
    assert exported.status_code == 200
    exp_turma = exported.json()["turmas"][0]
    assert exp_turma["excecoes_por_dia"]["TER"]["slots"][1]["inicio"] == "13:30"


@pytest.mark.asyncio
async def test_copy_turma_with_and_without_disciplinas_and_errors(client: AsyncClient):
    auth = await register_and_login(client, email="turma-copy@inst.br")
    headers = {"Authorization": f"Bearer {auth['access_token']}"}

    # Viewer user for RBAC check
    resp_v = await client.post(
        "/api/v1/auth/users",
        headers=headers,
        json={"full_name": "Viewer", "email": "viewer-copy@inst.br", "password": "secret123", "role": "VISUALIZADOR"},
    )
    assert resp_v.status_code == 201
    login_v = await client.post(
        "/api/v1/auth/login", json={"email": "viewer-copy@inst.br", "password": "secret123"}
    )
    headers_viewer = {"Authorization": f"Bearer {login_v.json()['access_token']}"}

    resp_p = await client.post(
        "/api/v1/projects",
        headers=headers,
        json={"name": "Projeto Cópia", "load_default_example": True},
    )
    assert resp_p.status_code == 201
    pid = resp_p.json()["id"]

    # Add custom day exception to T01 and a non-prefixed disciplina linked to T01
    turmas_resp = await client.get(f"/api/v1/projects/{pid}/turmas", headers=headers)
    t01 = next(t for t in turmas_resp.json() if t["code"] == "T01")
    t01["excecoes_por_dia"] = {"SEX": {"slots": [{"inicio": "08:00", "fim": "08:50"}]}}
    upd = await client.put(f"/api/v1/projects/{pid}/turmas/T01", headers=headers, json=t01)
    assert upd.status_code == 200

    extra_disc = await client.post(
        f"/api/v1/projects/{pid}/disciplinas",
        headers=headers,
        json={
            "code": "ARTES_1",
            "nome": "Artes",
            "tipo": "TEORICA",
            "duracao_minutos": 50,
            "aulas_semanais": 2,
            "regime": "ANUAL",
            "semestre": "AMBOS",
            "professores": ["P05"],
            "turmas": ["T01"],
            "salas_alternativas": [],
            "exigir_aulas_consecutivas": False,
            "max_aulas_por_dia_turma": 2,
        },
    )
    assert extra_disc.status_code == 201
    assert "turnos_permitidos" not in extra_disc.json()

    discs_before = (await client.get(f"/api/v1/projects/{pid}/disciplinas", headers=headers)).json()
    t01_discs_count = sum(1 for d in discs_before if "T01" in d["turmas"])
    assert t01_discs_count == 8

    # 1. Copy without disciplinas (auto code -> T04, auto room expands allowed theoretical rooms)
    copy_no_disc = await client.post(
        f"/api/v1/projects/{pid}/turmas/T01/copy",
        headers=headers,
        json={"nome": "1º Ano B - Ensino Médio", "copy_disciplinas": False},
    )
    assert copy_no_disc.status_code == 201, copy_no_disc.text
    res1 = copy_no_disc.json()
    assert res1["turma"]["code"] == "T04"
    assert res1["turma"]["nome"] == "1º Ano B - Ensino Médio"
    assert res1["turma"]["excecoes_por_dia"] == t01["excecoes_por_dia"]
    assert res1["turma"]["sala_teorica_id"] is None
    assert set(res1["turma"]["salas_teoricas_permitidas"]) == {"S101", "S102", "S201"}
    assert res1["copied_disciplinas_count"] == 0

    discs_after_no = (await client.get(f"/api/v1/projects/{pid}/disciplinas", headers=headers)).json()
    assert len(discs_after_no) == len(discs_before)

    # 2. Copy with disciplinas and explicit code T05
    copy_with_disc = await client.post(
        f"/api/v1/projects/{pid}/turmas/T01/copy",
        headers=headers,
        json={
            "code": "T05",
            "nome": "1º Ano C - Ensino Médio",
            "copy_disciplinas": True,
            "sala_teorica_id": "S201",
        },
    )
    assert copy_with_disc.status_code == 201, copy_with_disc.text
    res2 = copy_with_disc.json()
    assert res2["turma"]["code"] == "T05"
    assert res2["turma"]["sala_teorica_id"] == "S201"
    assert res2["turma"]["salas_teoricas_permitidas"] == ["S201"]
    assert res2["copied_disciplinas_count"] == t01_discs_count

    copied_codes = {d["code"] for d in res2["copied_disciplinas"]}
    assert "T05_MAT" in copied_codes
    assert "T05_QUI_EXP" in copied_codes
    assert "D01" in copied_codes  # ARTES_1 had no T01_ prefix

    mat_copy = next(d for d in res2["copied_disciplinas"] if d["code"] == "T05_MAT")
    assert mat_copy["turmas"] == ["T05"]
    assert mat_copy["professores"] == ["P01"]
    assert mat_copy["duracao_minutos"] == 45

    qui_copy = next(d for d in res2["copied_disciplinas"] if d["code"] == "T05_QUI_EXP")
    assert qui_copy["salas_alternativas"] == [["LAB01"], ["LAB02"]]

    # Verify original T01 and its disciplinas are intact
    discs_after_yes = (await client.get(f"/api/v1/projects/{pid}/disciplinas", headers=headers)).json()
    assert len(discs_after_yes) == len(discs_before) + t01_discs_count
    assert sum(1 for d in discs_after_yes if "T01" in d["turmas"]) == t01_discs_count

    # 3. Error cases: 404, 400 duplicate code, 403 viewer
    not_found = await client.post(
        f"/api/v1/projects/{pid}/turmas/T99/copy",
        headers=headers,
        json={"nome": "Inexistente"},
    )
    assert not_found.status_code == 404

    dup = await client.post(
        f"/api/v1/projects/{pid}/turmas/T01/copy",
        headers=headers,
        json={"code": "T05", "nome": "Duplicada"},
    )
    assert dup.status_code == 400

    forbid = await client.post(
        f"/api/v1/projects/{pid}/turmas/T01/copy",
        headers=headers_viewer,
        json={"nome": "Sem Permissão"},
    )
    assert forbid.status_code == 403


@pytest.mark.asyncio
async def test_disciplina_without_turnos_permitidos_legacy_import_and_afternoon_solve(
    client: AsyncClient, db_engines, monkeypatch: pytest.MonkeyPatch
):
    _, sync_sf = db_engines
    monkeypatch.setattr(
        worker_tasks.run_optimization_job,
        "delay",
        lambda job_id: types.SimpleNamespace(id=f"mock-task-{job_id}"),
    )

    auth = await register_and_login(client, email="tarde-solver@inst.br")
    headers = {"Authorization": f"Bearer {auth['access_token']}"}

    resp_p = await client.post(
        "/api/v1/projects",
        headers=headers,
        json={"name": "Projeto Tarde"},
    )
    assert resp_p.status_code == 201
    pid = resp_p.json()["id"]

    # Import legacy JSON containing "turnos_permitidos": ["MANHA"] on a TARDE turma
    legacy_payload = {
        "professores": [
            {
                "id": "P01",
                "nome": "Prof. Tarde",
                "preferencia_dias": {"SEG": 8, "TER": 8, "QUA": 8, "QUI": 8, "SEX": 8},
                "disponibilidade": {
                    "SEG": [{"inicio": "13:30", "fim": "18:00"}],
                    "TER": [{"inicio": "13:30", "fim": "18:00"}],
                },
                "turnos_pretendidos": {"SEG": ["TARDE"], "TER": ["TARDE"]},
                "carga_maxima_diaria_minutos": 0,
                "max_aulas_por_dia": 0,
                "peso_janela": 8,
            }
        ],
        "salas": [
            {"id": "S101", "nome": "Sala 101", "tipo": "TEORICA", "capacidade": 40, "recursos": []}
        ],
        "turmas": [
            {
                "id": "T01",
                "nome": "1º Ano Tarde",
                "turno": "TARDE",
                "dias_letivos": ["SEG", "TER"],
                "jornada_padrao": {
                    "slots": [
                        {"inicio": "13:30", "fim": "14:20"},
                        {"inicio": "14:20", "fim": "15:10"},
                    ]
                },
                "excecoes_por_dia": {},
                "sala_teorica_id": "S101",
                "salas_teoricas_permitidas": ["S101"],
            }
        ],
        "disciplinas": [
            {
                "id": "D01",
                "nome": "Matemática Tarde",
                "tipo": "TEORICA",
                "duracao_minutos": 50,
                "aulas_semanais": 2,
                "regime": "ANUAL",
                "semestre": "AMBOS",
                "professores": ["P01"],
                "turmas": ["T01"],
                "turnos_permitidos": ["MANHA"],
                "salas_alternativas": [],
                "exigir_aulas_consecutivas": False,
                "max_aulas_por_dia_turma": 2,
            }
        ],
        "config": {
            "algoritmo": "CP_SAT",
            "preenchimento_integral_jornada": False,
            "tempo_limite_segundos": 10.0,
        },
    }

    imp = await client.post(
        f"/api/v1/projects/{pid}/import-json",
        headers=headers,
        json={"data": legacy_payload},
    )
    assert imp.status_code == 200, imp.text

    discs = (await client.get(f"/api/v1/projects/{pid}/disciplinas", headers=headers)).json()
    assert len(discs) == 1
    assert "turnos_permitidos" not in discs[0]

    exported = (await client.get(f"/api/v1/projects/{pid}/export-json", headers=headers)).json()
    assert len(exported["disciplinas"]) == 1
    assert "turnos_permitidos" not in exported["disciplinas"][0]

    job_resp = await client.post(
        f"/api/v1/projects/{pid}/optimize",
        headers=headers,
        json={"algoritmo": "CP_SAT", "tempo_limite_segundos": 10.0},
    )
    assert job_resp.status_code == 202
    job_id = job_resp.json()["id"]

    fake_redis = fakeredis.FakeRedis(decode_responses=True)
    result = execute_optimization_job_sync(job_id, session_factory=sync_sf, redis_client=fake_redis)
    assert result["status"] == "COMPLETED"
    assert result["solver_status"] in ("OPTIMAL", "FEASIBLE")

    tt = (await client.get(f"/api/v1/projects/{pid}/timetable", headers=headers)).json()
    assert len(tt["alocacoes"]) == 2
    assert tt["conflitos"] == {}
    for aloc in tt["alocacoes"]:
        assert aloc["inicio"] in ("13:30", "14:20")


@pytest.mark.asyncio
async def test_copy_turma_with_selected_disciplinas_and_validation(client: AsyncClient):
    auth = await register_and_login(client, email="turma-select-copy@inst.br")
    headers = {"Authorization": f"Bearer {auth['access_token']}"}

    resp_p = await client.post(
        "/api/v1/projects",
        headers=headers,
        json={"name": "Projeto Cópia Seletiva", "load_default_example": True},
    )
    assert resp_p.status_code == 201
    pid = resp_p.json()["id"]

    # 1. Cópia seletiva de apenas 2 disciplinas de T01
    sel_resp = await client.post(
        f"/api/v1/projects/{pid}/turmas/T01/copy",
        headers=headers,
        json={
            "code": "T04",
            "nome": "1º Ano B - Seletivo",
            "copy_disciplinas": True,
            "disciplina_codes": ["T01_MAT", "T01_FIS"],
        },
    )
    assert sel_resp.status_code == 201, sel_resp.text
    sel_data = sel_resp.json()
    assert sel_data["turma"]["code"] == "T04"
    assert sel_data["copied_disciplinas_count"] == 2
    copied_codes = {d["code"] for d in sel_data["copied_disciplinas"]}
    assert copied_codes == {"T04_MAT", "T04_FIS"}
    for disc in sel_data["copied_disciplinas"]:
        assert disc["turmas"] == ["T04"]

    all_discs = (await client.get(f"/api/v1/projects/{pid}/disciplinas", headers=headers)).json()
    assert [d["code"] for d in all_discs if "T04" in d["turmas"]] == ["T04_FIS", "T04_MAT"]

    # 2. Cópia com disciplina_codes=[] cria apenas a turma
    empty_resp = await client.post(
        f"/api/v1/projects/{pid}/turmas/T01/copy",
        headers=headers,
        json={
            "code": "T05",
            "nome": "1º Ano C - Sem Disciplinas",
            "copy_disciplinas": True,
            "disciplina_codes": [],
        },
    )
    assert empty_resp.status_code == 201, empty_resp.text
    assert empty_resp.json()["copied_disciplinas_count"] == 0

    # 3. Cópia com disciplina de outra turma ou inexistente retorna 400 e não cria T06
    for bad_codes in (["T01_MAT", "T02_MAT"], ["NAO_EXISTE"]):
        bad_resp = await client.post(
            f"/api/v1/projects/{pid}/turmas/T01/copy",
            headers=headers,
            json={
                "code": "T06",
                "nome": "1º Ano D - Inválida",
                "copy_disciplinas": True,
                "disciplina_codes": bad_codes,
            },
        )
        assert bad_resp.status_code == 400

    turmas_after = (await client.get(f"/api/v1/projects/{pid}/turmas", headers=headers)).json()
    assert all(t["code"] != "T06" for t in turmas_after)
