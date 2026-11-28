import { describe, expect, it } from "vitest";
import {
  computeImplicitBreaks,
  formatDurationMs,
  formatDurationSeconds,
  hmsToSeconds,
  minToTime,
  secondsToHms,
  timeToMin,
} from "../time";
import { filterAndSortItems, normalizeText } from "../searchSort";
import { evaluateLessonAtSlot, evaluateLessonSwap, type DomainSnapshot } from "../conflictEngine";

describe("domain time & searchSort", () => {
  it("computes implicit breaks between non-contiguous slots", () => {
    const breaks = computeImplicitBreaks([
      { inicio: "08:00", fim: "08:50" },
      { inicio: "08:50", fim: "09:40" },
      { inicio: "09:55", fim: "10:40" },
    ]);
    expect(breaks).toEqual([{ start_min: timeToMin("09:40"), end_min: timeToMin("09:55") }]);
    expect(minToTime(580)).toBe("09:40");
  });

  it("strips accents and performs natural numeric sorting", () => {
    expect(normalizeText("Física Química")).toBe("fisica quimica");
    const items = [{ nome: "Sala 10" }, { nome: "Sala 2" }, { nome: "Sala 1" }];
    const sorted = filterAndSortItems(items, "", (x) => x.nome, (x) => x.nome, true);
    expect(sorted.map((x) => x.nome)).toEqual(["Sala 1", "Sala 2", "Sala 10"]);
  });
  it("formats durations and converts seconds to/from H/M/S", () => {
    expect(formatDurationSeconds(0)).toBe("00h 00m 00s");
    expect(formatDurationSeconds(30)).toBe("00h 00m 30s");
    expect(formatDurationSeconds(3723)).toBe("01h 02m 03s");

    expect(formatDurationMs(0)).toBe("00h 00m 00s");
    expect(formatDurationMs(450)).toBe("00h 00m 00.45s");
    expect(formatDurationMs(5)).toBe("00h 00m 00.01s");
    expect(formatDurationMs(995)).toBe("00h 00m 01s");
    expect(formatDurationMs(3723000)).toBe("01h 02m 03s");

    expect(secondsToHms(3665)).toEqual({ hours: 1, minutes: 1, seconds: 5 });
    expect(hmsToSeconds(1, 15, 20)).toBe(4520);
    expect(hmsToSeconds(0, 0, 0)).toBe(1);
  });
});

describe("domain conflictEngine", () => {
  const snap: DomainSnapshot = {
    professores: {
      P01: {
        id: "P01",
        code: "P01",
        nome: "Carlos",
        preferencia_dias: { SEG: 5 },
        disponibilidade: { SEG: [{ inicio: "08:00", fim: "10:40" }] },
        turnos_pretendidos: { SEG: ["MANHA"] },
        carga_maxima_diaria_minutos: 0,
        max_aulas_por_dia: 0,
        peso_janela: 8,
      },
    },
    salas: {
      S101: { id: "S101", code: "S101", nome: "Sala 101", tipo: "TEORICA", capacidade: 40, recursos: [] },
      LAB1: { id: "LAB1", code: "LAB1", nome: "Lab 1", tipo: "LABORATORIO", capacidade: 30, recursos: [] },
    },
    turmas: {
      T01: {
        id: "T01",
        code: "T01",
        nome: "1A",
        turno: "MANHA",
        dias_letivos: ["SEG"],
        jornada_padrao: {
          slots: [
            { inicio: "08:00", fim: "08:50" },
            { inicio: "08:50", fim: "09:40" },
            { inicio: "09:55", fim: "10:40" },
          ],
        },
        excecoes_por_dia: {},
        sala_teorica_id: "S101",
        salas_teoricas_permitidas: ["S101"],
      },
    },
    disciplinas: {
      MAT: {
        id: "MAT",
        code: "MAT",
        nome: "Matemática",
        tipo: "TEORICA",
        duracao_minutos: 50,
        aulas_semanais: 2,
        regime: "ANUAL",
        semestre: "AMBOS",
        professores: ["P01"],
        turmas: ["T01"],
        salas_alternativas: [],
        exigir_aulas_consecutivas: false,
        max_aulas_por_dia_turma: 2,
      },
    },
    config: { distribuir_disciplinas_na_semana: true },
    alocacoes: [
      {
        aula_id: "MAT_1",
        disciplina_id: "MAT",
        disciplina_nome: "Matemática",
        tipo: "TEORICA",
        dia: "SEG",
        inicio: "08:00",
        fim: "08:50",
        start_min: 480,
        end_min: 530,
        professores: ["P01"],
        turmas: ["T01"],
        salas: ["S101"],
        is_pinned: false,
        regime: "ANUAL",
        semestre: "AMBOS",
        semestre_alocado: 1,
      },
      {
        aula_id: "MAT_2",
        disciplina_id: "MAT",
        disciplina_nome: "Matemática 45",
        tipo: "LABORATORIO",
        dia: "SEG",
        inicio: "09:55",
        fim: "10:40",
        start_min: 595,
        end_min: 640,
        professores: ["P01"],
        turmas: ["T01"],
        salas: ["LAB1"],
        is_pinned: false,
        regime: "ANUAL",
        semestre: "AMBOS",
        semestre_alocado: 1,
      },
    ],
  };

  it("detects break overlap, inactive day, and duration mismatch on swap", () => {
    const brk = evaluateLessonAtSlot(snap, "MAT_1", "SEG", 580, 630);
    expect(brk.some((c) => c.type === "INTERVALO_RECREIO")).toBe(true);

    const inactive = evaluateLessonAtSlot(snap, "MAT_1", "TER", 480, 530);
    expect(inactive.some((c) => c.type === "TURMA_INATIVA")).toBe(true);

    const swap = evaluateLessonSwap(snap, "MAT_1", "MAT_2");
    expect(swap.some((c) => c.type === "DURACAO_INCOMPATIVEL")).toBe(true);
  });

  it("enforces per-discipline max_aulas_por_dia_turma, treats 0 as unlimited, and respects R10 toggle", () => {
    const snapLimit1: DomainSnapshot = {
      ...snap,
      disciplinas: {
        MAT: { ...snap.disciplinas.MAT, max_aulas_por_dia_turma: 1 },
      },
    };
    const c1 = evaluateLessonAtSlot(snapLimit1, "MAT_2", "SEG", 530, 580);
    expect(c1.some((c) => c.type === "LIMITE_DIARIO_DISCIPLINA")).toBe(true);

    const snapUnlimited: DomainSnapshot = {
      ...snap,
      disciplinas: {
        MAT: { ...snap.disciplinas.MAT, max_aulas_por_dia_turma: 0 },
      },
    };
    const c0 = evaluateLessonAtSlot(snapUnlimited, "MAT_2", "SEG", 530, 580);
    expect(c0.some((c) => c.type === "LIMITE_DIARIO_DISCIPLINA")).toBe(false);

    const snapDisabled: DomainSnapshot = {
      ...snapLimit1,
      config: { distribuir_disciplinas_na_semana: false },
    };
    const cOff = evaluateLessonAtSlot(snapDisabled, "MAT_2", "SEG", 530, 580);
    expect(cOff.some((c) => c.type === "LIMITE_DIARIO_DISCIPLINA")).toBe(false);
  });
});
