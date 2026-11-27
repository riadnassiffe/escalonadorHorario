import { describe, expect, it } from "vitest";
import {
  computeDisciplinaRoomOptions,
  describeDisciplinaRooms,
  filterUnallocatedByViewContext,
  flattenSalasAlternativas,
  getAllowedRoomsForLesson,
  toggleRoomInSalasAlternativas,
} from "../disciplinaRooms";
import type { Disciplina, Room, Turma, UnallocatedLesson } from "../../types";

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

  it("filters unallocated lessons by active timetable view context", () => {
    const lessons: UnallocatedLesson[] = [
      {
        aula_id: "MAT_1",
        disciplina_id: "MAT",
        disciplina_nome: "Matemática",
        tipo: "TEORICA",
        duracao_minutos: 50,
        regime: "ANUAL",
        semestre: "AMBOS",
        professores: ["P01"],
        turmas: ["T01"],
      },
      {
        aula_id: "EDF_1",
        disciplina_id: "EDF",
        disciplina_nome: "Educação Física",
        tipo: "ESPECIAL",
        duracao_minutos: 50,
        regime: "ANUAL",
        semestre: "AMBOS",
        professores: ["P02"],
        turmas: ["T01", "T02"],
      },
      {
        aula_id: "QUI_1",
        disciplina_id: "QUI",
        disciplina_nome: "Química",
        tipo: "LABORATORIO",
        duracao_minutos: 50,
        regime: "ANUAL",
        semestre: "AMBOS",
        professores: ["P02"],
        turmas: ["T02"],
      },
    ];

    expect(
      filterUnallocatedByViewContext(lessons, { viewType: "TURMA", targetId: "T01" }).map((l) => l.aula_id)
    ).toEqual(["MAT_1", "EDF_1"]);

    expect(
      filterUnallocatedByViewContext(lessons, { viewType: "PROFESSOR", targetId: "P02" }).map((l) => l.aula_id)
    ).toEqual(["EDF_1", "QUI_1"]);

    expect(
      filterUnallocatedByViewContext(lessons, { viewType: "GERAL", targetId: "T02" }).map((l) => l.aula_id)
    ).toEqual(["EDF_1", "QUI_1"]);

    expect(
      filterUnallocatedByViewContext(lessons, { viewType: "GERAL", targetId: "" }).map((l) => l.aula_id)
    ).toEqual(["MAT_1", "EDF_1", "QUI_1"]);

    expect(
      filterUnallocatedByViewContext(lessons, { viewType: "SALA", targetId: "S101" }).map((l) => l.aula_id)
    ).toEqual(["MAT_1", "EDF_1", "QUI_1"]);
  });

  it("resolves allowed rooms for a lesson according to explicit, turma, and type rules", () => {
    const turmasByCode = Object.fromEntries(TURMAS.map((t) => [t.code, t]));
    const salasByCode = Object.fromEntries(SALAS.map((r) => [r.code, r]));

    const makeDisc = (partial: Partial<Disciplina> & { code: string; nome: string }): Disciplina => ({
      id: partial.code,
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
      ...partial,
    });

    const disciplinasByCode: Record<string, Disciplina> = {
      D_FIXED: makeDisc({ code: "D_FIXED", nome: "Português", turmas: ["T01"] }),
      D_PERM: makeDisc({ code: "D_PERM", nome: "História", turmas: ["T02"] }),
      D_FALLBACK: makeDisc({ code: "D_FALLBACK", nome: "Geografia", turmas: ["T03"] }),
      D_EXPLICIT: makeDisc({
        code: "D_EXPLICIT",
        nome: "Robótica",
        tipo: "TEORICA",
        turmas: ["T01"],
        salas_alternativas: [["LAB01"], ["LAB01", "QUADRA"], ["INEXISTENTE"]],
      }),
      D_LAB: makeDisc({ code: "D_LAB", nome: "Química Lab", tipo: "LABORATORIO", turmas: ["T01"] }),
      D_ESP: makeDisc({ code: "D_ESP", nome: "Ed. Física", tipo: "ESPECIAL", turmas: ["T01"] }),
    };

    // 1. Explicit rooms on disciplina override turma fixed room and support multi-room combos
    const explicitRes = getAllowedRoomsForLesson(
      { disciplina_id: "D_EXPLICIT", tipo: "TEORICA", turmas: ["T01"] },
      disciplinasByCode,
      turmasByCode,
      salasByCode
    );
    expect(explicitRes.source).toBe("EXPLICIT_DISCIPLINA");
    expect(explicitRes.options.map((o) => o.salas)).toEqual([["LAB01"], ["LAB01", "QUADRA"]]);

    // 2. Theoretical with fixed room on turma
    const fixedRes = getAllowedRoomsForLesson(
      { disciplina_id: "D_FIXED", tipo: "TEORICA", turmas: ["T01"] },
      disciplinasByCode,
      turmasByCode,
      salasByCode
    );
    expect(fixedRes.source).toBe("TURMA_FIXED");
    expect(fixedRes.options.map((o) => o.salas)).toEqual([["S101"]]);

    // 3. Theoretical with permitted rooms and designated room suggestion
    const permRes = getAllowedRoomsForLesson(
      { disciplina_id: "D_PERM", tipo: "TEORICA", turmas: ["T02"] },
      disciplinasByCode,
      turmasByCode,
      salasByCode,
      { T02: "S201" }
    );
    expect(permRes.source).toBe("TURMA_PERMITTED");
    expect(permRes.options.map((o) => `${o.key}:${Boolean(o.isSuggested)}`)).toEqual([
      "S102:false",
      "S201:true",
    ]);

    // 4. Theoretical without turma room restriction falls back to all theoretical rooms only
    const fallbackRes = getAllowedRoomsForLesson(
      { disciplina_id: "D_FALLBACK", tipo: "TEORICA", turmas: ["T03"] },
      disciplinasByCode,
      turmasByCode,
      salasByCode
    );
    expect(fallbackRes.source).toBe("ALL_THEORETICAL_FALLBACK");
    expect(fallbackRes.options.map((o) => o.key)).toEqual(["S101", "S102", "S201"]);

    // 5. Lab and Special filter out theoretical rooms and order exact type first
    const labRes = getAllowedRoomsForLesson(
      { disciplina_id: "D_LAB", tipo: "LABORATORIO", turmas: ["T01"] },
      disciplinasByCode,
      turmasByCode,
      salasByCode
    );
    expect(labRes.source).toBe("LAB_OR_SPECIAL");
    expect(labRes.options.map((o) => o.key)).toEqual(["LAB01", "QUADRA"]);

    const espRes = getAllowedRoomsForLesson(
      { disciplina_id: "D_ESP", tipo: "ESPECIAL", turmas: ["T01"] },
      disciplinasByCode,
      turmasByCode,
      salasByCode
    );
    expect(espRes.options.map((o) => o.key)).toEqual(["QUADRA", "LAB01"]);

    // 6. Lab without any non-theoretical rooms available returns empty
    const onlyTheoreticalSalas = Object.fromEntries(
      SALAS.filter((r) => r.tipo === "TEORICA").map((r) => [r.code, r])
    );
    const noLabRes = getAllowedRoomsForLesson(
      { disciplina_id: "D_LAB", tipo: "LABORATORIO", turmas: ["T01"] },
      disciplinasByCode,
      turmasByCode,
      onlyTheoreticalSalas
    );
    expect(noLabRes.options).toEqual([]);
  });
});
