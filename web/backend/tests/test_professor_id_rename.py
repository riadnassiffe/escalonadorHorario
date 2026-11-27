from __future__ import annotations

import pytest
from httpx import AsyncClient

from tests.conftest import register_and_login


@pytest.mark.asyncio
async def test_rename_professor_id_propagates_across_disciplinas_timetables_exports_and_portal(
    client: AsyncClient,
):
    auth = await register_and_login(
        client,
        institution_name="Colégio Rename",
        full_name="Admin Rename",
        email="admin-rename@colegio.edu.br",
        password="adminpassword",
    )
    headers = {"Authorization": f"Bearer {auth['access_token']}"}

    resp_p = await client.post("/api/v1/projects", headers=headers, json={"name": "Grade Rename 2026"})
    assert resp_p.status_code == 201
    pid = resp_p.json()["id"]

    # Criar professores P01 (sem e-mail, gera login .internal) e P02
    disp = {
        d: [{"inicio": "07:30", "fim": "12:00"}]
        for d in ["SEG", "TER", "QUA", "QUI", "SEX"]
    }
    resp_p01 = await client.post(
        f"/api/v1/projects/{pid}/professores",
        headers=headers,
        json={"code": "P01", "nome": "Prof. Carlos Silva", "disponibilidade": disp},
    )
    assert resp_p01.status_code == 201, resp_p01.text
    p01_created = resp_p01.json()
    p01_email = p01_created["email"]
    p01_pw = p01_created["initial_password"]
    assert p01_email and p01_pw

    resp_p02 = await client.post(
        f"/api/v1/projects/{pid}/professores",
        headers=headers,
        json={
            "code": "P02",
            "nome": "Profa. Ana Souza",
            "email": "ana@colegio.edu.br",
            "disponibilidade": disp,
        },
    )
    assert resp_p02.status_code == 201, resp_p02.text

    # Login do professor P01 no portal
    login_p01 = await client.post("/api/v1/auth/login", json={"email": p01_email, "password": p01_pw})
    assert login_p01.status_code == 200
    headers_p01 = {"Authorization": f"Bearer {login_p01.json()['access_token']}"}

    # Criar sala, turma e disciplinas (individual + compartilhada)
    assert (
        await client.post(
            f"/api/v1/projects/{pid}/salas",
            headers=headers,
            json={"code": "S101", "nome": "Sala 101"},
        )
    ).status_code == 201
    assert (
        await client.post(
            f"/api/v1/projects/{pid}/turmas",
            headers=headers,
            json={
                "code": "T01",
                "nome": "1º Ano A",
                "turno": "MANHA",
                "dias_letivos": ["SEG", "TER", "QUA", "QUI", "SEX"],
                "jornada_padrao": {
                    "slots": [
                        {"inicio": "07:30", "fim": "08:20"},
                        {"inicio": "08:20", "fim": "09:10"},
                    ]
                },
                "sala_teorica_id": "S101",
                "salas_teoricas_permitidas": ["S101"],
            },
        )
    ).status_code == 201

    assert (
        await client.post(
            f"/api/v1/projects/{pid}/disciplinas",
            headers=headers,
            json={
                "code": "MAT",
                "nome": "Matemática",
                "professores": ["P01"],
                "turmas": ["T01"],
            },
        )
    ).status_code == 201
    assert (
        await client.post(
            f"/api/v1/projects/{pid}/disciplinas",
            headers=headers,
            json={
                "code": "PROJ",
                "nome": "Projeto Integrador",
                "professores": ["P01", "P02"],
                "turmas": ["T01"],
            },
        )
    ).status_code == 201

    # Importar JSON duas vezes para gerar versão histórica (v1) e versão ativa (v2)
    exported_base = (await client.get(f"/api/v1/projects/{pid}/export-json", headers=headers)).json()
    exported_base["grade"] = {
        "alocacoes": [
            {
                "aula_id": "MAT_1",
                "disciplina_id": "MAT",
                "disciplina_nome": "Matemática",
                "tipo": "TEORICA",
                "dia": "SEG",
                "inicio": "07:30",
                "fim": "08:20",
                "professores": ["P01"],
                "turmas": ["T01"],
                "salas": ["S101"],
                "is_pinned": False,
                "regime": "ANUAL",
                "semestre": "AMBOS",
                "semestre_alocado": 1,
            }
        ],
        "salas_teoricas_designadas": {"T01": "S101"},
        "estatisticas": {"status": "OPTIMAL"},
    }
    imp_v1 = await client.post(
        f"/api/v1/projects/{pid}/import-json", headers=headers, json={"data": exported_base}
    )
    assert imp_v1.status_code == 200, imp_v1.text
    v1_id = imp_v1.json()["version_id"]
    assert v1_id

    exported_v2 = (await client.get(f"/api/v1/projects/{pid}/export-json", headers=headers)).json()
    exported_v2["grade"] = {
        "alocacoes": [
            {
                "aula_id": "MAT_1",
                "disciplina_id": "MAT",
                "disciplina_nome": "Matemática",
                "tipo": "TEORICA",
                "dia": "SEG",
                "inicio": "07:30",
                "fim": "08:20",
                "professores": ["P01"],
                "turmas": ["T01"],
                "salas": ["S101"],
                "is_pinned": False,
                "regime": "ANUAL",
                "semestre": "AMBOS",
                "semestre_alocado": 1,
            },
            {
                "aula_id": "PROJ_1",
                "disciplina_id": "PROJ",
                "disciplina_nome": "Projeto Integrador",
                "tipo": "TEORICA",
                "dia": "SEG",
                "inicio": "08:20",
                "fim": "09:10",
                "professores": ["P01", "P02"],
                "turmas": ["T01"],
                "salas": ["S101"],
                "is_pinned": True,
                "regime": "ANUAL",
                "semestre": "AMBOS",
                "semestre_alocado": 1,
            },
        ],
        "salas_teoricas_designadas": {"T01": "S101"},
        "estatisticas": {"status": "OPTIMAL"},
    }
    imp_v2 = await client.post(
        f"/api/v1/projects/{pid}/import-json", headers=headers, json={"data": exported_v2}
    )
    assert imp_v2.status_code == 200, imp_v2.text
    v2_id = imp_v2.json()["version_id"]
    assert v2_id != v1_id

    # Renomear P01 -> "  PROF_CARLOS  " (testando strip + propagação completa)
    p01_current = next(
        p
        for p in (await client.get(f"/api/v1/projects/{pid}/professores", headers=headers)).json()
        if p["code"] == "P01"
    )
    rename_payload = {**p01_current, "code": "  PROF_CARLOS  "}
    resp_upd = await client.put(
        f"/api/v1/projects/{pid}/professores/P01", headers=headers, json=rename_payload
    )
    assert resp_upd.status_code == 200, resp_upd.text
    upd_data = resp_upd.json()
    assert upd_data["id"] == "PROF_CARLOS"
    assert upd_data["code"] == "PROF_CARLOS"
    assert upd_data["email"] == p01_email

    # Listagem de professores atualizada
    profs_after = (await client.get(f"/api/v1/projects/{pid}/professores", headers=headers)).json()
    codes_after = [p["code"] for p in profs_after]
    assert "PROF_CARLOS" in codes_after
    assert "P01" not in codes_after

    # Disciplinas propagadas (individual e compartilhada sem duplicatas)
    discs_after = (await client.get(f"/api/v1/projects/{pid}/disciplinas", headers=headers)).json()
    discs_by_code = {d["code"]: d for d in discs_after}
    assert discs_by_code["MAT"]["professores"] == ["PROF_CARLOS"]
    assert discs_by_code["PROJ"]["professores"] == ["PROF_CARLOS", "P02"]

    # Grade ativa (v2) e grade histórica (v1) atualizadas e sem conflitos
    tt_v2 = (await client.get(f"/api/v1/projects/{pid}/timetable", headers=headers)).json()
    assert tt_v2["conflitos"] == {}
    v2_by_aula = {a["aula_id"]: a for a in tt_v2["alocacoes"]}
    assert v2_by_aula["MAT_1"]["professores"] == ["PROF_CARLOS"]
    assert v2_by_aula["PROJ_1"]["professores"] == ["PROF_CARLOS", "P02"]

    tt_v1 = (
        await client.get(f"/api/v1/projects/{pid}/timetable?version_id={v1_id}", headers=headers)
    ).json()
    assert tt_v1["alocacoes"][0]["professores"] == ["PROF_CARLOS"]

    # Exportações JSON, CSV e HTML refletem novo ID
    exp_json = (await client.get(f"/api/v1/projects/{pid}/export-json", headers=headers)).json()
    assert any(p["id"] == "PROF_CARLOS" for p in exp_json["professores"])
    assert all(p["id"] != "P01" for p in exp_json["professores"])

    exp_csv = await client.get(f"/api/v1/projects/{pid}/export/csv", headers=headers)
    assert exp_csv.status_code == 200
    assert "Prof. Carlos Silva" in exp_csv.text

    exp_html = await client.get(f"/api/v1/projects/{pid}/export/html", headers=headers)
    assert exp_html.status_code == 200
    assert "PROF_CARLOS" in exp_html.text

    # Portal do professor continua funcionando com mesmo login e exibe novo code + aulas
    resp_profiles = await client.get("/api/v1/professor/me/profiles", headers=headers_p01)
    assert resp_profiles.status_code == 200
    profiles = resp_profiles.json()
    assert len(profiles) == 1
    assert profiles[0]["code"] == "PROF_CARLOS"
    prof_uuid = profiles[0]["professor_id"]

    resp_prof_tt = await client.get(
        f"/api/v1/professor/me/profiles/{prof_uuid}/timetable", headers=headers_p01
    )
    assert resp_prof_tt.status_code == 200
    prof_tt = resp_prof_tt.json()
    assert prof_tt["professor_code"] == "PROF_CARLOS"
    assert [a["aula_id"] for a in prof_tt["alocacoes"]] == ["MAT_1", "PROJ_1"]


@pytest.mark.asyncio
async def test_rename_professor_id_validation_and_edge_cases(client: AsyncClient):
    auth = await register_and_login(client, email="admin-val@colegio.edu.br")
    headers = {"Authorization": f"Bearer {auth['access_token']}"}

    pid1 = (
        await client.post("/api/v1/projects", headers=headers, json={"name": "Proj 1"})
    ).json()["id"]
    pid2 = (
        await client.post("/api/v1/projects", headers=headers, json={"name": "Proj 2"})
    ).json()["id"]

    await client.post(
        f"/api/v1/projects/{pid1}/professores",
        headers=headers,
        json={"code": "P01", "nome": "Prof 1"},
    )
    await client.post(
        f"/api/v1/projects/{pid1}/professores",
        headers=headers,
        json={"code": "P02", "nome": "Prof 2"},
    )
    await client.post(
        f"/api/v1/projects/{pid2}/professores",
        headers=headers,
        json={"code": "P99", "nome": "Prof Outro Proj"},
    )

    # 1. Duplicado no mesmo projeto -> 400 e não altera P01
    r_dup = await client.put(
        f"/api/v1/projects/{pid1}/professores/P01",
        headers=headers,
        json={"code": "P02", "nome": "Prof 1"},
    )
    assert r_dup.status_code == 400
    assert "já existe" in r_dup.json()["detail"]

    # 2. Vazio / apenas espaços -> 400
    r_empty = await client.put(
        f"/api/v1/projects/{pid1}/professores/P01",
        headers=headers,
        json={"code": "   ", "nome": "Prof 1"},
    )
    assert r_empty.status_code == 400

    # 3. Caracteres '/' ou '\\' -> 400
    for bad in ("P/01", "P\\01"):
        r_slash = await client.put(
            f"/api/v1/projects/{pid1}/professores/P01",
            headers=headers,
            json={"code": bad, "nome": "Prof 1"},
        )
        assert r_slash.status_code == 400

    # 4. Mais de 64 caracteres -> 400
    r_long = await client.put(
        f"/api/v1/projects/{pid1}/professores/P01",
        headers=headers,
        json={"code": "P" * 65, "nome": "Prof 1"},
    )
    assert r_long.status_code == 400

    # 5. Omitir code ou enviar code=None mantém code atual
    r_omit = await client.put(
        f"/api/v1/projects/{pid1}/professores/P01",
        headers=headers,
        json={"nome": "Prof 1 Atualizado"},
    )
    assert r_omit.status_code == 200
    assert r_omit.json()["code"] == "P01"
    assert r_omit.json()["nome"] == "Prof 1 Atualizado"

    r_null = await client.put(
        f"/api/v1/projects/{pid1}/professores/P01",
        headers=headers,
        json={"code": None, "nome": "Prof 1 Null Code"},
    )
    assert r_null.status_code == 200
    assert r_null.json()["code"] == "P01"

    # 6. Manter mesmo code no PUT funciona normalmente
    r_same = await client.put(
        f"/api/v1/projects/{pid1}/professores/P01",
        headers=headers,
        json={"code": "P01", "nome": "Prof 1 Mesmo Code"},
    )
    assert r_same.status_code == 200
    assert r_same.json()["code"] == "P01"

    # 7. Renomear para code existente em outro projeto é permitido (isolamento por projeto)
    r_cross = await client.put(
        f"/api/v1/projects/{pid2}/professores/P99",
        headers=headers,
        json={"code": "P02", "nome": "Prof Outro Proj"},
    )
    assert r_cross.status_code == 200
    assert r_cross.json()["code"] == "P02"
