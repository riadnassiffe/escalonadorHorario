import type { DayOfWeek, JornadaTurma, TimeSlot, Turma } from "../types";
import { ALL_DAYS, DEFAULT_SHIFT_SLOTS, minToTime, timeToMin, totalClassMinutes } from "./time";

export interface DayScheduleState {
  active: boolean;
  slots: TimeSlot[];
}

export type WeeklyScheduleMap = Record<DayOfWeek, DayScheduleState>;

export interface DayValidationResult {
  valid: boolean;
  error?: string;
}

export interface BatchSlotParams {
  startTime: string;
  count: number;
  durationMin: number;
  hasBreak: boolean;
  breakAfterSlot: number;
  breakDurationMin: number;
}

const TIME_PATTERN = /^\d{2}:\d{2}$/;

function isValidTimeString(value: string): boolean {
  if (!TIME_PATTERN.test(value)) return false;
  const [hh, mm] = value.split(":").map(Number);
  if (Number.isNaN(hh) || Number.isNaN(mm)) return false;
  if (hh < 0 || hh > 24 || mm < 0 || mm > 59) return false;
  if (hh === 24 && mm > 0) return false;
  return true;
}

/**
 * Returns a new array of slots sorted chronologically by start and end time.
 */
export function sortTimeSlots(slots: TimeSlot[]): TimeSlot[] {
  return [...slots]
    .map((s) => ({ inicio: s.inicio, fim: s.fim }))
    .sort((a, b) => timeToMin(a.inicio) - timeToMin(b.inicio) || timeToMin(a.fim) - timeToMin(b.fim));
}

/**
 * Expands a Turma's dias_letivos, jornada_padrao, and excecoes_por_dia into a full Seg-Sex map.
 */
export function turmaToWeeklySchedule(turma: Turma): WeeklyScheduleMap {
  const result = {} as WeeklyScheduleMap;
  for (const d of ALL_DAYS) {
    const active = turma.dias_letivos.includes(d);
    const rawSlots = turma.excecoes_por_dia?.[d]?.slots ?? turma.jornada_padrao?.slots ?? [];
    result[d] = {
      active,
      slots: structuredClone(rawSlots),
    };
  }
  return result;
}

/**
 * Validates a single day's schedule (inactive days are always valid; active days require >= 1 non-overlapping slot).
 */
export function validateDaySchedule(day: DayScheduleState): DayValidationResult {
  if (!day.active) {
    return { valid: true };
  }
  if (day.slots.length === 0) {
    return { valid: false, error: "Adicione ao menos 1 slot para este dia letivo." };
  }

  for (const s of day.slots) {
    if (!isValidTimeString(s.inicio) || !isValidTimeString(s.fim) || timeToMin(s.inicio) >= timeToMin(s.fim)) {
      return { valid: false, error: "Cada slot deve ter Início menor que Fim." };
    }
  }

  const ordered = sortTimeSlots(day.slots);
  for (let i = 0; i + 1 < ordered.length; i++) {
    if (timeToMin(ordered[i].fim) > timeToMin(ordered[i + 1].inicio)) {
      return { valid: false, error: "Há sobreposição entre slots neste dia." };
    }
  }

  return { valid: true };
}

/**
 * Compacts a weekly schedule map back into dias_letivos, jornada_padrao (most frequent active schedule),
 * and excecoes_por_dia (active days differing from jornada_padrao).
 */
export function weeklyScheduleToTurmaFields(
  weekly: WeeklyScheduleMap,
  fallbackShift = "MANHA"
): {
  dias_letivos: DayOfWeek[];
  jornada_padrao: JornadaTurma;
  excecoes_por_dia: Record<string, JornadaTurma>;
} {
  const activeEntries: Array<{ day: DayOfWeek; slots: TimeSlot[]; key: string }> = [];

  for (const d of ALL_DAYS) {
    const entry = weekly[d];
    if (entry?.active) {
      const sorted = sortTimeSlots(entry.slots);
      activeEntries.push({
        day: d,
        slots: sorted,
        key: JSON.stringify(sorted),
      });
    }
  }

  if (activeEntries.length === 0) {
    return {
      dias_letivos: [],
      jornada_padrao: {
        slots: structuredClone(DEFAULT_SHIFT_SLOTS[fallbackShift] || DEFAULT_SHIFT_SLOTS.MANHA),
      },
      excecoes_por_dia: {},
    };
  }

  const counts = new Map<string, { count: number; slots: TimeSlot[] }>();
  for (const item of activeEntries) {
    const existing = counts.get(item.key);
    if (existing) {
      existing.count += 1;
    } else {
      counts.set(item.key, { count: 1, slots: item.slots });
    }
  }

  let bestKey = activeEntries[0].key;
  let bestCount = -1;
  let bestSlots = activeEntries[0].slots;
  for (const [key, val] of counts.entries()) {
    if (val.count > bestCount) {
      bestCount = val.count;
      bestKey = key;
      bestSlots = val.slots;
    }
  }

  const excecoes_por_dia: Record<string, JornadaTurma> = {};
  for (const item of activeEntries) {
    if (item.key !== bestKey) {
      excecoes_por_dia[item.day] = { slots: structuredClone(item.slots) };
    }
  }

  return {
    dias_letivos: activeEntries.map((item) => item.day),
    jornada_padrao: { slots: structuredClone(bestSlots) },
    excecoes_por_dia,
  };
}

/**
 * Replicates the active flag and slots from sourceDay to all selected targetDays without mutating the input map.
 */
export function replicateDaySchedule(
  weekly: WeeklyScheduleMap,
  sourceDay: DayOfWeek,
  targetDays: DayOfWeek[]
): WeeklyScheduleMap {
  const next = structuredClone(weekly);
  const src = weekly[sourceDay];
  for (const d of targetDays) {
    if (d !== sourceDay) {
      next[d] = {
        active: src.active,
        slots: structuredClone(src.slots),
      };
    }
  }
  return next;
}

/**
 * Suggests the next slot immediately following the last slot with matching duration.
 */
export function nextSuggestedSlot(slots: TimeSlot[]): TimeSlot {
  if (slots.length === 0) {
    return { inicio: "08:00", fim: "08:50" };
  }
  const last = slots[slots.length - 1];
  const lastStart = isValidTimeString(last.inicio) ? timeToMin(last.inicio) : 480;
  const lastEnd = isValidTimeString(last.fim) ? timeToMin(last.fim) : 530;
  const dur = lastEnd > lastStart ? lastEnd - lastStart : 50;

  const start = Math.min(lastEnd, 23 * 60 + 50);
  const end = Math.min(start + dur, 24 * 60);
  if (end <= start) {
    return { inicio: "08:00", fim: "08:50" };
  }
  return { inicio: minToTime(start), fim: minToTime(end) };
}

/**
 * Generates sequential slots in batch, optionally inserting a break after a specified slot index.
 */
export function generateBatchSlots(params: BatchSlotParams): TimeSlot[] {
  const { startTime, count, durationMin, hasBreak, breakAfterSlot, breakDurationMin } = params;
  if (!isValidTimeString(startTime) || count <= 0 || durationMin <= 0) {
    return [];
  }

  const generated: TimeSlot[] = [];
  let cur = timeToMin(startTime);

  for (let i = 1; i <= count; i++) {
    const next = cur + durationMin;
    if (next > 1440 || next <= cur) break;
    generated.push({ inicio: minToTime(cur), fim: minToTime(next) });
    cur = next;
    if (hasBreak && i === breakAfterSlot) {
      cur += Math.max(0, breakDurationMin);
    }
  }

  return generated;
}

/**
 * Computes weekly totals and whether a Turma has day-specific schedule overrides.
 */
export function summarizeTurmaSchedule(turma: Turma): {
  activeDays: number;
  totalSlots: number;
  totalMinutes: number;
  hasCustomDays: boolean;
} {
  let activeDays = 0;
  let totalSlots = 0;
  let totalMinutes = 0;

  for (const d of ALL_DAYS) {
    if (!turma.dias_letivos.includes(d)) continue;
    activeDays += 1;
    const slots = turma.excecoes_por_dia?.[d]?.slots ?? turma.jornada_padrao?.slots ?? [];
    totalSlots += slots.length;
    totalMinutes += totalClassMinutes(slots);
  }

  const hasCustomDays = Object.keys(turma.excecoes_por_dia || {}).some((d) =>
    turma.dias_letivos.includes(d)
  );

  return { activeDays, totalSlots, totalMinutes, hasCustomDays };
}
