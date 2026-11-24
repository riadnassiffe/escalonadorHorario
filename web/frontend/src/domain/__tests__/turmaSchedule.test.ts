import { describe, expect, it } from "vitest";
import {
  generateBatchSlots,
  nextSuggestedSlot,
  replicateDaySchedule,
  summarizeTurmaSchedule,
  turmaToWeeklySchedule,
  validateDaySchedule,
  weeklyScheduleToTurmaFields,
} from "../turmaSchedule";
import type { Turma } from "../../types";

describe("domain turmaSchedule", () => {
  const baseTurma: Turma = {
    id: "T01",
    code: "T01",
    nome: "1º Ano A",
    turno: "MANHA",
    dias_letivos: ["SEG", "TER", "QUA", "QUI", "SEX"],
    jornada_padrao: {
      slots: [
        { inicio: "07:30", fim: "08:20" },
        { inicio: "08:20", fim: "09:10" },
      ],
    },
    excecoes_por_dia: {},
    sala_teorica_id: "S101",
    salas_teoricas_permitidas: ["S101"],
  };

  it("roundtrips uniform week without generating excecoes_por_dia", () => {
    const weekly = turmaToWeeklySchedule(baseTurma);
    const compacted = weeklyScheduleToTurmaFields(weekly, baseTurma.turno);
    expect(compacted.dias_letivos).toEqual(["SEG", "TER", "QUA", "QUI", "SEX"]);
    expect(compacted.jornada_padrao.slots).toEqual(baseTurma.jornada_padrao.slots);
    expect(compacted.excecoes_por_dia).toEqual({});
  });

  it("picks most frequent schedule as jornada_padrao and stores differing days in excecoes_por_dia", () => {
    const weekly = turmaToWeeklySchedule(baseTurma);
    weekly.TER = {
      active: true,
      slots: [
        { inicio: "07:30", fim: "08:20" },
        { inicio: "13:30", fim: "14:20" },
      ],
    };
    weekly.SEX = { active: false, slots: [] };

    const compacted = weeklyScheduleToTurmaFields(weekly, "MANHA");
    expect(compacted.dias_letivos).toEqual(["SEG", "TER", "QUA", "QUI"]);
    expect(compacted.jornada_padrao.slots).toEqual(baseTurma.jornada_padrao.slots);
    expect(Object.keys(compacted.excecoes_por_dia)).toEqual(["TER"]);
    expect(compacted.excecoes_por_dia.TER.slots).toEqual(weekly.TER.slots);
  });

  it("replicates day schedule to selected days without mutating source", () => {
    const weekly = turmaToWeeklySchedule(baseTurma);
    weekly.SEG = {
      active: true,
      slots: [{ inicio: "09:00", fim: "09:50" }],
    };

    const replicated = replicateDaySchedule(weekly, "SEG", ["QUA", "SEX"]);
    expect(replicated.QUA.slots).toEqual([{ inicio: "09:00", fim: "09:50" }]);
    expect(replicated.SEX.slots).toEqual([{ inicio: "09:00", fim: "09:50" }]);
    expect(replicated.TER.slots).toEqual(baseTurma.jornada_padrao.slots);

    // Mutate replicated QUA and ensure SEG is untouched
    replicated.QUA.slots[0].inicio = "10:00";
    expect(replicated.SEG.slots[0].inicio).toBe("09:00");
  });

  it("validates empty active days, start >= end, and overlapping slots", () => {
    expect(validateDaySchedule({ active: false, slots: [] }).valid).toBe(true);
    expect(
      validateDaySchedule({ active: true, slots: [{ inicio: "08:00", fim: "08:50" }] }).valid
    ).toBe(true);
    expect(validateDaySchedule({ active: true, slots: [] }).valid).toBe(false);
    expect(
      validateDaySchedule({ active: true, slots: [{ inicio: "09:00", fim: "08:50" }] }).valid
    ).toBe(false);
    expect(
      validateDaySchedule({
        active: true,
        slots: [
          { inicio: "08:00", fim: "08:50" },
          { inicio: "08:40", fim: "09:30" },
        ],
      }).valid
    ).toBe(false);
    expect(
      validateDaySchedule({
        active: true,
        slots: [{ inicio: "", fim: "08:50" }],
      }).valid
    ).toBe(false);
  });

  it("generates batch slots with optional break and suggests next slot", () => {
    const batch = generateBatchSlots({
      startTime: "07:30",
      count: 4,
      durationMin: 50,
      hasBreak: true,
      breakAfterSlot: 2,
      breakDurationMin: 15,
    });
    expect(batch).toEqual([
      { inicio: "07:30", fim: "08:20" },
      { inicio: "08:20", fim: "09:10" },
      { inicio: "09:25", fim: "10:15" },
      { inicio: "10:15", fim: "11:05" },
    ]);

    const noBreak = generateBatchSlots({
      startTime: "19:00",
      count: 2,
      durationMin: 45,
      hasBreak: false,
      breakAfterSlot: 1,
      breakDurationMin: 15,
    });
    expect(noBreak).toEqual([
      { inicio: "19:00", fim: "19:45" },
      { inicio: "19:45", fim: "20:30" },
    ]);

    expect(nextSuggestedSlot([])).toEqual({ inicio: "08:00", fim: "08:50" });
    expect(nextSuggestedSlot([{ inicio: "08:00", fim: "08:45" }])).toEqual({
      inicio: "08:45",
      fim: "09:30",
    });
  });

  it("summarizes weekly schedule accounting for custom days and inactive days", () => {
    const custom: Turma = {
      ...baseTurma,
      dias_letivos: ["SEG", "TER", "QUA"],
      excecoes_por_dia: {
        TER: {
          slots: [
            { inicio: "07:30", fim: "08:20" },
            { inicio: "08:20", fim: "09:10" },
            { inicio: "13:30", fim: "14:20" },
          ],
        },
      },
    };
    const summary = summarizeTurmaSchedule(custom);
    expect(summary).toEqual({
      activeDays: 3,
      totalSlots: 7,
      totalMinutes: 350,
      hasCustomDays: true,
    });
  });
});
