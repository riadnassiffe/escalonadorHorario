from __future__ import annotations

import uuid
from typing import Any

from sqlalchemy import delete, select
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy.orm import Session

from app.models.academic import DisciplinaModel, ProfessorModel, RoomModel, TurmaModel
from app.models.auth import User
from app.models.project import OptimizationConfigModel, Project, default_slot_templates
from app.models.timetable import AllocatedLessonModel, TimetableVersion
from app.services.conflict_engine import ProjectSnapshot
from app.services.time_utils import (
    ALL_DAYS,
    legacy_jornada_to_slots,
    minutes_to_time_str,
    normalize_day,
    time_str_to_minutes,
)


def _professor_to_json(p: ProfessorModel) -> dict[str, Any]:
    data: dict[str, Any] = {
        "id": p.code,
        "nome": p.nome,
        "preferencia_dias": p.preferencia_dias,
        "disponibilidade": p.disponibilidade,
        "carga_maxima_diaria_minutos": p.carga_maxima_diaria_minutos,
        "max_aulas_por_dia": p.max_aulas_por_dia,
        "peso_janela": p.peso_janela,
        "turnos_pretendidos": p.turnos_pretendidos,
    }
    if p.email:
        data["email"] = p.email
    return data


def _room_to_json(r: RoomModel) -> dict[str, Any]:
    return {
        "id": r.code,
        "nome": r.nome,
        "tipo": r.tipo,
        "capacidade": r.capacidade,
        "recursos": r.recursos,
    }


def _turma_to_json(t: TurmaModel) -> dict[str, Any]:
    return {
        "id": t.code,
        "nome": t.nome,
        "turno": t.turno,
        "dias_letivos": t.dias_letivos,
        "jornada_padrao": t.jornada_padrao,
        "excecoes_por_dia": t.excecoes_por_dia,
        "sala_teorica_id": t.sala_teorica_id,
        "salas_teoricas_permitidas": t.salas_teoricas_permitidas,
    }


def _disciplina_to_json(d: DisciplinaModel) -> dict[str, Any]:
    return {
        "id": d.code,
        "nome": d.nome,
        "tipo": d.tipo,
        "duracao_minutos": d.duracao_minutos,
        "aulas_semanais": d.aulas_semanais,
        "regime": d.regime,
        "semestre": d.semestre,
        "professores": d.professores,
        "turmas": d.turmas,
        "salas_alternativas": d.salas_alternativas,
        "exigir_aulas_consecutivas": d.exigir_aulas_consecutivas,
        "max_aulas_por_dia_turma": d.max_aulas_por_dia_turma,
    }


def _config_to_json(cfg: OptimizationConfigModel) -> dict[str, Any]:
    return {
        "lambda_janelas": cfg.lambda_janelas,
        "lambda_dias": cfg.lambda_dias,
        "lambda_preferencias": cfg.lambda_preferencias,
        "lambda_lacunas_turma": cfg.lambda_lacunas_turma,
        "lambda_preferencia_dias": cfg.lambda_preferencia_dias,
        "lambda_preferencia_turnos": cfg.lambda_preferencia_turnos,
        "modelo_preferencia_dias": cfg.modelo_preferencia_dias,
        "preenchimento_integral_jornada": cfg.preenchimento_integral_jornada,
        "distribuir_disciplinas_na_semana": cfg.distribuir_disciplinas_na_semana,
        "max_aulas_disciplina_dia_turma": cfg.max_aulas_disciplina_dia_turma,
        "escopo_semestre": cfg.escopo_semestre,
        "sincronizar_anuais_entre_semestres": cfg.sincronizar_anuais_entre_semestres,
        "algoritmo": cfg.algoritmo,
        "tempo_limite_segundos": cfg.tempo_limite_segundos,
        "sem_limite_tempo": cfg.sem_limite_tempo,
        "max_memoria_mb": cfg.max_memoria_mb,
        "sem_limite_memoria": cfg.sem_limite_memoria,
        "num_threads": cfg.num_threads,
        "usar_distribuicao_como_entrada": cfg.usar_distribuicao_como_entrada,
    }


def _timetable_to_json(
    version: TimetableVersion | None,
    alocacoes: list[AllocatedLessonModel],
) -> dict[str, Any]:
    if version is None:
        return {
            "alocacoes": [],
            "salas_teoricas_designadas": {},
            "estatisticas": {
                "status": "NAO_EXECUTADO",
                "tempo_execucao_ms": 0.0,
                "valor_objetivo": 0.0,
                "penalidade_janelas_J": 0,
                "penalidade_dias_D": 0,
                "penalidade_preferencias_P": 0,
                "penalidade_preferencia_dias_W": 0,
                "penalidade_lacunas_turma_JG": 0,
                "total_aulas_alocadas": 0,
                "total_aulas_esperadas": 0,
            },
            "mensagens_diagnostico": [],
        }

    return {
        "alocacoes": [
            {
                "aula_id": a.aula_id,
                "disciplina_id": a.disciplina_id,
                "disciplina_nome": a.disciplina_nome,
                "tipo": a.tipo,
                "dia": a.dia,
                "inicio": minutes_to_time_str(a.start_min),
                "fim": minutes_to_time_str(a.end_min),
                "professores": a.professores,
                "turmas": a.turmas,
                "salas": a.salas,
                "is_pinned": a.is_pinned,
                "regime": a.regime,
                "semestre": a.semestre,
                "semestre_alocado": a.semestre_alocado,
            }
            for a in alocacoes
        ],
        "salas_teoricas_designadas": version.salas_teoricas_designadas,
        "estatisticas": version.estatisticas,
        "mensagens_diagnostico": version.mensagens_diagnostico,
    }


async def export_project_json(db: AsyncSession, project_id: uuid.UUID) -> dict[str, Any]:
    project = await db.get(Project, project_id)
    profs = (await db.execute(select(ProfessorModel).where(ProfessorModel.project_id == project_id))).scalars().all()
    rooms = (await db.execute(select(RoomModel).where(RoomModel.project_id == project_id))).scalars().all()
    turmas = (await db.execute(select(TurmaModel).where(TurmaModel.project_id == project_id))).scalars().all()
    discs = (await db.execute(select(DisciplinaModel).where(DisciplinaModel.project_id == project_id))).scalars().all()
    cfg = await db.get(OptimizationConfigModel, project_id)

    version: TimetableVersion | None = None
    alocs: list[AllocatedLessonModel] = []
    if project and project.active_timetable_version_id:
        version = await db.get(TimetableVersion, project.active_timetable_version_id)
        if version:
            alocs = (
                await db.execute(
                    select(AllocatedLessonModel).where(AllocatedLessonModel.version_id == version.id)
                )
            ).scalars().all()

    return {
        "professores": [_professor_to_json(p) for p in profs],
        "salas": [_room_to_json(r) for r in rooms],
        "turmas": [_turma_to_json(t) for t in turmas],
        "disciplinas": [_disciplina_to_json(d) for d in discs],
        "slot_templates": cfg.slot_templates if cfg else default_slot_templates(),
        "config": _config_to_json(cfg) if cfg else {},
        "grade": _timetable_to_json(version, alocs),
    }


def export_project_json_sync(db: Session, project_id: uuid.UUID, config_override: dict[str, Any] | None = None) -> dict[str, Any]:
    project = db.get(Project, project_id)
    profs = db.execute(select(ProfessorModel).where(ProfessorModel.project_id == project_id)).scalars().all()
    rooms = db.execute(select(RoomModel).where(RoomModel.project_id == project_id)).scalars().all()
    turmas = db.execute(select(TurmaModel).where(TurmaModel.project_id == project_id)).scalars().all()
    discs = db.execute(select(DisciplinaModel).where(DisciplinaModel.project_id == project_id)).scalars().all()
    cfg = db.get(OptimizationConfigModel, project_id)

    version: TimetableVersion | None = None
    alocs: list[AllocatedLessonModel] = []
    if project and project.active_timetable_version_id:
        version = db.get(TimetableVersion, project.active_timetable_version_id)
        if version:
            alocs = db.execute(
                select(AllocatedLessonModel).where(AllocatedLessonModel.version_id == version.id)
            ).scalars().all()

    cfg_dict = _config_to_json(cfg) if cfg else {}
    if config_override:
        cfg_dict.update({k: v for k, v in config_override.items() if v is not None})

    return {
        "professores": [_professor_to_json(p) for p in profs],
        "salas": [_room_to_json(r) for r in rooms],
        "turmas": [_turma_to_json(t) for t in turmas],
        "disciplinas": [_disciplina_to_json(d) for d in discs],
        "slot_templates": cfg.slot_templates if cfg else default_slot_templates(),
        "config": cfg_dict,
        "grade": _timetable_to_json(version, alocs),
    }


async def build_project_snapshot(
    db: AsyncSession, project_id: uuid.UUID, version_id: uuid.UUID | None = None
) -> tuple[ProjectSnapshot, TimetableVersion | None]:
    project = await db.get(Project, project_id)
    profs = (await db.execute(select(ProfessorModel).where(ProfessorModel.project_id == project_id))).scalars().all()
    rooms = (await db.execute(select(RoomModel).where(RoomModel.project_id == project_id))).scalars().all()
    turmas = (await db.execute(select(TurmaModel).where(TurmaModel.project_id == project_id))).scalars().all()
    discs = (await db.execute(select(DisciplinaModel).where(DisciplinaModel.project_id == project_id))).scalars().all()
    cfg = await db.get(OptimizationConfigModel, project_id)

    target_vid = version_id or (project.active_timetable_version_id if project else None)
    version = await db.get(TimetableVersion, target_vid) if target_vid else None
    alocs: list[AllocatedLessonModel] = []
    if version:
        alocs = (
            await db.execute(select(AllocatedLessonModel).where(AllocatedLessonModel.version_id == version.id))
        ).scalars().all()

    snap = ProjectSnapshot(
        professores={p.code: _professor_to_json(p) for p in profs},
        salas={r.code: _room_to_json(r) for r in rooms},
        turmas={t.code: _turma_to_json(t) for t in turmas},
        disciplinas={d.code: _disciplina_to_json(d) for d in discs},
        config=_config_to_json(cfg) if cfg else {},
        alocacoes=[
            {
                "aula_id": a.aula_id,
                "disciplina_id": a.disciplina_id,
                "disciplina_nome": a.disciplina_nome,
                "tipo": a.tipo,
                "dia": a.dia,
                "start_min": a.start_min,
                "end_min": a.end_min,
                "professores": list(a.professores),
                "turmas": list(a.turmas),
                "salas": list(a.salas),
                "is_pinned": a.is_pinned,
                "regime": a.regime,
                "semestre": a.semestre,
                "semestre_alocado": a.semestre_alocado,
            }
            for a in alocs
        ],
    )
    return snap, version


async def import_project_json(
    db: AsyncSession,
    project_id: uuid.UUID,
    data: dict[str, Any],
    user_id: uuid.UUID | None = None,
) -> TimetableVersion | None:
    project = await db.get(Project, project_id)
    if project is None:
        raise ValueError("Projeto não encontrado")

    # Preserve existing professor account links before replacing entities
    existing_profs = (
        await db.execute(select(ProfessorModel).where(ProfessorModel.project_id == project_id))
    ).scalars().all()
    existing_links = {p.code: (p.email, p.user_id) for p in existing_profs}

    inst_users = (
        await db.execute(select(User).where(User.institution_id == project.institution_id))
    ).scalars().all()
    inst_users_by_email = {u.email.lower(): u.id for u in inst_users}

    # Clear existing academic entities
    await db.execute(delete(ProfessorModel).where(ProfessorModel.project_id == project_id))
    await db.execute(delete(RoomModel).where(RoomModel.project_id == project_id))
    await db.execute(delete(TurmaModel).where(TurmaModel.project_id == project_id))
    await db.execute(delete(DisciplinaModel).where(DisciplinaModel.project_id == project_id))

    for p in data.get("professores", []):
        turnos = p.get("turnos_pretendidos") or {d: ["MANHA", "TARDE", "NOITE"] for d in ALL_DAYS}
        code = p["id"]
        prev_email, prev_user_id = existing_links.get(code, (None, None))
        raw_email = p.get("email")
        email = raw_email.strip().lower() if isinstance(raw_email, str) and raw_email.strip() else prev_email
        linked_uid = inst_users_by_email.get(email) if email else prev_user_id
        if linked_uid is None and email == prev_email:
            linked_uid = prev_user_id
        db.add(
            ProfessorModel(
                project_id=project_id,
                code=code,
                nome=p["nome"],
                email=email,
                user_id=linked_uid,
                preferencia_dias=p.get("preferencia_dias", {d: 5 for d in ALL_DAYS}),
                disponibilidade=p.get("disponibilidade", {}),
                turnos_pretendidos=turnos,
                carga_maxima_diaria_minutos=int(p.get("carga_maxima_diaria_minutos", 0)),
                max_aulas_por_dia=int(p.get("max_aulas_por_dia", 0)),
                peso_janela=int(p.get("peso_janela", 8)),
            )
        )

    for r in data.get("salas", []):
        db.add(
            RoomModel(
                project_id=project_id,
                code=r["id"],
                nome=r["nome"],
                tipo=r.get("tipo", "TEORICA"),
                capacidade=int(r.get("capacidade", 40)),
                recursos=r.get("recursos", []),
            )
        )

    for t in data.get("turmas", []):
        dias = [normalize_day(d) for d in t.get("dias_letivos", ALL_DAYS)]
        jornada = legacy_jornada_to_slots(t.get("jornada_padrao", {}))
        exc_raw = t.get("excecoes_por_dia", {})
        exc = {normalize_day(k): legacy_jornada_to_slots(v) for k, v in exc_raw.items() if isinstance(v, dict)}
        db.add(
            TurmaModel(
                project_id=project_id,
                code=t["id"],
                nome=t["nome"],
                turno=t.get("turno", "MANHA"),
                dias_letivos=dias,
                jornada_padrao=jornada,
                excecoes_por_dia=exc,
                sala_teorica_id=t.get("sala_teorica_id") or None,
                salas_teoricas_permitidas=t.get("salas_teoricas_permitidas", []),
            )
        )

    for d in data.get("disciplinas", []):
        db.add(
            DisciplinaModel(
                project_id=project_id,
                code=d["id"],
                nome=d["nome"],
                tipo=d.get("tipo", "TEORICA"),
                duracao_minutos=int(d.get("duracao_minutos", 50)),
                aulas_semanais=int(d.get("aulas_semanais", 4)),
                regime=d.get("regime", "ANUAL"),
                semestre=str(d.get("semestre", "AMBOS")),
                professores=d.get("professores", []),
                turmas=d.get("turmas", []),
                salas_alternativas=d.get("salas_alternativas", []),
                exigir_aulas_consecutivas=bool(d.get("exigir_aulas_consecutivas", False)),
                max_aulas_por_dia_turma=int(d.get("max_aulas_por_dia_turma", 2)),
            )
        )

    cfg_model = await db.get(OptimizationConfigModel, project_id)
    if cfg_model is None:
        cfg_model = OptimizationConfigModel(project_id=project_id)
        db.add(cfg_model)

    cfg_in = data.get("config", {})
    for field_name in [
        "algoritmo",
        "escopo_semestre",
        "sincronizar_anuais_entre_semestres",
        "lambda_janelas",
        "lambda_dias",
        "lambda_preferencias",
        "lambda_preferencia_dias",
        "lambda_preferencia_turnos",
        "lambda_lacunas_turma",
        "modelo_preferencia_dias",
        "preenchimento_integral_jornada",
        "distribuir_disciplinas_na_semana",
        "max_aulas_disciplina_dia_turma",
        "tempo_limite_segundos",
        "sem_limite_tempo",
        "max_memoria_mb",
        "sem_limite_memoria",
        "num_threads",
        "log_search_progress",
        "usar_distribuicao_como_entrada",
    ]:
        if field_name in cfg_in:
            setattr(cfg_model, field_name, cfg_in[field_name])

    if isinstance(data.get("slot_templates"), dict):
        cfg_model.slot_templates = data["slot_templates"]

    grade_in = data.get("grade") or {}
    created_version: TimetableVersion | None = None
    if grade_in.get("alocacoes"):
        existing_versions = (
            await db.execute(select(TimetableVersion).where(TimetableVersion.project_id == project_id))
        ).scalars().all()
        next_ver = max((v.version_number for v in existing_versions), default=0) + 1
        created_version = TimetableVersion(
            project_id=project_id,
            version_number=next_ver,
            label=f"Importado v{next_ver}",
            salas_teoricas_designadas=grade_in.get("salas_teoricas_designadas", {}),
            estatisticas=grade_in.get("estatisticas", {}),
            mensagens_diagnostico=grade_in.get("mensagens_diagnostico", []),
            created_by=user_id,
        )
        db.add(created_version)
        await db.flush()

        for aloc in grade_in["alocacoes"]:
            s_min = time_str_to_minutes(aloc["inicio"])
            e_min = time_str_to_minutes(aloc["fim"])
            db.add(
                AllocatedLessonModel(
                    version_id=created_version.id,
                    aula_id=aloc["aula_id"],
                    disciplina_id=aloc.get("disciplina_id", ""),
                    disciplina_nome=aloc.get("disciplina_nome", ""),
                    tipo=aloc.get("tipo", "TEORICA"),
                    dia=normalize_day(aloc.get("dia", "SEG")),
                    start_min=s_min,
                    end_min=e_min,
                    professores=aloc.get("professores", []),
                    turmas=aloc.get("turmas", []),
                    salas=aloc.get("salas", []),
                    is_pinned=bool(aloc.get("is_pinned", False)),
                    regime=aloc.get("regime", "ANUAL"),
                    semestre=str(aloc.get("semestre", "AMBOS")),
                    semestre_alocado=int(aloc.get("semestre_alocado", 1)),
                )
            )
        project.active_timetable_version_id = created_version.id

    await db.commit()
    return created_version


def build_default_example_json() -> dict[str, Any]:
    """Matches InstitutionData::criarExemploPadrao() in src/model/InstitutionData.cpp."""
    disp_all = {d: [{"inicio": "08:00", "fim": "12:30"}] for d in ALL_DAYS}
    turnos_all = {d: ["MANHA", "TARDE", "NOITE"] for d in ALL_DAYS}
    professores = [
        {
            "id": "P01",
            "nome": "Prof. Carlos Silva (Matemática)",
            "preferencia_dias": {"SEG": 10, "TER": 7, "QUA": 2, "QUI": 8, "SEX": 0},
            "disponibilidade": disp_all,
            "turnos_pretendidos": turnos_all,
            "carga_maxima_diaria_minutos": 0,
            "max_aulas_por_dia": 0,
            "peso_janela": 8,
        },
        {
            "id": "P02",
            "nome": "Profa. Ana Souza (Química)",
            "preferencia_dias": {"SEG": 5, "TER": 10, "QUA": 8, "QUI": 6, "SEX": 2},
            "disponibilidade": disp_all,
            "turnos_pretendidos": turnos_all,
            "carga_maxima_diaria_minutos": 0,
            "max_aulas_por_dia": 0,
            "peso_janela": 8,
        },
        {
            "id": "P03",
            "nome": "Prof. Marcos Lima (Física)",
            "preferencia_dias": {"SEG": 4, "TER": 5, "QUA": 10, "QUI": 9, "SEX": 6},
            "disponibilidade": disp_all,
            "turnos_pretendidos": turnos_all,
            "carga_maxima_diaria_minutos": 0,
            "max_aulas_por_dia": 0,
            "peso_janela": 8,
        },
        {
            "id": "P04",
            "nome": "Profa. Juliana Mendes (Humanas)",
            "preferencia_dias": {"SEG": 8, "TER": 6, "QUA": 5, "QUI": 10, "SEX": 7},
            "disponibilidade": disp_all,
            "turnos_pretendidos": turnos_all,
            "carga_maxima_diaria_minutos": 0,
            "max_aulas_por_dia": 0,
            "peso_janela": 8,
        },
        {
            "id": "P05",
            "nome": "Prof. Roberto Castro (Linguagens)",
            "preferencia_dias": {"SEG": 6, "TER": 9, "QUA": 7, "QUI": 5, "SEX": 8},
            "disponibilidade": disp_all,
            "turnos_pretendidos": turnos_all,
            "carga_maxima_diaria_minutos": 0,
            "max_aulas_por_dia": 0,
            "peso_janela": 8,
        },
        {
            "id": "P06",
            "nome": "Profa. Beatriz Costa (Biológicas)",
            "preferencia_dias": {"SEG": 7, "TER": 5, "QUA": 8, "QUI": 7, "SEX": 9},
            "disponibilidade": disp_all,
            "turnos_pretendidos": turnos_all,
            "carga_maxima_diaria_minutos": 0,
            "max_aulas_por_dia": 0,
            "peso_janela": 8,
        },
    ]

    salas = [
        {"id": "S101", "nome": "Sala Teórica 101", "tipo": "TEORICA", "capacidade": 40, "recursos": []},
        {"id": "S102", "nome": "Sala Teórica 102", "tipo": "TEORICA", "capacidade": 40, "recursos": []},
        {"id": "S201", "nome": "Sala Teórica 201", "tipo": "TEORICA", "capacidade": 45, "recursos": []},
        {"id": "LAB01", "nome": "Laboratório de Química 1", "tipo": "LABORATORIO", "capacidade": 35, "recursos": []},
        {"id": "LAB02", "nome": "Laboratório Multidisciplinar", "tipo": "LABORATORIO", "capacidade": 30, "recursos": []},
        {"id": "LAB_INF", "nome": "Laboratório de Informática", "tipo": "LABORATORIO", "capacidade": 35, "recursos": []},
    ]

    jornada_exemplo = {
        "slots": [
            {"inicio": "08:00", "fim": "08:50"},
            {"inicio": "08:50", "fim": "09:40"},
            {"inicio": "09:55", "fim": "10:40"},
            {"inicio": "10:40", "fim": "11:25"},
            {"inicio": "11:40", "fim": "12:30"},
        ]
    }
    turmas = [
        {
            "id": "T01",
            "nome": "1º Ano A - Ensino Médio",
            "turno": "MANHA",
            "dias_letivos": ALL_DAYS,
            "jornada_padrao": jornada_exemplo,
            "excecoes_por_dia": {},
            "sala_teorica_id": "S101",
            "salas_teoricas_permitidas": ["S101"],
        },
        {
            "id": "T02",
            "nome": "2º Ano A - Ensino Médio",
            "turno": "MANHA",
            "dias_letivos": ALL_DAYS,
            "jornada_padrao": jornada_exemplo,
            "excecoes_por_dia": {},
            "sala_teorica_id": "S102",
            "salas_teoricas_permitidas": ["S102"],
        },
        {
            "id": "T03",
            "nome": "3º Ano A - Ensino Médio",
            "turno": "MANHA",
            "dias_letivos": ALL_DAYS,
            "jornada_padrao": jornada_exemplo,
            "excecoes_por_dia": {},
            "sala_teorica_id": None,
            "salas_teoricas_permitidas": ["S201"],
        },
    ]

    def disc(code, nome, tipo, dur, aulas, reg, sem, profs, turmas_l, salas_alt=None):
        return {
            "id": code,
            "nome": nome,
            "tipo": tipo,
            "duracao_minutos": dur,
            "aulas_semanais": aulas,
            "regime": reg,
            "semestre": sem,
            "professores": profs,
            "turmas": turmas_l,
            "salas_alternativas": salas_alt or [],
            "exigir_aulas_consecutivas": False,
            "max_aulas_por_dia_turma": 2,
        }

    disciplinas = [
        disc("T01_MAT", "Matemática", "TEORICA", 45, 5, "ANUAL", "AMBOS", ["P01"], ["T01"]),
        disc("T01_PORT", "Português", "TEORICA", 50, 4, "ANUAL", "AMBOS", ["P05"], ["T01"]),
        disc("T01_FIS", "Física", "TEORICA", 50, 3, "ANUAL", "AMBOS", ["P03"], ["T01"]),
        disc("T01_QUI_EXP", "Química Experimental", "LABORATORIO", 50, 2, "SEMESTRAL", "1", ["P02"], ["T01"], [["LAB01"], ["LAB02"]]),
        disc("T01_HIST", "História", "TEORICA", 45, 3, "SEMESTRAL", "1", ["P04"], ["T01"]),
        disc("T01_GEO", "Geografia", "TEORICA", 45, 3, "SEMESTRAL", "2", ["P04"], ["T01"]),
        disc("T01_BIO", "Biologia", "TEORICA", 45, 2, "ANUAL", "AMBOS", ["P06"], ["T01"]),
        disc("T02_MAT", "Matemática", "TEORICA", 50, 4, "ANUAL", "AMBOS", ["P01"], ["T02"]),
        disc("T02_PORT", "Português", "TEORICA", 50, 4, "ANUAL", "AMBOS", ["P05"], ["T02"]),
        disc("T02_FIS", "Física", "TEORICA", 50, 3, "ANUAL", "AMBOS", ["P03"], ["T02"]),
        disc("T02_QUI", "Química Geral", "TEORICA", 45, 3, "SEMESTRAL", "1", ["P02"], ["T02"]),
        disc("T02_HIST", "História", "TEORICA", 45, 3, "SEMESTRAL", "1", ["P04"], ["T02"]),
        disc("T02_BIO", "Biologia", "TEORICA", 50, 3, "SEMESTRAL", "2", ["P06"], ["T02"]),
        disc("T03_MAT", "Matemática Avançada", "TEORICA", 50, 4, "ANUAL", "AMBOS", ["P01"], ["T03"]),
        disc("T03_FIS", "Física Moderna", "TEORICA", 50, 4, "ANUAL", "AMBOS", ["P03"], ["T03"]),
        disc("T03_QUI_LAB", "Química Orgânica (Prática)", "LABORATORIO", 50, 3, "SEMESTRAL", "1", ["P02"], ["T03"], [["LAB01"], ["LAB02"]]),
        disc("T03_LIT", "Literatura & Redação", "TEORICA", 50, 4, "ANUAL", "AMBOS", ["P05"], ["T03"]),
        disc("T03_BIO", "Genética & Biologia", "TEORICA", 45, 3, "SEMESTRAL", "2", ["P06"], ["T03"]),
    ]

    return {
        "professores": professores,
        "salas": salas,
        "turmas": turmas,
        "disciplinas": disciplinas,
        "slot_templates": default_slot_templates(),
        "config": {},
        "grade": {"alocacoes": [], "salas_teoricas_designadas": {}, "estatisticas": {}},
    }
