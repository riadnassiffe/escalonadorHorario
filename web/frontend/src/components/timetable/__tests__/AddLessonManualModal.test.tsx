import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen, within } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import type { DomainSnapshot } from "../../../domain/conflictEngine";
import { useTimetableStore } from "../../../store/timetableStore";
import type { TimetableResponse, UnallocatedLesson } from "../../../types";
import { AddLessonManualModal } from "../TimetableModals";
import { TimetableView } from "../TimetableView";

const mockUnallocated: UnallocatedLesson[] = [
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
    aula_id: "QUI_LAB_1",
    disciplina_id: "QUI_LAB",
    disciplina_nome: "Química Prática",
    tipo: "LABORATORIO",
    duracao_minutos: 50,
    regime: "ANUAL",
    semestre: "AMBOS",
    professores: ["P02"],
    turmas: ["T01"],
  },
  {
    aula_id: "ROB_1",
    disciplina_id: "ROB",
    disciplina_nome: "Robótica",
    tipo: "TEORICA",
    duracao_minutos: 50,
    regime: "ANUAL",
    semestre: "AMBOS",
    professores: ["P01"],
    turmas: ["T01"],
  },
  {
    aula_id: "HIST_1",
    disciplina_id: "HIST",
    disciplina_nome: "História",
    tipo: "TEORICA",
    duracao_minutos: 50,
    regime: "ANUAL",
    semestre: "AMBOS",
    professores: ["P02"],
    turmas: ["T02"],
  },
];

const mockSnapshot: DomainSnapshot = {
  professores: {
    P01: {
      id: "P01",
      code: "P01",
      nome: "Carlos Silva",
      preferencia_dias: { SEG: 5 },
      disponibilidade: { SEG: [{ inicio: "08:00", fim: "12:00" }] },
      turnos_pretendidos: { SEG: ["MANHA"] },
      carga_maxima_diaria_minutos: 0,
      max_aulas_por_dia: 0,
      peso_janela: 8,
    },
    P02: {
      id: "P02",
      code: "P02",
      nome: "Ana Souza",
      preferencia_dias: { SEG: 5 },
      disponibilidade: { SEG: [{ inicio: "08:00", fim: "12:00" }] },
      turnos_pretendidos: { SEG: ["MANHA"] },
      carga_maxima_diaria_minutos: 0,
      max_aulas_por_dia: 0,
      peso_janela: 8,
    },
  },
  salas: {
    S101: { id: "S101", code: "S101", nome: "Sala 101", tipo: "TEORICA", capacidade: 40, recursos: [] },
    S102: { id: "S102", code: "S102", nome: "Sala 102", tipo: "TEORICA", capacidade: 40, recursos: [] },
    LAB01: { id: "LAB01", code: "LAB01", nome: "Lab Química", tipo: "LABORATORIO", capacidade: 30, recursos: [] },
    QUADRA: { id: "QUADRA", code: "QUADRA", nome: "Quadra", tipo: "ESPECIAL", capacidade: 60, recursos: [] },
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
        ],
      },
      excecoes_por_dia: {},
      sala_teorica_id: "S101",
      salas_teoricas_permitidas: ["S101"],
    },
    T02: {
      id: "T02",
      code: "T02",
      nome: "2º Ano B",
      turno: "MANHA",
      dias_letivos: ["SEG", "TER"],
      jornada_padrao: {
        slots: [{ inicio: "08:00", fim: "08:50" }],
      },
      excecoes_por_dia: {},
      sala_teorica_id: "S102",
      salas_teoricas_permitidas: ["S102"],
    },
    T03: {
      id: "T03",
      code: "T03",
      nome: "3º Ano C",
      turno: "MANHA",
      dias_letivos: ["SEG"],
      jornada_padrao: { slots: [{ inicio: "08:00", fim: "08:50" }] },
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
      aulas_semanais: 3,
      regime: "ANUAL",
      semestre: "AMBOS",
      professores: ["P01"],
      turmas: ["T01"],
      salas_alternativas: [],
      exigir_aulas_consecutivas: false,
      max_aulas_por_dia_turma: 2,
    },
    QUI_LAB: {
      id: "QUI_LAB",
      code: "QUI_LAB",
      nome: "Química Prática",
      tipo: "LABORATORIO",
      duracao_minutos: 50,
      aulas_semanais: 1,
      regime: "ANUAL",
      semestre: "AMBOS",
      professores: ["P02"],
      turmas: ["T01"],
      salas_alternativas: [],
      exigir_aulas_consecutivas: false,
      max_aulas_por_dia_turma: 2,
    },
    ROB: {
      id: "ROB",
      code: "ROB",
      nome: "Robótica",
      tipo: "TEORICA",
      duracao_minutos: 50,
      aulas_semanais: 1,
      regime: "ANUAL",
      semestre: "AMBOS",
      professores: ["P01"],
      turmas: ["T01"],
      salas_alternativas: [["QUADRA"]],
      exigir_aulas_consecutivas: false,
      max_aulas_por_dia_turma: 2,
    },
    HIST: {
      id: "HIST",
      code: "HIST",
      nome: "História",
      tipo: "TEORICA",
      duracao_minutos: 50,
      aulas_semanais: 2,
      regime: "ANUAL",
      semestre: "AMBOS",
      professores: ["P02"],
      turmas: ["T02"],
      salas_alternativas: [],
      exigir_aulas_consecutivas: false,
      max_aulas_por_dia_turma: 2,
    },
  },
  config: { distribuir_disciplinas_na_semana: true, max_aulas_disciplina_dia_turma: 2 },
  alocacoes: [],
};

const noopMutation = { mutate: vi.fn(), isPending: false };

vi.mock("../../../api/hooks", () => ({
  useUnallocated: () => ({ data: { unallocated: mockUnallocated }, isLoading: false }),
  useTimetableMutations: () => ({
    moveLesson: noopMutation,
    swapLessons: noopMutation,
    changeRoom: noopMutation,
    allocateManual: noopMutation,
    pinLesson: noopMutation,
    pinAll: noopMutation,
    removeLesson: noopMutation,
    clearTimetable: noopMutation,
  }),
}));

describe("AddLessonManualModal", () => {
  beforeEach(() => {
    useTimetableStore.setState({
      viewType: "TURMA",
      targetId: "T01",
      semesterFilter: 0,
      searchQuery: "",
      selectedAulaId: null,
    });
  });

  it("filters unallocated lessons by selected turma and shows contextual empty state when none pending", () => {
    const onConfirm = vi.fn();
    render(
      <AddLessonManualModal
        unallocated={mockUnallocated}
        snapshot={mockSnapshot}
        viewType="TURMA"
        targetId="T01"
        onClose={vi.fn()}
        onConfirm={onConfirm}
      />
    );

    expect(screen.getByText(/Turma 1º Ano A \(T01\)/i)).toBeTruthy();

    const aulaSelect = screen.getByLabelText(/Ocorrência de Aula/i) as HTMLSelectElement;
    const optionValues = Array.from(aulaSelect.options).map((o) => o.value);
    expect(optionValues).toEqual(["MAT_1", "QUI_LAB_1", "ROB_1"]);
    expect(optionValues).not.toContain("HIST_1");

    // Empty state for T03 (no pending lessons for T03, even though other turmas have pending)
    const qc = new QueryClient();
    render(
      <QueryClientProvider client={qc}>
        <AddLessonManualModal
          unallocated={mockUnallocated}
          snapshot={mockSnapshot}
          viewType="TURMA"
          targetId="T03"
          onClose={vi.fn()}
          onConfirm={onConfirm}
        />
      </QueryClientProvider>
    );

    expect(
      screen.getByText(/Não há aulas pendentes de alocação para Turma 3º Ano C \(T03\)\./i)
    ).toBeTruthy();
  });

  it("filters unallocated lessons by selected professor", () => {
    render(
      <AddLessonManualModal
        unallocated={mockUnallocated}
        snapshot={mockSnapshot}
        viewType="PROFESSOR"
        targetId="P02"
        onClose={vi.fn()}
        onConfirm={vi.fn()}
      />
    );

    expect(screen.getByText(/Professor Ana Souza \(P02\)/i)).toBeTruthy();
    const aulaSelect = screen.getByLabelText(/Ocorrência de Aula/i) as HTMLSelectElement;
    const optionValues = Array.from(aulaSelect.options).map((o) => o.value);
    expect(optionValues).toEqual(["QUI_LAB_1", "HIST_1"]);
  });

  it("updates allowed rooms dynamically when switching lessons and submits selected allowed room", () => {
    const onConfirm = vi.fn();
    render(
      <AddLessonManualModal
        unallocated={mockUnallocated}
        snapshot={mockSnapshot}
        viewType="TURMA"
        targetId="T01"
        onClose={vi.fn()}
        onConfirm={onConfirm}
      />
    );

    const aulaSelect = screen.getByLabelText(/Ocorrência de Aula/i) as HTMLSelectElement;
    const salaSelect = screen.getByLabelText(/^Sala$/i) as HTMLSelectElement;

    // Initially MAT_1 (TEORICA for T01 -> fixed room S101 only)
    expect(aulaSelect.value).toBe("MAT_1");
    expect(Array.from(salaSelect.options).map((o) => o.value)).toEqual(["S101"]);
    expect(salaSelect.value).toBe("S101");

    // Switch to QUI_LAB_1 (LABORATORIO -> only non-theoretical rooms, LAB01 first)
    fireEvent.change(aulaSelect, { target: { value: "QUI_LAB_1" } });
    expect(Array.from(salaSelect.options).map((o) => o.value)).toEqual(["LAB01", "QUADRA"]);
    expect(salaSelect.value).toBe("LAB01");

    // Switch to ROB_1 (explicit salas_alternativas: [["QUADRA"]] -> only QUADRA)
    fireEvent.change(aulaSelect, { target: { value: "ROB_1" } });
    expect(Array.from(salaSelect.options).map((o) => o.value)).toEqual(["QUADRA"]);
    expect(salaSelect.value).toBe("QUADRA");

    fireEvent.click(screen.getByRole("button", { name: /Alocar na Grade/i }));
    expect(onConfirm).toHaveBeenCalledWith(
      expect.objectContaining({
        aula_id: "ROB_1",
        disciplina_id: "ROB",
        salas: ["QUADRA"],
      })
    );
  });

  it("disables submit and warns when no compatible room exists for the selected lesson", () => {
    const noLabSnapshot: DomainSnapshot = {
      ...mockSnapshot,
      salas: {
        S101: mockSnapshot.salas.S101,
      },
    };

    render(
      <AddLessonManualModal
        unallocated={[mockUnallocated[1]]} // QUI_LAB_1
        snapshot={noLabSnapshot}
        viewType="TURMA"
        targetId="T01"
        onClose={vi.fn()}
        onConfirm={vi.fn()}
      />
    );

    const salaSelect = screen.getByLabelText(/^Sala$/i) as HTMLSelectElement;
    expect(salaSelect.disabled).toBe(true);
    expect(screen.getByText(/Nenhuma sala compatível cadastrada para esta aula/i)).toBeTruthy();
    const confirmBtn = screen.getByRole("button", { name: /Alocar na Grade/i }) as HTMLButtonElement;
    expect(confirmBtn.disabled).toBe(true);
  });

  it("passes active TURMA and PROFESSOR context from TimetableView when clicking + Alocar", () => {
    const timetable: TimetableResponse = {
      version_id: "v1",
      version_number: 1,
      label: "v1",
      alocacoes: [],
      salas_teoricas_designadas: {},
      estatisticas: {},
      mensagens_diagnostico: [],
      conflitos: {},
    };

    const { rerender } = render(<TimetableView projectId="proj-1" snapshot={mockSnapshot} timetable={timetable} />);

    // Open modal in TURMA=T01 view
    fireEvent.click(screen.getByRole("button", { name: /Alocar/i }));
    let dialogHeading = screen.getByText("Alocar Disciplina Manualmente").closest("div")?.parentElement;
    expect(dialogHeading).toBeTruthy();
    expect(within(dialogHeading!).getByText(/Turma 1º Ano A \(T01\)/i)).toBeTruthy();
    let aulaSelect = within(dialogHeading!).getByLabelText(/Ocorrência de Aula/i) as HTMLSelectElement;
    expect(Array.from(aulaSelect.options).map((o) => o.value)).toEqual(["MAT_1", "QUI_LAB_1", "ROB_1"]);

    // Close modal, switch store to PROFESSOR=P02, and reopen
    fireEvent.click(within(dialogHeading!).getByRole("button", { name: /^Fechar$/i }));
    useTimetableStore.setState({ viewType: "PROFESSOR", targetId: "P02" });
    rerender(<TimetableView projectId="proj-1" snapshot={mockSnapshot} timetable={timetable} />);

    fireEvent.click(screen.getByRole("button", { name: /Alocar/i }));
    dialogHeading = screen.getByText("Alocar Disciplina Manualmente").closest("div")?.parentElement;
    expect(within(dialogHeading!).getByText(/Professor Ana Souza \(P02\)/i)).toBeTruthy();
    aulaSelect = within(dialogHeading!).getByLabelText(/Ocorrência de Aula/i) as HTMLSelectElement;
    expect(Array.from(aulaSelect.options).map((o) => o.value)).toEqual(["QUI_LAB_1", "HIST_1"]);
  });
});
