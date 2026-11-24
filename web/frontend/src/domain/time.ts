import type { DayOfWeek, TimeSlot } from "../types";

export const ALL_DAYS: DayOfWeek[] = ["SEG", "TER", "QUA", "QUI", "SEX"];

export const DAY_LABELS: Record<DayOfWeek, string> = {
  SEG: "Segunda-feira",
  TER: "Terça-feira",
  QUA: "Quarta-feira",
  QUI: "Quinta-feira",
  SEX: "Sexta-feira",
};

export const DAY_SHORT_LABELS: Record<DayOfWeek, string> = {
  SEG: "Seg",
  TER: "Ter",
  QUA: "Qua",
  QUI: "Qui",
  SEX: "Sex",
};

export function timeToMin(timeStr: string): number {
  const [hh, mm] = timeStr.split(":").map(Number);
  return (hh || 0) * 60 + (mm || 0);
}

export function minToTime(minutes: number): string {
  const clamped = Math.max(0, Math.min(1440, Math.round(minutes)));
  const hh = Math.floor(clamped / 60);
  const mm = clamped % 60;
  return `${String(hh).padStart(2, "0")}:${String(mm).padStart(2, "0")}`;
}

export function formatInterval(startMin: number, endMin: number): string {
  return `${minToTime(startMin)} - ${minToTime(endMin)}`;
}

export function computeImplicitBreaks(slots: TimeSlot[]): Array<{ start_min: number; end_min: number }> {
  const ordered = [...slots]
    .map((s) => ({ s: timeToMin(s.inicio), e: timeToMin(s.fim) }))
    .sort((a, b) => a.s - b.s || a.e - b.e);
  const breaks: Array<{ start_min: number; end_min: number }> = [];
  for (let i = 0; i + 1 < ordered.length; i++) {
    if (ordered[i].e < ordered[i + 1].s) {
      breaks.push({ start_min: ordered[i].e, end_min: ordered[i + 1].s });
    }
  }
  return breaks;
}

export function totalClassMinutes(slots: TimeSlot[]): number {
  return slots.reduce((acc, s) => acc + Math.max(0, timeToMin(s.fim) - timeToMin(s.inicio)), 0);
}

export const DEFAULT_SHIFT_SLOTS: Record<string, TimeSlot[]> = {
  MANHA: [
    { inicio: "07:30", fim: "08:20" },
    { inicio: "08:20", fim: "09:10" },
    { inicio: "09:25", fim: "10:15" },
    { inicio: "10:15", fim: "11:05" },
    { inicio: "11:05", fim: "11:55" },
  ],
  TARDE: [
    { inicio: "13:30", fim: "14:20" },
    { inicio: "14:20", fim: "15:10" },
    { inicio: "15:25", fim: "16:15" },
    { inicio: "16:15", fim: "17:05" },
    { inicio: "17:05", fim: "17:55" },
  ],
  NOITE: [
    { inicio: "19:00", fim: "19:50" },
    { inicio: "19:50", fim: "20:40" },
    { inicio: "20:55", fim: "21:45" },
    { inicio: "21:45", fim: "22:35" },
  ],
  INTEGRAL: [
    { inicio: "07:30", fim: "08:20" },
    { inicio: "08:20", fim: "09:10" },
    { inicio: "09:25", fim: "10:15" },
    { inicio: "10:15", fim: "11:05" },
    { inicio: "13:30", fim: "14:20" },
    { inicio: "14:20", fim: "15:10" },
    { inicio: "15:25", fim: "16:15" },
    { inicio: "16:15", fim: "17:05" },
  ],
};
