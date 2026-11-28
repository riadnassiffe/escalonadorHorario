import { fireEvent, render, screen } from "@testing-library/react";
import { describe, expect, it, vi } from "vitest";
import type { DomainSnapshot } from "../../../domain/conflictEngine";
import { TimetableGrid } from "../TimetableGrid";

const SNAPSHOT: DomainSnapshot = {
  professores: {
    P01: {
      id: "P01",
      code: "P01",
      nome: "Carlos",
      preferencia_dias: { SEG: 5, TER: 5 },
      disponibilidade: {
        SEG: [{ inicio: "08:00", fim: "11:00" }],
        TER: [],
      },
      turnos_pretendidos: { SEG: ["MANHA"], TER: ["MANHA"] },
      carga_maxima_diaria_minutos: 0,
      max_aulas_por_dia: 0,
      peso_janela: 8,
    },
    P02: {
      id: "P02",
      code: "P02",
      nome: "Ana",
      preferencia_dias: { SEG: 5, TER: 5 },
      disponibilidade: {
        SEG: [{ inicio: "08:00", fim: "11:00" }],
        TER: [{ inicio: "08:00", fim: "11:00" }],
      },
      turnos_pretendidos: { SEG: ["MANHA"], TER: ["MANHA"] },
      carga_maxima_diaria_minutos: 0,
      max_aulas_por_dia: 0,
      peso_janela: 8,
    },
  },
  salas: {
    S101: { id: "S101", code: "S101", nome: "Sala 101", tipo: "TEORICA", capacidade: 40, recursos: [] },
  },
  turmas: {
    T01: {
      id: "T01",
      code: "T01",
      nome: "1º Ano A",
      turno: "MANHA",
      dias_letivos: ["SEG", "TER"],
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
    FIS: {
      id: "FIS",
      code: "FIS",
      nome: "Física",
      tipo: "TEORICA",
      duracao_minutos: 50,
      aulas_semanais: 2,
      regime: "ANUAL",
      semestre: "AMBOS",
      professores: ["P02"],
      turmas: ["T01"],
      salas_alternativas: [],
      exigir_aulas_consecutivas: false,
      max_aulas_por_dia_turma: 2,
    },
    ART: {
      id: "ART",
      code: "ART",
      nome: "Artes",
      tipo: "TEORICA",
      duracao_minutos: 45,
      aulas_semanais: 1,
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
      aula_id: "A1",
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
      aula_id: "A2",
      disciplina_id: "FIS",
      disciplina_nome: "Física",
      tipo: "TEORICA",
      dia: "SEG",
      inicio: "08:50",
      fim: "09:40",
      start_min: 530,
      end_min: 580,
      professores: ["P02"],
      turmas: ["T01"],
      salas: ["S101"],
      is_pinned: false,
      regime: "ANUAL",
      semestre: "AMBOS",
      semestre_alocado: 1,
    },
    {
      aula_id: "A3",
      disciplina_id: "ART",
      disciplina_nome: "Artes",
      tipo: "TEORICA",
      dia: "SEG",
      inicio: "09:55",
      fim: "10:40",
      start_min: 595,
      end_min: 640,
      professores: ["P01"],
      turmas: ["T01"],
      salas: ["S101"],
      is_pinned: false,
      regime: "ANUAL",
      semestre: "AMBOS",
      semestre_alocado: 1,
    },
    {
      aula_id: "A4",
      disciplina_id: "FIS",
      disciplina_nome: "Física Terça",
      tipo: "TEORICA",
      dia: "TER",
      inicio: "08:00",
      fim: "08:50",
      start_min: 480,
      end_min: 530,
      professores: ["P02"],
      turmas: ["T01"],
      salas: ["S101"],
      is_pinned: false,
      regime: "ANUAL",
      semestre: "AMBOS",
      semestre_alocado: 1,
    },
  ],
};

const ROWS = [
  { startMin: 480, endMin: 530, isBreak: false },
  { startMin: 530, endMin: 580, isBreak: false },
  { startMin: 595, endMin: 640, isBreak: false },
];

describe("TimetableGrid swap highlight", () => {
  it("highlights only conflict-free swappable lessons with green background when a lesson is selected", () => {
    const onSelectLesson = vi.fn();

    const { container, rerender } = render(
      <TimetableGrid
        snapshot={SNAPSHOT}
        alocacoes={SNAPSHOT.alocacoes}
        conflitos={{}}
        rows={ROWS}
        transposeGrid={false}
        searchQuery=""
        selectedAulaId={null}
        onSelectLesson={onSelectLesson}
        onDoubleClickLesson={vi.fn()}
        onContextMenuLesson={vi.fn()}
        onContextMenuEmpty={vi.fn()}
        onDropLesson={vi.fn()}
      />
    );

    expect(container.querySelectorAll('[data-swap-eligible="true"]').length).toBe(0);
    expect(screen.queryByText(/Troca livre/i)).toBeNull();

    rerender(
      <TimetableGrid
        snapshot={SNAPSHOT}
        alocacoes={SNAPSHOT.alocacoes}
        conflitos={{}}
        rows={ROWS}
        transposeGrid={false}
        searchQuery=""
        selectedAulaId="A1"
        onSelectLesson={onSelectLesson}
        onDoubleClickLesson={vi.fn()}
        onContextMenuLesson={vi.fn()}
        onContextMenuEmpty={vi.fn()}
        onDropLesson={vi.fn()}
      />
    );

    const eligibleCards = container.querySelectorAll('[data-swap-eligible="true"]');
    expect(eligibleCards.length).toBe(1);

    const fisicaCard = screen.getByText("Física").closest('[data-swap-eligible="true"]') as HTMLElement | null;
    expect(fisicaCard).not.toBeNull();
    expect(fisicaCard?.className).toContain("bg-emerald-100/95");
    expect(fisicaCard?.textContent).toContain("Troca livre");

    // Selected lesson (A1), duration mismatch (A3, 45m), and professor unavailable on TER (A4) must NOT be green
    expect(screen.getByText("Matemática").closest('[data-swap-eligible="true"]')).toBeNull();
    expect(screen.getByText("Artes").closest('[data-swap-eligible="true"]')).toBeNull();
    expect(screen.getByText("Física Terça").closest('[data-swap-eligible="true"]')).toBeNull();

    // Clicking an empty slot clears the selection
    const emptyCells = container.querySelectorAll("td");
    const lastEmptyCell = emptyCells[emptyCells.length - 1];
    fireEvent.click(lastEmptyCell);
    expect(onSelectLesson).toHaveBeenCalledWith(null);
  });
});
