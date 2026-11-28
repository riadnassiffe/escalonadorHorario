from __future__ import annotations

import math
import re

MINUTES_PER_DAY = 1440
ALL_DAYS = ["SEG", "TER", "QUA", "QUI", "SEX"]
DAY_LONG_NAMES = {
    "SEG": "Segunda-feira",
    "TER": "Terça-feira",
    "QUA": "Quarta-feira",
    "QUI": "Quinta-feira",
    "SEX": "Sexta-feira",
}


def time_str_to_minutes(time_str: str) -> int:
    parts = time_str.strip().split(":")
    if len(parts) != 2:
        raise ValueError(f"Formato de hora inválido: {time_str}")
    h = int(parts[0])
    m = int(parts[1])
    if h < 0 or h > 24 or m < 0 or m > 59 or (h == 24 and m > 0):
        raise ValueError(f"Horário fora do intervalo [00:00, 24:00]: {time_str}")
    return h * 60 + m


def minutes_to_time_str(minutes: int) -> str:
    minutes = max(0, min(MINUTES_PER_DAY, int(minutes)))
    h = minutes // 60
    m = minutes % 60
    return f"{h:02d}:{m:02d}"


def normalize_day(day: str) -> str:
    d = day.strip().upper()
    mapping = {
        "SEG": "SEG",
        "SEGUNDA": "SEG",
        "SEGUNDA-FEIRA": "SEG",
        "TER": "TER",
        "TERCA": "TER",
        "TERÇA": "TER",
        "TERCA-FEIRA": "TER",
        "TERÇA-FEIRA": "TER",
        "QUA": "QUA",
        "QUARTA": "QUA",
        "QUARTA-FEIRA": "QUA",
        "QUI": "QUI",
        "QUINTA": "QUI",
        "QUINTA-FEIRA": "QUI",
        "SEX": "SEX",
        "SEXTA": "SEX",
        "SEXTA-FEIRA": "SEX",
    }
    return mapping.get(d, "SEG")


def legacy_jornada_to_slots(jornada_dict: dict) -> dict:
    """Converts legacy {inicio, fim, intervalos} format into {slots: [{inicio, fim}]} if needed."""
    if "slots" in jornada_dict and isinstance(jornada_dict["slots"], list):
        slots = [
            {"inicio": s["inicio"], "fim": s["fim"]}
            for s in jornada_dict["slots"]
            if "inicio" in s and "fim" in s
        ]
        slots.sort(key=lambda s: (time_str_to_minutes(s["inicio"]), time_str_to_minutes(s["fim"])))
        return {"slots": slots}

    ini = time_str_to_minutes(jornada_dict.get("inicio", "08:00"))
    fim = time_str_to_minutes(jornada_dict.get("fim", "11:30"))
    intervals: list[tuple[int, int]] = []
    for item in jornada_dict.get("intervalos", []):
        if item.get("ativo", True):
            s = time_str_to_minutes(item.get("inicio", "08:40"))
            e = time_str_to_minutes(item.get("fim", "08:55"))
            if s < e:
                intervals.append((s, e))
    intervals.sort()

    blocks: list[tuple[int, int]] = []
    current = ini
    for b_s, b_e in intervals:
        if b_s > current and b_s < fim:
            blocks.append((current, min(b_s, fim)))
        current = max(current, b_e)
    if fim > current:
        blocks.append((current, fim))

    slots: list[dict[str, str]] = []
    dur = 50
    for b_s, b_e in blocks:
        cur = b_s
        while cur + dur <= b_e:
            slots.append({"inicio": minutes_to_time_str(cur), "fim": minutes_to_time_str(cur + dur)})
            cur += dur
        if cur < b_e and (b_e - cur) >= 30:
            slots.append({"inicio": minutes_to_time_str(cur), "fim": minutes_to_time_str(b_e)})
    return {"slots": slots}


def compute_implicit_breaks(slots: list[dict[str, str]]) -> list[tuple[int, int]]:
    ordered = sorted(
        [(time_str_to_minutes(s["inicio"]), time_str_to_minutes(s["fim"])) for s in slots],
        key=lambda x: (x[0], x[1]),
    )
    breaks: list[tuple[int, int]] = []
    for idx in range(len(ordered) - 1):
        if ordered[idx][1] < ordered[idx + 1][0]:
            breaks.append((ordered[idx][1], ordered[idx + 1][0]))
    return breaks


def next_sequential_code(existing_codes: list[str], prefix: str) -> str:
    lower_prefix = prefix.lower()
    max_num = 0
    pattern = re.compile(re.escape(lower_prefix) + r"(\d+)")
    for code in existing_codes:
        m = pattern.search(code.lower())
        if m:
            try:
                val = int(m.group(1))
                if val > max_num:
                    max_num = val
            except ValueError:
                pass
    n = max_num + 1
    existing_set = set(existing_codes)
    while True:
        cand = f"{prefix.upper()}{n:02d}"
        if cand not in existing_set:
            return cand
        n += 1


def format_duration_seconds(total_seconds: float) -> str:
    if not math.isfinite(total_seconds) or total_seconds <= 0:
        total = 0
    else:
        total = int(round(total_seconds))
        if total < 0:
            total = 0
    h = total // 3600
    m = (total % 3600) // 60
    s = total % 60
    return f"{h:02d}h {m:02d}m {s:02d}s"


def format_duration_ms(duration_ms: float) -> str:
    if not math.isfinite(duration_ms) or duration_ms <= 0:
        return "00h 00m 00s"
    if duration_ms < 995.0:
        hundredths = max(1, min(99, int(round(duration_ms / 10.0))))
        return f"00h 00m 00.{hundredths:02d}s"
    return format_duration_seconds(round(duration_ms / 1000.0))
