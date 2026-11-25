import { describe, expect, it } from "vitest";
import {
  computeDisciplinaRoomOptions,
  describeDisciplinaRooms,
  flattenSalasAlternativas,
  toggleRoomInSalasAlternativas,
} from "../disciplinaRooms";
import type { Disciplina, Room, Turma } from "../../types";

const SALAS: Room[] = [
  { id: "S101", code: "S101", nome: "Sala 101", tipo: "TEORICA", capacidade: 40, recursos: [] },
  { id: "S102", code: "S102", nome: "Sala 102", tipo: "TEORICA", capacidade: 40, recursos: [] },
  { id: "S201", code: "S201", nome: "Sala 201", tipo: "TEORICA", capacidade: 45, recursos: [] },
  { id: "LAB01", code: "LAB01", nome: "Lab Química", tipo: "LABORATORIO", capacidade: 30, recursos: [] },
  { id: "QUADRA", code: "QUADRA", nome: "Quadra Poliesportiva", tipo: "ESPECIAL", capacidade: 60, recursos: [] },
];

const makeTurma = (partial: Partial<Turma> & { code: string; nome: string }): Turma => ({
  id: partial.code,
  turno: "MANHA",
  dias_letivos: ["SEG"],
  jornada_padrao: { slots: [] },
  excecoes_por_dia: {},
  sala_teorica_id: null,
  salas_teoricas_permitidas: [],
  ...partial,
});

const TURMAS: Turma[] = [
  makeTurma({ code: "T01", nome: "1º Ano A", sala_teorica_id: "S101", salas_teoricas_permitidas: ["S101"] }),
  makeTurma({ code: "T02", nome: "2º Ano A", sala_teorica_id: null, salas_teoricas_permitidas: ["S102", "S201"] }),
  makeTurma({ code: "T03", nome: "3º Ano A", sala_teorica_id: null, salas_teoricas_permitidas: [] }),
];

describe("disciplinaRooms", () => {
  it("flattens and toggles salas_alternativas without duplicates", () => {
    expect(flattenSalasAlternativas([["LAB01"], ["S101", "LAB01"], [""]])).toEqual(["LAB01", "S101"]);

    const added = toggleRoomInSalasAlternativas([], "LAB01");
    expect(added).toEqual([["LAB01"]]);

    const addedSecond = toggleRoomInSalasAlternativas(added, "QUADRA");
    expect(addedSecond).toEqual([["LAB01"], ["QUADRA"]]);

    const removed = toggleRoomInSalasAlternativas(addedSecond, "LAB01");
    expect(removed).toEqual([["QUADRA"]]);
  });

  it("groups rooms for a turma with fixed room and separates labs and other theoretical rooms", () => {
    const res = computeDisciplinaRoomOptions(["T01"], TURMAS, SALAS, []);
    expect(res.turmaRooms).toHaveLength(1);
    expect(res.turmaRooms[0].room.code).toBe("S101");
    expect(res.turmaRooms[0].isFixed).toBe(true);
    expect(res.turmaRooms[0].turmaNames).toEqual(["1º Ano A"]);
    expect(res.labAndSpecialRooms.map((r) => r.code)).toEqual(["LAB01", "QUADRA"]);
    expect(res.otherTheoreticalRooms.map((r) => r.code)).toEqual(["S102", "S201"]);
    expect(res.hasSelectedOtherRoom).toBe(false);
    expect(res.turmaDefaultSummary).toContain("Sala 101 (S101)");
  });

  it("supports turma with multiple permitted theoretical rooms and detects selected other rooms", () => {
    const res = computeDisciplinaRoomOptions(["T02"], TURMAS, SALAS, ["S101"]);
    expect(res.turmaRooms.map((o) => `${o.room.code}:${o.isFixed}`)).toEqual(["S102:false", "S201:false"]);
    expect(res.otherTheoreticalRooms.map((r) => r.code)).toEqual(["S101"]);
    expect(res.hasSelectedOtherRoom).toBe(true);
  });

  it("merges rooms when multiple turmas are selected and falls back when none or empty", () => {
    const merged = computeDisciplinaRoomOptions(["T01", "T02"], TURMAS, SALAS, []);
    expect(merged.turmaRooms.map((o) => o.room.code)).toEqual(["S101", "S102", "S201"]);
    expect(merged.otherTheoreticalRooms).toEqual([]);

    const fallback = computeDisciplinaRoomOptions(["T03"], TURMAS, SALAS, []);
    expect(fallback.turmaRooms.map((o) => o.room.code)).toEqual(["S101", "S102", "S201"]);

    const noTurma = computeDisciplinaRoomOptions([], TURMAS, SALAS, []);
    expect(noTurma.turmaRooms).toEqual([]);
    expect(noTurma.labAndSpecialRooms.map((r) => r.code)).toEqual(["LAB01", "QUADRA"]);
    expect(noTurma.otherTheoreticalRooms.map((r) => r.code)).toEqual(["S101", "S102", "S201"]);
    expect(noTurma.turmaDefaultSummary).toContain("Selecione ao menos uma turma");
  });

  it("describes disciplina rooms concisely for the table", () => {
    const turmasByCode = Object.fromEntries(TURMAS.map((t) => [t.code, t]));
    const salasByCode = Object.fromEntries(SALAS.map((r) => [r.code, r]));

    const baseDisc: Disciplina = {
      id: "D1",
      code: "D1",
      nome: "Física",
      tipo: "TEORICA",
      duracao_minutos: 50,
      aulas_semanais: 3,
      regime: "ANUAL",
      semestre: "AMBOS",
      professores: ["P1"],
      turmas: ["T01"],
      salas_alternativas: [],
      exigir_aulas_consecutivas: false,
      max_aulas_por_dia_turma: 2,
    };

    expect(describeDisciplinaRooms(baseDisc, turmasByCode, salasByCode)).toEqual({
      label: "Turma: S101",
      isExplicit: false,
    });

    expect(
      describeDisciplinaRooms({ ...baseDisc, salas_alternativas: [["LAB01"]] }, turmasByCode, salasByCode)
    ).toEqual({
      label: "LAB01",
      isExplicit: true,
    });

    expect(
      describeDisciplinaRooms({ ...baseDisc, turmas: ["T02"] }, turmasByCode, salasByCode)
    ).toEqual({
      label: "Turma: Automática (2)",
      isExplicit: false,
    });

    expect(
      describeDisciplinaRooms({ ...baseDisc, tipo: "LABORATORIO" }, turmasByCode, salasByCode)
    ).toEqual({
      label: "Qualquer lab/especial",
      isExplicit: false,
    });
  });
});
