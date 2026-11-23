from __future__ import annotations

import io
import csv
from typing import Any

from jinja2 import Template

from app.services.conflict_engine import ProjectSnapshot
from app.services.time_utils import ALL_DAYS, DAY_LONG_NAMES, compute_implicit_breaks, minutes_to_time_str, time_str_to_minutes


def _regime_semestre_label(aloc: dict[str, Any]) -> str:
    if aloc.get("regime") == "ANUAL":
        return "Anual"
    sem = aloc.get("semestre")
    sem_aloc = aloc.get("semestre_alocado", 1)
    if sem == "1" or sem_aloc == 1:
        return "Semestral (1º Sem)"
    if sem == "2" or sem_aloc == 2:
        return "Semestral (2º Sem)"
    return "Semestral"


def export_csv_report(snap: ProjectSnapshot) -> str:
    buf = io.StringIO()
    writer = csv.writer(buf, delimiter=";", lineterminator="\n")
    writer.writerow([
        "Aula_ID",
        "Disciplina",
        "Tipo",
        "Regime",
        "Semestre",
        "Dia",
        "Inicio",
        "Fim",
        "Duracao_Min",
        "Professores",
        "Turmas",
        "Salas",
        "Fixada",
    ])
    for a in snap.alocacoes:
        sem_aloc = a.get("semestre_alocado", 1)
        sem_label = "Ano Todo" if sem_aloc == 3 else ("2º Sem" if sem_aloc == 2 else "1º Sem")
        profs = ", ".join(snap.professores.get(p, {}).get("nome", p) for p in a.get("professores", []))
        turmas = ", ".join(snap.turmas.get(t, {}).get("nome", t) for t in a.get("turmas", []))
        salas = ", ".join(snap.salas.get(s, {}).get("nome", s) for s in a.get("salas", []))
        dur = int(a["end_min"]) - int(a["start_min"])
        writer.writerow([
            a["aula_id"],
            a["disciplina_nome"],
            a.get("tipo", "TEORICA"),
            _regime_semestre_label(a),
            sem_label,
            a["dia"],
            minutes_to_time_str(a["start_min"]),
            minutes_to_time_str(a["end_min"]),
            dur,
            profs,
            turmas,
            salas,
            "SIM" if a.get("is_pinned") else "NAO",
        ])
    return buf.getvalue()


HTML_TEMPLATE = Template("""<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Grade Horária - {{ project_name }}</title>
<style>
  :root {
    --bg: #f8fafc; --card: #ffffff; --border: #cbd5e1; --text: #0f172a;
    --muted: #475569; --accent: #2563eb; --break-bg: #fef3c7; --break-text: #92400e;
  }
  * { box-sizing: border-box; }
  body { font-family: system-ui, -apple-system, sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 24px; }
  .header { margin-bottom: 24px; }
  .kpis { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px; margin-bottom: 24px; }
  .kpi { background: var(--card); border: 1px solid var(--border); border-radius: 10px; padding: 12px 16px; }
  .kpi-label { font-size: 12px; color: var(--muted); text-transform: uppercase; }
  .kpi-val { font-size: 20px; font-weight: 700; margin-top: 4px; }
  .section-card { background: var(--card); border: 1px solid var(--border); border-radius: 12px; padding: 18px; margin-bottom: 24px; break-inside: avoid; }
  h2 { margin: 0 0 6px; font-size: 18px; }
  .subtitle { margin: 0 0 12px; color: var(--muted); font-size: 13px; }
  table.timetable { width: 100%; border-collapse: collapse; font-size: 12px; }
  table.timetable th, table.timetable td { border: 1px solid var(--border); padding: 6px 8px; vertical-align: top; text-align: left; }
  table.timetable th { background: #e2e8f0; font-weight: 600; text-align: center; }
  .time-col { width: 105px; font-weight: 600; text-align: center !important; background: #f1f5f9; white-space: nowrap; }
  .break-row td { background: var(--break-bg); color: var(--break-text); text-align: center !important; font-weight: 600; }
  .lesson-card { border-left: 4px solid var(--accent); background: #eff6ff; padding: 4px 6px; border-radius: 4px; margin-bottom: 4px; }
  .lesson-card.lab { border-left-color: #db2777; background: #fdf2f8; }
  .lesson-card.esp { border-left-color: #7c3aed; background: #f5f3ff; }
  .badge { display: inline-block; font-size: 10px; padding: 1px 5px; border-radius: 999px; background: #e2e8f0; margin-top: 2px; }
  @media print {
    body { background: #fff; padding: 8px; }
    .section-card { box-shadow: none; page-break-inside: avoid; }
  }
</style>
</head>
<body>
  <div class="header">
    <h1>{{ project_name }}</h1>
    <p class="subtitle">Relatório Gerado pelo Escalonador Inteligente de Horários</p>
  </div>
  <div class="kpis">
    <div class="kpi"><div class="kpi-label">Status</div><div class="kpi-val">{{ stats.get("status", "NAO_EXECUTADO") }}</div></div>
    <div class="kpi"><div class="kpi-label">Tempo</div><div class="kpi-val">{{ "%.1f"|format(stats.get("tempo_execucao_ms", 0)) }} ms</div></div>
    <div class="kpi"><div class="kpi-label">Aulas Alocadas</div><div class="kpi-val">{{ stats.get("total_aulas_alocadas", 0) }} / {{ stats.get("total_aulas_esperadas", 0) }}</div></div>
    <div class="kpi"><div class="kpi-label">Janelas Profs (J)</div><div class="kpi-val">{{ stats.get("penalidade_janelas_J", 0) }} min</div></div>
    <div class="kpi"><div class="kpi-label">Dias Trabalhados (D)</div><div class="kpi-val">{{ stats.get("penalidade_dias_D", 0) }}</div></div>
    <div class="kpi"><div class="kpi-label">Lacunas Turmas (JG)</div><div class="kpi-val">{{ stats.get("penalidade_lacunas_turma_JG", 0) }} min</div></div>
  </div>

  {% for section in sections %}
  <div class="section-card">
    <h2>{{ section.title }}</h2>
    <p class="subtitle">{{ section.subtitle }}</p>
    <table class="timetable">
      <thead>
        <tr>
          <th>Horário</th>
          {% for d in days %}<th>{{ day_names[d] }}</th>{% endfor %}
        </tr>
      </thead>
      <tbody>
        {% for row in section.rows %}
        <tr class="{{ 'break-row' if row.is_break else '' }}">
          <td class="time-col">{{ row.label }}</td>
          {% if row.is_break %}
            <td colspan="5">☕ INTERVALO</td>
          {% else %}
            {% for d in days %}
            <td>
              {% for card in row.cells[d] %}
              <div class="lesson-card {{ 'lab' if card.tipo == 'LABORATORIO' else ('esp' if card.tipo == 'ESPECIAL' else '') }}">
                <strong>{{ card.disciplina_nome }}</strong>{% if card.is_pinned %} 📌{% endif %}<br>
                <span>{{ card.meta }}</span><br>
                <span class="badge">{{ card.regime_label }}</span>
              </div>
              {% endfor %}
            </td>
            {% endfor %}
          {% endif %}
        </tr>
        {% endfor %}
      </tbody>
    </table>
  </div>
  {% endfor %}
</body>
</html>""")


def export_html_report(
    project_name: str,
    snap: ProjectSnapshot,
    stats: dict[str, Any],
    salas_designadas: dict[str, str],
) -> str:
    sections = []

    # 1. Turmas
    for t_code, t in snap.turmas.items():
        intervals_set: set[tuple[int, int]] = set()
        break_set: set[tuple[int, int]] = set()
        for a in snap.alocacoes:
            if t_code in a.get("turmas", []):
                intervals_set.add((int(a["start_min"]), int(a["end_min"])))
        for d in t.get("dias_letivos", ALL_DAYS):
            exc = (t.get("excecoes_por_dia") or {}).get(d)
            slots = exc.get("slots", []) if exc else (t.get("jornada_padrao") or {}).get("slots", [])
            for s in slots:
                intervals_set.add((time_str_to_minutes(s["inicio"]), time_str_to_minutes(s["fim"])))
            for br in compute_implicit_breaks(slots):
                intervals_set.add(br)
                break_set.add(br)

        rows = []
        for s_m, e_m in sorted(intervals_set):
            is_brk = (s_m, e_m) in break_set and not any(
                t_code in a.get("turmas", []) and int(a["start_min"]) == s_m and int(a["end_min"]) == e_m
                for a in snap.alocacoes
            )
            cells = {d: [] for d in ALL_DAYS}
            if not is_brk:
                for a in snap.alocacoes:
                    if t_code in a.get("turmas", []) and int(a["start_min"]) == s_m and int(a["end_min"]) == e_m:
                        profs = ", ".join(snap.professores.get(p, {}).get("nome", p) for p in a.get("professores", []))
                        salas = ", ".join(snap.salas.get(s, {}).get("nome", s) for s in a.get("salas", []))
                        cells[a["dia"]].append({
                            "disciplina_nome": a["disciplina_nome"],
                            "tipo": a.get("tipo", "TEORICA"),
                            "is_pinned": a.get("is_pinned", False),
                            "meta": f"{profs} | {salas}",
                            "regime_label": _regime_semestre_label(a),
                        })
            rows.append({
                "label": f"{minutes_to_time_str(s_m)} - {minutes_to_time_str(e_m)}",
                "is_break": is_brk,
                "cells": cells,
            })

        sala_teo = t.get("sala_teorica_id") or salas_designadas.get(t_code, "Auto")
        sections.append({
            "title": f"Turma: {t['nome']} ({t_code})",
            "subtitle": f"Turno: {t.get('turno', 'MANHA')} | Sala Teórica Fixa: {sala_teo}",
            "rows": rows,
        })

    # 2. Professores
    for p_code, p in snap.professores.items():
        p_alocs = [a for a in snap.alocacoes if p_code in a.get("professores", [])]
        intervals_set = {(int(a["start_min"]), int(a["end_min"])) for a in p_alocs}
        rows = []
        for s_m, e_m in sorted(intervals_set):
            cells = {d: [] for d in ALL_DAYS}
            for a in p_alocs:
                if int(a["start_min"]) == s_m and int(a["end_min"]) == e_m:
                    turmas = ", ".join(snap.turmas.get(t, {}).get("nome", t) for t in a.get("turmas", []))
                    salas = ", ".join(snap.salas.get(s, {}).get("nome", s) for s in a.get("salas", []))
                    cells[a["dia"]].append({
                        "disciplina_nome": a["disciplina_nome"],
                        "tipo": a.get("tipo", "TEORICA"),
                        "is_pinned": a.get("is_pinned", False),
                        "meta": f"{turmas} | {salas}",
                        "regime_label": _regime_semestre_label(a),
                    })
            rows.append({
                "label": f"{minutes_to_time_str(s_m)} - {minutes_to_time_str(e_m)}",
                "is_break": False,
                "cells": cells,
            })
        sections.append({
            "title": f"Professor: {p['nome']} ({p_code})",
            "subtitle": f"Total de aulas alocadas: {len(p_alocs)}",
            "rows": rows,
        })

    # 3. Salas
    for r_code, r in snap.salas.items():
        r_alocs = [a for a in snap.alocacoes if r_code in a.get("salas", [])]
        intervals_set = {(int(a["start_min"]), int(a["end_min"])) for a in r_alocs}
        rows = []
        for s_m, e_m in sorted(intervals_set):
            cells = {d: [] for d in ALL_DAYS}
            for a in r_alocs:
                if int(a["start_min"]) == s_m and int(a["end_min"]) == e_m:
                    turmas = ", ".join(snap.turmas.get(t, {}).get("nome", t) for t in a.get("turmas", []))
                    profs = ", ".join(snap.professores.get(p, {}).get("nome", p) for p in a.get("professores", []))
                    cells[a["dia"]].append({
                        "disciplina_nome": a["disciplina_nome"],
                        "tipo": a.get("tipo", "TEORICA"),
                        "is_pinned": a.get("is_pinned", False),
                        "meta": f"{turmas} | {profs}",
                        "regime_label": _regime_semestre_label(a),
                    })
            rows.append({
                "label": f"{minutes_to_time_str(s_m)} - {minutes_to_time_str(e_m)}",
                "is_break": False,
                "cells": cells,
            })
        sections.append({
            "title": f"Sala: {r['nome']} ({r_code})",
            "subtitle": f"Tipo: {r.get('tipo', 'TEORICA')} | Capacidade: {r.get('capacidade', 40)}",
            "rows": rows,
        })

    return HTML_TEMPLATE.render(
        project_name=project_name,
        stats=stats or {},
        sections=sections,
        days=ALL_DAYS,
        day_names=DAY_LONG_NAMES,
    )
