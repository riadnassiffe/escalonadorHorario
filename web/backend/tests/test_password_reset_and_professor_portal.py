from __future__ import annotations

import pytest
from httpx import AsyncClient

from app.core.config import settings
from tests.conftest import register_and_login


@pytest.mark.asyncio
async def test_forgot_and_reset_password_flow(client: AsyncClient, monkeypatch: pytest.MonkeyPatch):
    monkeypatch.setattr(settings, "APP_ENV", "development")
    monkeypatch.setattr(settings, "SMTP_HOST", None)

    await register_and_login(
        client,
        institution_name="Escola Reset",
        full_name="Admin Reset",
        email="reset@escola.br",
        password="oldpassword1",
    )

    # E-mail inexistente retorna 200 sem dev_reset_token
    resp_missing = await client.post("/api/v1/auth/forgot-password", json={"email": "naoexiste@escola.br"})
    assert resp_missing.status_code == 200
    assert resp_missing.json()["dev_reset_token"] is None

    # Solicita primeiro token
    resp_req1 = await client.post("/api/v1/auth/forgot-password", json={"email": "reset@escola.br"})
    assert resp_req1.status_code == 200
    token1 = resp_req1.json()["dev_reset_token"]
    assert token1
    assert resp_req1.json()["dev_reset_url"]

    # Novo request invalida token anterior
    resp_req2 = await client.post("/api/v1/auth/forgot-password", json={"email": "reset@escola.br"})
    assert resp_req2.status_code == 200
    token2 = resp_req2.json()["dev_reset_token"]
    assert token2 and token2 != token1

    resp_old_tok = await client.post(
        "/api/v1/auth/reset-password",
        json={"token": token1, "new_password": "newpassword2"},
    )
    assert resp_old_tok.status_code == 400

    # Token inválido retorna 400
    resp_bad_tok = await client.post(
        "/api/v1/auth/reset-password",
        json={"token": "token-invalido-123456", "new_password": "newpassword2"},
    )
    assert resp_bad_tok.status_code == 400

    # Reset com token válido
    resp_reset = await client.post(
        "/api/v1/auth/reset-password",
        json={"token": token2, "new_password": "newpassword2"},
    )
    assert resp_reset.status_code == 200
    assert resp_reset.json()["ok"] is True

    # Reutilização do mesmo token falha
    resp_reuse = await client.post(
        "/api/v1/auth/reset-password",
        json={"token": token2, "new_password": "newpassword3"},
    )
    assert resp_reuse.status_code == 400

    # Senha antiga falha, nova senha funciona
    fail_login = await client.post(
        "/api/v1/auth/login",
        json={"email": "reset@escola.br", "password": "oldpassword1"},
    )
    assert fail_login.status_code == 401

    ok_login = await client.post(
        "/api/v1/auth/login",
        json={"email": "reset@escola.br", "password": "newpassword2"},
    )
    assert ok_login.status_code == 200

    # Em produção não expõe dev_reset_token nem dev_reset_url
    monkeypatch.setattr(settings, "APP_ENV", "production")
    resp_prod = await client.post("/api/v1/auth/forgot-password", json={"email": "reset@escola.br"})
    assert resp_prod.status_code == 200
    assert resp_prod.json()["dev_reset_token"] is None
    assert resp_prod.json()["dev_reset_url"] is None


@pytest.mark.asyncio
async def test_professor_auto_provisioning_portal_and_admin_lock(client: AsyncClient):
    auth_admin = await register_and_login(
        client,
        institution_name="Colégio Central",
        full_name="Diretor Geral",
        email="diretor@central.edu.br",
        password="adminpassword",
    )
    headers_admin = {"Authorization": f"Bearer {auth_admin['access_token']}"}

    # Criar coordenador para testar restrição do botão de bloqueio exclusivo do ADMIN
    resp_coord = await client.post(
        "/api/v1/auth/users",
        headers=headers_admin,
        json={
            "full_name": "Coord Teste",
            "email": "coord@central.edu.br",
            "password": "coordpassword",
            "role": "COORDENADOR",
        },
    )
    assert resp_coord.status_code == 201
    login_coord = await client.post(
        "/api/v1/auth/login",
        json={"email": "coord@central.edu.br", "password": "coordpassword"},
    )
    headers_coord = {"Authorization": f"Bearer {login_coord.json()['access_token']}"}

    # Criar projeto
    resp_p = await client.post(
        "/api/v1/projects",
        headers=headers_admin,
        json={"name": "Grade Central 2026"},
    )
    assert resp_p.status_code == 201
    pid = resp_p.json()["id"]
    assert resp_p.json()["allow_professor_preferences_edit"] is True

    # Criar professor 1 sem e-mail -> gera .internal + senha inicial
    resp_prof1 = await client.post(
        f"/api/v1/projects/{pid}/professores",
        headers=headers_admin,
        json={"code": "P01", "nome": "Prof. João Sem Email"},
    )
    assert resp_prof1.status_code == 201, resp_prof1.text
    prof1_data = resp_prof1.json()
    assert prof1_data["email"].endswith(".internal")
    assert prof1_data["account_created"] is True
    assert prof1_data["has_user_account"] is True
    assert prof1_data["must_change_password"] is True
    assert prof1_data["initial_password"]

    # Criar professor 2 com e-mail real
    resp_prof2 = await client.post(
        f"/api/v1/projects/{pid}/professores",
        headers=headers_admin,
        json={"code": "P02", "nome": "Profa. Maria", "email": "maria@central.edu.br"},
    )
    assert resp_prof2.status_code == 201
    prof2_data = resp_prof2.json()
    assert prof2_data["email"] == "maria@central.edu.br"
    assert prof2_data["initial_password"]

    # Login do Professor 1 com senha inicial
    login_p1 = await client.post(
        "/api/v1/auth/login",
        json={"email": prof1_data["email"], "password": prof1_data["initial_password"]},
    )
    assert login_p1.status_code == 200
    assert login_p1.json()["user"]["role"] == "PROFESSOR"
    assert login_p1.json()["user"]["must_change_password"] is True
    headers_p1 = {"Authorization": f"Bearer {login_p1.json()['access_token']}"}

    # Troca obrigatória de senha
    resp_chg = await client.post(
        "/api/v1/auth/change-password",
        headers=headers_p1,
        json={
            "current_password": prof1_data["initial_password"],
            "new_password": "minhaNovaSenha123",
        },
    )
    assert resp_chg.status_code == 200
    assert resp_chg.json()["must_change_password"] is False

    # Perfis do professor 1
    resp_profiles = await client.get("/api/v1/professor/me/profiles", headers=headers_p1)
    assert resp_profiles.status_code == 200
    profiles = resp_profiles.json()
    assert len(profiles) == 1
    prof1_uuid = profiles[0]["professor_id"]
    assert profiles[0]["allow_preferences_edit"] is True

    # Atualiza preferências de turno, dia, disponibilidade e peso_janela
    resp_upd_pref = await client.patch(
        f"/api/v1/professor/me/profiles/{prof1_uuid}/preferences",
        headers=headers_p1,
        json={
            "preferencia_dias": {"SEG": 10, "TER": 8, "QUA": 0, "QUI": 6, "SEX": 4},
            "turnos_pretendidos": {
                "SEG": ["MANHA"],
                "TER": ["MANHA", "TARDE"],
                "QUA": [],
                "QUI": ["NOITE"],
                "SEX": ["MANHA"],
            },
            "disponibilidade": {
                "SEG": [{"inicio": "07:30", "fim": "12:00"}],
                "TER": [{"inicio": "07:30", "fim": "17:00"}],
                "QUA": [],
                "QUI": [{"inicio": "19:00", "fim": "22:30"}],
                "SEX": [{"inicio": "07:30", "fim": "12:00"}],
            },
            "peso_janela": 9,
        },
    )
    assert resp_upd_pref.status_code == 200, resp_upd_pref.text
    updated_prof = resp_upd_pref.json()
    assert updated_prof["preferencia_dias"]["SEG"] == 10
    assert updated_prof["turnos_pretendidos"]["QUI"] == ["NOITE"]
    assert updated_prof["peso_janela"] == 9

    # Admin enxerga alterações na listagem e no export-json
    resp_list_profs = await client.get(f"/api/v1/projects/{pid}/professores", headers=headers_admin)
    assert resp_list_profs.status_code == 200
    p01_admin = next(p for p in resp_list_profs.json() if p["code"] == "P01")
    assert p01_admin["preferencia_dias"]["QUA"] == 0
    assert p01_admin["turnos_pretendidos"]["SEG"] == ["MANHA"]

    resp_export = await client.get(f"/api/v1/projects/{pid}/export-json", headers=headers_admin)
    assert resp_export.status_code == 200
    p01_exp = next(p for p in resp_export.json()["professores"] if p["id"] == "P01")
    assert p01_exp["turnos_pretendidos"]["QUI"] == ["NOITE"]

    # RBAC: Professor não pode editar outro professor, criar turma ou iniciar otimização
    assert (
        await client.put(
            f"/api/v1/projects/{pid}/professores/P02",
            headers=headers_p1,
            json={"nome": "Invasão"},
        )
    ).status_code == 403
    assert (
        await client.post(
            f"/api/v1/projects/{pid}/turmas",
            headers=headers_p1,
            json={"nome": "Turma X"},
        )
    ).status_code == 403
    assert (
        await client.post(
            f"/api/v1/projects/{pid}/optimize",
            headers=headers_p1,
            json={},
        )
    ).status_code == 403

    # Professor 2 tentando editar perfil do Professor 1 recebe 404
    login_p2 = await client.post(
        "/api/v1/auth/login",
        json={"email": "maria@central.edu.br", "password": prof2_data["initial_password"]},
    )
    headers_p2 = {"Authorization": f"Bearer {login_p2.json()['access_token']}"}
    assert (
        await client.patch(
            f"/api/v1/professor/me/profiles/{prof1_uuid}/preferences",
            headers=headers_p2,
            json={
                "preferencia_dias": {"SEG": 1, "TER": 1, "QUA": 1, "QUI": 1, "SEX": 1},
                "turnos_pretendidos": {"SEG": ["MANHA"], "TER": ["MANHA"], "QUA": ["MANHA"], "QUI": ["MANHA"], "SEX": ["MANHA"]},
            },
        )
    ).status_code == 404

    # Controle de bloqueio/liberação exclusivo do ADMIN
    resp_coord_lock = await client.patch(
        f"/api/v1/projects/{pid}/professor-preferences-access",
        headers=headers_coord,
        json={"allow_edit": False},
    )
    assert resp_coord_lock.status_code == 403

    resp_admin_lock = await client.patch(
        f"/api/v1/projects/{pid}/professor-preferences-access",
        headers=headers_admin,
        json={"allow_edit": False},
    )
    assert resp_admin_lock.status_code == 200
    assert resp_admin_lock.json()["allow_professor_preferences_edit"] is False

    # Professor vê bloqueio e recebe 403 ao tentar salvar
    resp_profiles_locked = await client.get("/api/v1/professor/me/profiles", headers=headers_p1)
    assert resp_profiles_locked.json()[0]["allow_preferences_edit"] is False

    resp_save_locked = await client.patch(
        f"/api/v1/professor/me/profiles/{prof1_uuid}/preferences",
        headers=headers_p1,
        json={
            "preferencia_dias": {"SEG": 5, "TER": 5, "QUA": 5, "QUI": 5, "SEX": 5},
            "turnos_pretendidos": {"SEG": ["MANHA"], "TER": ["MANHA"], "QUA": ["MANHA"], "QUI": ["MANHA"], "SEX": ["MANHA"]},
        },
    )
    assert resp_save_locked.status_code == 403

    # Admin libera novamente -> Professor consegue salvar
    resp_admin_unlock = await client.patch(
        f"/api/v1/projects/{pid}/professor-preferences-access",
        headers=headers_admin,
        json={"allow_edit": True},
    )
    assert resp_admin_unlock.status_code == 200
    assert resp_admin_unlock.json()["allow_professor_preferences_edit"] is True

    resp_save_unlocked = await client.patch(
        f"/api/v1/professor/me/profiles/{prof1_uuid}/preferences",
        headers=headers_p1,
        json={
            "preferencia_dias": {"SEG": 7, "TER": 7, "QUA": 7, "QUI": 7, "SEX": 7},
            "turnos_pretendidos": {"SEG": ["TARDE"], "TER": ["TARDE"], "QUA": ["TARDE"], "QUI": ["TARDE"], "SEX": ["TARDE"]},
        },
    )
    assert resp_save_unlocked.status_code == 200

    # Grade do professor retorna apenas suas aulas alocadas
    await client.post(
        f"/api/v1/projects/{pid}/salas",
        headers=headers_admin,
        json={"code": "S101", "nome": "Sala 101"},
    )
    await client.post(
        f"/api/v1/projects/{pid}/turmas",
        headers=headers_admin,
        json={"code": "T01", "nome": "1º Ano A"},
    )
    await client.post(
        f"/api/v1/projects/{pid}/disciplinas",
        headers=headers_admin,
        json={"code": "MAT", "nome": "Matemática", "professores": ["P01"], "turmas": ["T01"]},
    )
    await client.post(
        f"/api/v1/projects/{pid}/disciplinas",
        headers=headers_admin,
        json={"code": "QUI", "nome": "Química", "professores": ["P02"], "turmas": ["T01"]},
    )
    await client.post(
        f"/api/v1/projects/{pid}/timetable/allocate-manual",
        headers=headers_admin,
        json={
            "aula_id": "MAT_1",
            "disciplina_id": "MAT",
            "dia": "SEG",
            "inicio": "07:30",
            "fim": "08:20",
            "salas": ["S101"],
            "semestre_alocado": 1,
            "is_pinned": False,
        },
    )
    await client.post(
        f"/api/v1/projects/{pid}/timetable/allocate-manual",
        headers=headers_admin,
        json={
            "aula_id": "QUI_1",
            "disciplina_id": "QUI",
            "dia": "TER",
            "inicio": "07:30",
            "fim": "08:20",
            "salas": ["S101"],
            "semestre_alocado": 1,
            "is_pinned": False,
        },
    )

    resp_tt = await client.get(f"/api/v1/professor/me/profiles/{prof1_uuid}/timetable", headers=headers_p1)
    assert resp_tt.status_code == 200
    tt = resp_tt.json()
    assert len(tt["alocacoes"]) == 1
    assert tt["alocacoes"][0]["aula_id"] == "MAT_1"
    assert tt["turmas_map"]["T01"] == "1º Ano A"
    assert tt["salas_map"]["S101"] == "Sala 101"


@pytest.mark.asyncio
async def test_batch_and_single_professor_provision_after_import(client: AsyncClient):
    auth = await register_and_login(
        client,
        institution_name="Instituto Exemplo",
        full_name="Admin Exemplo",
        email="admin@exemplo.edu.br",
        password="adminpassword",
    )
    headers = {"Authorization": f"Bearer {auth['access_token']}"}

    resp_p = await client.post(
        "/api/v1/projects",
        headers=headers,
        json={"name": "Projeto Exemplo", "load_default_example": True},
    )
    assert resp_p.status_code == 201
    pid = resp_p.json()["id"]

    # Gera acesso em lote apenas para pendentes (6 professores do exemplo padrão)
    resp_batch1 = await client.post(
        f"/api/v1/projects/{pid}/professores/provision-access-batch",
        headers=headers,
        json={"only_missing": True},
    )
    assert resp_batch1.status_code == 200, resp_batch1.text
    b1 = resp_batch1.json()
    assert b1["total_processed"] == 6
    assert b1["created_count"] == 6
    assert b1["skipped_count"] == 0
    assert len(b1["credentials"]) == 6

    first_cred = b1["credentials"][0]
    assert first_cred["initial_password"]

    # Verifica que o primeiro professor consegue logar
    login_first = await client.post(
        "/api/v1/auth/login",
        json={"email": first_cred["email"], "password": first_cred["initial_password"]},
    )
    assert login_first.status_code == 200
    assert login_first.json()["user"]["must_change_password"] is True

    # Repete com only_missing=True -> todos ignorados
    resp_batch2 = await client.post(
        f"/api/v1/projects/{pid}/professores/provision-access-batch",
        headers=headers,
        json={"only_missing": True},
    )
    assert resp_batch2.status_code == 200
    b2 = resp_batch2.json()
    assert b2["created_count"] == 0
    assert b2["skipped_count"] == 6
    assert b2["credentials"] == []

    # Redefine senha individual de P01
    resp_single = await client.post(
        f"/api/v1/projects/{pid}/professores/P01/provision-access",
        headers=headers,
        json={"reset_password": True},
    )
    assert resp_single.status_code == 200
    s_cred = resp_single.json()
    assert s_cred["password_reset"] is True
    assert s_cred["initial_password"]
    assert s_cred["initial_password"] != first_cred["initial_password"]

    # Nova senha funciona e antiga falha
    assert (
        await client.post(
            "/api/v1/auth/login",
            json={"email": s_cred["email"], "password": s_cred["initial_password"]},
        )
    ).status_code == 200
    assert (
        await client.post(
            "/api/v1/auth/login",
            json={"email": s_cred["email"], "password": first_cred["initial_password"]},
        )
    ).status_code == 401
