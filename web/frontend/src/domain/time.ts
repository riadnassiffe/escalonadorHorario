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
    { inicio: "08:00", fim: "08:45" },
    { inicio: "08:45", fim: "09:30" },
    { inicio: "09:45", fim: "10:30" },
    { inicio: "10:30", fim: "11:15" },
    { inicio: "11:15", fim: "12:00" },
  ],
  TARDE: [
    { inicio: "13:15", fim: "14:00" },
    { inicio: "14:00", fim: "14:45" },
    { inicio: "14:45", fim: "15:30" },
    { inicio: "15:45", fim: "16:30" },
    { inicio: "16:30", fim: "17:15" },
  ],
  NOITE: [
    { inicio: "18:30", fim: "19:20" },
    { inicio: "19:20", fim: "20:10" },
    { inicio: "20:20", fim: "21:10" },
    { inicio: "21:10", fim: "22:00" },
  ],
  INTEGRAL: [
    { inicio: "08:00", fim: "08:45" },
    { inicio: "08:45", fim: "09:30" },
    { inicio: "09:45", fim: "10:30" },
    { inicio: "10:30", fim: "11:15" },
    { inicio: "11:15", fim: "12:00" },
    { inicio: "13:15", fim: "14:00" },
    { inicio: "14:00", fim: "14:45" },
    { inicio: "14:45", fim: "15:30" },
    { inicio: "15:45", fim: "16:30" },
    { inicio: "16:30", fim: "17:15" },
  ],
};
