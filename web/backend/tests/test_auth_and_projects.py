from __future__ import annotations

import json
from pathlib import Path
import pytest
from httpx import AsyncClient

from tests.conftest import register_and_login


REPO_ROOT = Path(__file__).resolve().parents[3]


@pytest.mark.asyncio
async def test_auth_rbac_and_multi_tenant_isolation(client: AsyncClient):
    auth_a = await register_and_login(client, "Inst A", "Admin A", "a@inst.br")
    auth_b = await register_and_login(client, "Inst B", "Admin B", "b@inst.br")

    headers_a = {"Authorization": f"Bearer {auth_a['access_token']}"}
    headers_b = {"Authorization": f"Bearer {auth_b['access_token']}"}

    # Create viewer user in Inst A
    resp_u = await client.post(
        "/api/v1/auth/users",
        headers=headers_a,
        json={"full_name": "Viewer A", "email": "viewer@inst.br", "password": "secret123", "role": "VISUALIZADOR"},
    )
    assert resp_u.status_code == 201

    login_v = await client.post(
        "/api/v1/auth/login",
        json={"email": "viewer@inst.br", "password": "secret123"},
    )
    assert login_v.status_code == 200
    headers_viewer = {"Authorization": f"Bearer {login_v.json()['access_token']}"}

    # Viewer cannot create project (403)
    resp_forbid = await client.post(
        "/api/v1/projects",
        headers=headers_viewer,
        json={"name": "Projeto Proibido"},
    )
    assert resp_forbid.status_code == 403

    # Admin A creates project
    resp_p = await client.post(
        "/api/v1/projects",
        headers=headers_a,
        json={"name": "Grade 2026 A", "load_default_example": True},
    )
    assert resp_p.status_code == 201
    pid = resp_p.json()["id"]
    assert resp_p.json()["counts"]["professores"] == 6
    assert resp_p.json()["counts"]["turmas"] == 3

    # Admin B cannot access project A (404)
    resp_cross = await client.get(f"/api/v1/projects/{pid}", headers=headers_b)
    assert resp_cross.status_code == 404

    # Refresh token works
    resp_ref = await client.post("/api/v1/auth/refresh", json={"refresh_token": auth_a["refresh_token"]})
    assert resp_ref.status_code == 200
    assert "access_token" in resp_ref.json()


@pytest.mark.asyncio
async def test_json_import_export_roundtrip_and_crud(client: AsyncClient):
    auth = await register_and_login(client, email="roundtrip@inst.br")
    headers = {"Authorization": f"Bearer {auth['access_token']}"}

    resp_p = await client.post("/api/v1/projects", headers=headers, json={"name": "RoundTrip"})
    assert resp_p.status_code == 201
    pid = resp_p.json()["id"]

    # Import real repo JSON
    sample_json = json.loads((REPO_ROOT / "escola_grade.json").read_text(encoding="utf-8"))
    resp_imp = await client.post(f"/api/v1/projects/{pid}/import-json", headers=headers, json={"data": sample_json})
    assert resp_imp.status_code == 200, resp_imp.text

    # Export JSON and verify counts
    resp_exp = await client.get(f"/api/v1/projects/{pid}/export-json", headers=headers)
    assert resp_exp.status_code == 200
    exported = resp_exp.json()
    assert len(exported["professores"]) == len(sample_json["professores"])
    assert len(exported["turmas"]) == len(sample_json["turmas"])
    assert len(exported["disciplinas"]) == len(sample_json["disciplinas"])

    # CRUD next-id + create + delete professor
    nxt = await client.get(f"/api/v1/projects/{pid}/professores/next-id", headers=headers)
    assert nxt.status_code == 200
    new_code = nxt.json()["next_id"]

    created = await client.post(
        f"/api/v1/projects/{pid}/professores",
        headers=headers,
        json={"code": new_code, "nome": "Prof. Novo Teste"},
    )
    assert created.status_code == 201
    assert created.json()["code"] == new_code

    deleted = await client.delete(f"/api/v1/projects/{pid}/professores/{new_code}", headers=headers)
    assert deleted.status_code == 204
