from __future__ import annotations

import json
import os
import signal
import subprocess
import tempfile
import uuid
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import redis
from celery.exceptions import SoftTimeLimitExceeded
from sqlalchemy import select
from sqlalchemy.orm import Session, sessionmaker

from app.core.config import settings
from app.core.database import SyncSessionLocal
from app.models.project import Project
from app.models.timetable import AllocatedLessonModel, OptimizationJob, TimetableVersion
from app.services.project_io import export_project_json_sync
from app.services.time_utils import normalize_day, time_str_to_minutes
from app.workers.celery_app import celery_app


def _job_channel(job_id: str) -> str:
    return f"job:{job_id}:events"


def _publish_event(r: Any, job_id: str, payload: dict[str, Any]) -> None:
    if r is None:
        return
    try:
        r.publish(_job_channel(job_id), json.dumps(payload))
    except Exception:
        pass


def execute_optimization_job_sync(
    job_id: str,
    session_factory: sessionmaker[Session] | None = None,
    redis_client: Any = None,
    cli_bin: str | None = None,
) -> dict[str, Any]:
    sf = session_factory or SyncSessionLocal
    r = redis_client
    owns_redis = False
    if r is None:
        try:
            r = redis.Redis.from_url(settings.REDIS_URL, decode_responses=True)
            owns_redis = True
        except Exception:
            r = None

    solver_bin = cli_bin or settings.SOLVER_CLI_BIN
    proc: subprocess.Popen[str] | None = None

    try:
        with sf() as db:
            job = db.get(OptimizationJob, uuid.UUID(job_id))
            if job is None:
                return {"status": "FAILED", "error": "Job not found"}
            if job.status == "CANCELLED":
                return {"status": "CANCELLED"}

            job.status = "RUNNING"
            job.started_at = datetime.now(timezone.utc)
            job.progress_ratio = 0.02
            job.progress_message = "Preparando snapshot JSON para otimização..."
            db.commit()
            _publish_event(
                r,
                job_id,
                {"type": "progress", "status": "RUNNING", "ratio": 0.02, "message": job.progress_message},
            )

            project_data = export_project_json_sync(db, job.project_id, job.config_snapshot)
            cfg = project_data.get("config", {})
            algo = job.algorithm or cfg.get("algoritmo", "CP_SAT")

        with tempfile.TemporaryDirectory(prefix="esc_job_") as tmpdir:
            in_path = Path(tmpdir) / "input.json"
            out_path = Path(tmpdir) / "output.json"
            in_path.write_text(json.dumps(project_data, ensure_ascii=False), encoding="utf-8")

            cmd = [
                solver_bin,
                "--input",
                str(in_path),
                "--output",
                str(out_path),
                "--progress-jsonl",
                "--algorithm",
                str(algo),
            ]
            if not cfg.get("sem_limite_tempo", False) and cfg.get("tempo_limite_segundos"):
                cmd.extend(["--time-limit", str(cfg["tempo_limite_segundos"])])
            if not cfg.get("sem_limite_memoria", False) and cfg.get("max_memoria_mb"):
                cmd.extend(["--max-memory", str(cfg["max_memoria_mb"])])

            proc = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1,
            )

            logs: list[str] = []
            done_event: dict[str, Any] | None = None

            assert proc.stdout is not None
            for raw_line in proc.stdout:
                line = raw_line.strip()
                if not line:
                    continue
                try:
                    ev = json.loads(line)
                except json.JSONDecodeError:
                    logs.append(line)
                    continue

                ev_type = ev.get("type")
                if ev_type == "progress":
                    ratio = float(ev.get("ratio", 0.0))
                    msg = str(ev.get("message", ""))
                    logs.append(f"[{int(ratio * 100)}%] {msg}")
                    with sf() as db:
                        job = db.get(OptimizationJob, uuid.UUID(job_id))
                        if job and job.status == "CANCELLED":
                            proc.send_signal(signal.SIGTERM)
                            break
                        if job:
                            job.progress_ratio = ratio
                            job.progress_message = msg
                            job.logs = list(logs[-200:])
                            db.commit()
                    _publish_event(
                        r,
                        job_id,
                        {"type": "progress", "status": "RUNNING", "ratio": ratio, "message": msg},
                    )
                elif ev_type == "done":
                    done_event = ev
                    for diag in ev.get("diagnostics", []):
                        logs.append(str(diag))

            rc = proc.wait()
            stderr_txt = proc.stderr.read() if proc.stderr else ""
            if stderr_txt.strip():
                logs.append(stderr_txt.strip())

            with sf() as db:
                job = db.get(OptimizationJob, uuid.UUID(job_id))
                if job is None:
                    return {"status": "FAILED"}
                if job.status == "CANCELLED" or rc == 130:
                    job.status = "CANCELLED"
                    job.progress_message = "Otimização cancelada pelo usuário."
                    job.logs = list(logs[-300:])
                    job.finished_at = datetime.now(timezone.utc)
                    db.commit()
                    _publish_event(r, job_id, {"type": "done", "status": "CANCELLED", "message": job.progress_message})
                    return {"status": "CANCELLED"}

                if not out_path.exists():
                    job.status = "FAILED"
                    job.error_detail = f"Solver CLI finalizou com código {rc} sem gerar saída."
                    job.logs = list(logs[-300:])
                    job.finished_at = datetime.now(timezone.utc)
                    db.commit()
                    _publish_event(r, job_id, {"type": "done", "status": "FAILED", "message": job.error_detail})
                    return {"status": "FAILED"}

                out_data = json.loads(out_path.read_text(encoding="utf-8"))
                grade = out_data.get("grade", {})
                stats = grade.get("estatisticas", {})
                solver_status = stats.get("status", "UNKNOWN")

                if solver_status in ("OPTIMAL", "FEASIBLE"):
                    existing_vers = db.execute(
                        select(TimetableVersion).where(TimetableVersion.project_id == job.project_id)
                    ).scalars().all()
                    next_num = max((v.version_number for v in existing_vers), default=0) + 1
                    label = (job.config_snapshot or {}).get("label") or f"{algo} v{next_num}"
                    version = TimetableVersion(
                        project_id=job.project_id,
                        version_number=next_num,
                        label=label,
                        salas_teoricas_designadas=grade.get("salas_teoricas_designadas", {}),
                        estatisticas=stats,
                        mensagens_diagnostico=grade.get("mensagens_diagnostico", []),
                        created_by=job.created_by,
                    )
                    db.add(version)
                    db.flush()

                    for aloc in grade.get("alocacoes", []):
                        db.add(
                            AllocatedLessonModel(
                                version_id=version.id,
                                aula_id=aloc["aula_id"],
                                disciplina_id=aloc.get("disciplina_id", ""),
                                disciplina_nome=aloc.get("disciplina_nome", ""),
                                tipo=aloc.get("tipo", "TEORICA"),
                                dia=normalize_day(aloc.get("dia", "SEG")),
                                start_min=time_str_to_minutes(aloc["inicio"]),
                                end_min=time_str_to_minutes(aloc["fim"]),
                                professores=aloc.get("professores", []),
                                turmas=aloc.get("turmas", []),
                                salas=aloc.get("salas", []),
                                is_pinned=bool(aloc.get("is_pinned", False)),
                                regime=aloc.get("regime", "ANUAL"),
                                semestre=str(aloc.get("semestre", "AMBOS")),
                                semestre_alocado=int(aloc.get("semestre_alocado", 1)),
                            )
                        )

                    project = db.get(Project, job.project_id)
                    if project:
                        project.active_timetable_version_id = version.id

                    job.result_version_id = version.id
                    job.status = "COMPLETED"
                    job.progress_ratio = 1.0
                    job.progress_message = f"Concluído ({solver_status}): {stats.get('total_aulas_alocadas', 0)} aulas alocadas."
                elif solver_status in ("INFEASIBLE", "DADOS_INVALIDOS"):
                    job.status = "INFEASIBLE"
                    job.progress_ratio = 1.0
                    job.progress_message = f"Inviável ({solver_status}). Verifique os logs de diagnóstico."
                else:
                    job.status = "FAILED"
                    job.error_detail = f"Status inesperado do solver: {solver_status}"

                job.logs = list(logs[-300:])
                job.finished_at = datetime.now(timezone.utc)
                db.commit()

                _publish_event(
                    r,
                    job_id,
                    {
                        "type": "done",
                        "status": job.status,
                        "solver_status": solver_status,
                        "ratio": job.progress_ratio,
                        "message": job.progress_message,
                        "result_version_id": str(job.result_version_id) if job.result_version_id else None,
                        "diagnostics": grade.get("mensagens_diagnostico", []),
                    },
                )
                return {
                    "status": job.status,
                    "solver_status": solver_status,
                    "result_version_id": str(job.result_version_id) if job.result_version_id else None,
                }
    except SoftTimeLimitExceeded:
        if proc and proc.poll() is None:
            proc.send_signal(signal.SIGTERM)
            proc.wait(timeout=5)
        with sf() as db:
            job = db.get(OptimizationJob, uuid.UUID(job_id))
            if job:
                job.status = "CANCELLED"
                job.progress_message = "Tempo limite do worker atingido."
                job.finished_at = datetime.now(timezone.utc)
                db.commit()
        _publish_event(r, job_id, {"type": "done", "status": "CANCELLED", "message": "Timeout"})
        return {"status": "CANCELLED"}
    except Exception as exc:
        with sf() as db:
            job = db.get(OptimizationJob, uuid.UUID(job_id))
            if job:
                job.status = "FAILED"
                job.error_detail = str(exc)
                job.finished_at = datetime.now(timezone.utc)
                db.commit()
        _publish_event(r, job_id, {"type": "done", "status": "FAILED", "message": str(exc)})
        return {"status": "FAILED", "error": str(exc)}
    finally:
        if owns_redis and r is not None:
            try:
                r.close()
            except Exception:
                pass


@celery_app.task(name="app.workers.tasks.run_optimization_job", bind=True)
def run_optimization_job(self, job_id: str) -> dict[str, Any]:
    return execute_optimization_job_sync(job_id)
