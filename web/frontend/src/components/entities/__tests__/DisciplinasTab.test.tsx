import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen, waitFor, within } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import { useUiStore } from "../../../store/uiStore";
import { DisciplinasTab } from "../DisciplinasTab";
import type { Disciplina, Professor, Room, Turma } from "../../../types";

const apiFetchMock = vi.fn();

vi.mock("../../../api/client", () => ({
  apiFetch: (...args: unknown[]) => apiFetchMock(...args),
}));

const PROFESSORES: Professor[] = [
  {
    id: "P01",
    code: "P01",
    nome: "Carlos Silva",
    preferencia_dias: { SEG: 5 },
    disponibilidade: {},
    turnos_pretendidos: {},
    carga_maxima_diaria_minutos: 0,
    max_aulas_por_dia: 0,
    peso_janela: 8,
  },
  {
    id: "P02",
    code: "P02",
    nome: "Ana Souza",
    preferencia_dias: { SEG: 5 },
    disponibilidade: {},
    turnos_pretendidos: {},
    carga_maxima_diaria_minutos: 0,
    max_aulas_por_dia: 0,
    peso_janela: 8,
  },
];

const SALAS: Room[] = [
  { id: "S101", code: "S101", nome: "Sala Teórica 101", tipo: "TEORICA", capacidade: 40, recursos: [] },
  { id: "S102", code: "S102", nome: "Sala Teórica 102", tipo: "TEORICA", capacidade: 40, recursos: [] },
  { id: "LAB01", code: "LAB01", nome: "Laboratório de Química", tipo: "LABORATORIO", capacidade: 30, recursos: [] },
  { id: "LAB_INF", code: "LAB_INF", nome: "Laboratório de Informática", tipo: "LABORATORIO", capacidade: 35, recursos: [] },
];

const TURMAS: Turma[] = [
  {
    id: "T01",
    code: "T01",
    nome: "1º Ano A",
    turno: "MANHA",
    dias_letivos: ["SEG"],
    jornada_padrao: { slots: [] },
    excecoes_por_dia: {},
    sala_teorica_id: "S101",
    salas_teoricas_permitidas: ["S101"],
  },
  {
    id: "T02",
    code: "T02",
    nome: "2º Ano B",
    turno: "MANHA",
    dias_letivos: ["SEG"],
    jornada_padrao: { slots: [] },
    excecoes_por_dia: {},
    sala_teorica_id: "S102",
    salas_teoricas_permitidas: ["S102"],
  },
];

const DISCIPLINAS: Disciplina[] = [
  {
    id: "T01_FIS",
    code: "T01_FIS",
    nome: "Física",
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
];

function renderTab(disciplinas: Disciplina[] = DISCIPLINAS) {
  const qc = new QueryClient({
    defaultOptions: { queries: { retry: false }, mutations: { retry: false } },
  });
  return render(
    <QueryClientProvider client={qc}>
      <DisciplinasTab
        projectId="proj-1"
        disciplinas={disciplinas}
        professores={PROFESSORES}
        turmas={TURMAS}
        salas={SALAS}
      />
    </QueryClientProvider>
  );
}

describe("DisciplinasTab", () => {
  beforeEach(() => {
    apiFetchMock.mockReset();
    useUiStore.getState().resetDisciplinasColumnWidths();
  });

  it("opens Professores & Turmas expanded on create, allows hiding/showing and quick search", async () => {
    apiFetchMock.mockResolvedValueOnce({ next_id: "D02" });
    renderTab();

    fireEvent.click(screen.getByRole("button", { name: /Adicionar Disciplina/i }));
    await screen.findByText("Nova Disciplina");

    const profSection = screen.getByTestId("selector-professores");
    const turmaSection = screen.getByTestId("selector-turmas");

    expect(within(profSection).getByRole("button", { name: /Esconder selecionáveis/i })).toBeTruthy();
    expect(within(turmaSection).getByRole("button", { name: /Esconder selecionáveis/i })).toBeTruthy();

    // Quick search filters selectable professors while keeping selected chip visible
    fireEvent.change(within(profSection).getByPlaceholderText(/Filtrar professores/i), {
      target: { value: "Ana" },
    });
    expect(within(profSection).getByRole("button", { name: /Ana Souza/i })).toBeTruthy();
    expect(within(profSection).queryByRole("button", { name: /Carlos Silva \(P01\)/i })).toBeNull();
    expect(within(profSection).getByRole("button", { name: /Remover Carlos Silva/i })).toBeTruthy();

    // Hide selectable professors list; selected chip stays visible
    fireEvent.click(within(profSection).getByRole("button", { name: /Esconder selecionáveis/i }));
    expect(within(profSection).getByRole("button", { name: /Mostrar selecionáveis \(2\)/i })).toBeTruthy();
    expect(within(profSection).getByRole("button", { name: /Remover Carlos Silva/i })).toBeTruthy();
    expect(within(profSection).queryByPlaceholderText(/Filtrar professores/i)).toBeNull();
  });

  it("opens Professores & Turmas collapsed on edit and expands on demand", () => {
    renderTab();

    fireEvent.click(screen.getByRole("button", { name: /Editar Física/i }));
    expect(screen.getByText("Editar Física")).toBeTruthy();

    const profSection = screen.getByTestId("selector-professores");
    const turmaSection = screen.getByTestId("selector-turmas");

    expect(within(profSection).getByRole("button", { name: /Mostrar selecionáveis \(2\)/i })).toBeTruthy();
    expect(within(profSection).getByRole("button", { name: /Remover Carlos Silva/i })).toBeTruthy();
    expect(within(turmaSection).getByRole("button", { name: /Mostrar selecionáveis \(2\)/i })).toBeTruthy();
    expect(within(turmaSection).getByRole("button", { name: /Remover 1º Ano A/i })).toBeTruthy();

    // Expand Professores and select Ana Souza
    fireEvent.click(within(profSection).getByRole("button", { name: /Mostrar selecionáveis/i }));
    fireEvent.click(within(profSection).getByRole("button", { name: /Ana Souza/i }));
    expect(within(profSection).getByText("Professores (2)")).toBeTruthy();
  });

  it("displays available rooms for the selected turma even when TEORICA and saves explicit lab selection", async () => {
    apiFetchMock.mockResolvedValue({});
    renderTab();

    fireEvent.click(screen.getByRole("button", { name: /Editar Física/i }));

    const salasSection = screen.getByTestId("salas-disponiveis-section");
    // Shows fixed room of T01 (S101) and labs (LAB01, LAB_INF), while S102 is hidden under "Mostrar outras salas teóricas"
    expect(within(salasSection).getByRole("button", { name: /Sala Teórica 101 \(S101\)/i })).toBeTruthy();
    expect(within(salasSection).getByRole("button", { name: /Laboratório de Química \(LAB01\)/i })).toBeTruthy();
    expect(within(salasSection).queryByRole("button", { name: /Sala Teórica 102 \(S102\)/i })).toBeNull();

    // Reveal other theoretical rooms on demand
    fireEvent.click(within(salasSection).getByRole("button", { name: /Mostrar outras salas teóricas \(1\)/i }));
    expect(within(salasSection).getByRole("button", { name: /Sala Teórica 102 \(S102\)/i })).toBeTruthy();

    // Switch turma from T01 to T02 -> T02's fixed room (S102) moves to turmaRooms
    const turmaSection = screen.getByTestId("selector-turmas");
    fireEvent.click(within(turmaSection).getByRole("button", { name: /Mostrar selecionáveis/i }));
    fireEvent.click(within(turmaSection).getByRole("button", { name: /Remover 1º Ano A/i }));
    fireEvent.click(within(turmaSection).getByRole("button", { name: /2º Ano B/i }));

    expect(within(salasSection).getByRole("button", { name: /Sala Teórica 102 \(S102\).*Fixa/i })).toBeTruthy();

    // Select a lab while keeping tipo === "TEORICA" and save
    fireEvent.click(within(salasSection).getByRole("button", { name: /Laboratório de Química \(LAB01\)/i }));
    fireEvent.click(screen.getByRole("button", { name: /^Salvar$/i }));

    await waitFor(() => {
      expect(apiFetchMock).toHaveBeenCalledWith(
        "/projects/proj-1/disciplinas/T01_FIS",
        expect.objectContaining({
          method: "PUT",
          body: expect.any(String),
        })
      );
    });

    const callBody = JSON.parse(apiFetchMock.mock.calls[0][1].body as string) as Disciplina;
    expect(callBody.tipo).toBe("TEORICA");
    expect(callBody.turmas).toEqual(["T02"]);
    expect(callBody.salas_alternativas).toEqual([["LAB01"]]);
  });

  it("filters disciplinas by turma selector, supports searching by turma name, clears filters, and preselects filtered turma on create", async () => {
    apiFetchMock.mockResolvedValueOnce({ next_id: "D02" });
    const multiDisciplinas: Disciplina[] = [
      ...DISCIPLINAS,
      {
        id: "T02_QUI",
        code: "T02_QUI",
        nome: "Química Orgânica",
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
    ];
    renderTab(multiDisciplinas);

    expect(screen.getByText("Física")).toBeTruthy();
    expect(screen.getByText("Química Orgânica")).toBeTruthy();

    // Select turma T02 in the turma filter
    const turmaSelect = screen.getByLabelText(/Filtrar por turma/i) as HTMLSelectElement;
    fireEvent.change(turmaSelect, { target: { value: "T02" } });

    expect(screen.queryByText("Física")).toBeNull();
    expect(screen.getByText("Química Orgânica")).toBeTruthy();
    expect(screen.getByText("1 de 2 encontrados")).toBeTruthy();

    // Combine with non-matching search -> empty state + Limpar filtros
    const searchInput = screen.getByPlaceholderText(/Buscar disciplinas/i);
    fireEvent.change(searchInput, { target: { value: "Inexistente" } });
    expect(screen.getByText(/Nenhuma disciplina encontrada para os filtros aplicados/i)).toBeTruthy();

    fireEvent.click(screen.getByRole("button", { name: /Limpar filtros/i }));
    expect(turmaSelect.value).toBe("");
    expect(screen.getByText("Física")).toBeTruthy();
    expect(screen.getByText("Química Orgânica")).toBeTruthy();

    // Search by turma name ("2º Ano B") in text search
    fireEvent.change(searchInput, { target: { value: "2º Ano B" } });
    expect(screen.queryByText("Física")).toBeNull();
    expect(screen.getByText("Química Orgânica")).toBeTruthy();

    // Clear search, select T02, and click Adicionar Disciplina -> T02 is preselected
    fireEvent.change(searchInput, { target: { value: "" } });
    fireEvent.change(turmaSelect, { target: { value: "T02" } });
    fireEvent.click(screen.getByRole("button", { name: /Adicionar Disciplina/i }));
    await screen.findByText("Nova Disciplina");

    const turmaSection = screen.getByTestId("selector-turmas");
    expect(within(turmaSection).getByRole("button", { name: /Remover 2º Ano B/i })).toBeTruthy();
  });

  it("displays professor names instead of IDs in the Turmas / Profs column with code fallback", () => {
    const disciplinasWithMultipleAndUnknownProfs: Disciplina[] = [
      ...DISCIPLINAS,
      {
        id: "T02_MAT",
        code: "T02_MAT",
        nome: "Matemática",
        tipo: "TEORICA",
        duracao_minutos: 50,
        aulas_semanais: 4,
        regime: "ANUAL",
        semestre: "AMBOS",
        professores: ["P01", "P02", "P99"],
        turmas: ["T02"],
        salas_alternativas: [],
        exigir_aulas_consecutivas: false,
        max_aulas_por_dia_turma: 2,
      },
    ];

    renderTab(disciplinasWithMultipleAndUnknownProfs);

    expect(screen.getByText("T01 | Carlos Silva")).toBeTruthy();
    expect(screen.queryByText("T01 | P01")).toBeNull();
    expect(screen.getByText("T02 | Carlos Silva, Ana Souza, P99")).toBeTruthy();
  });

  it("resizes columns via drag and keyboard, clamps to bounds, persists in uiStore, and resets", () => {
    const { container } = renderTab();

    const getColWidth = (key: string) => {
      const col = container.querySelector(`col[data-column-key="${key}"]`) as HTMLTableColElement | null;
      return col?.style.width;
    };

    expect(getColWidth("turmasProfs")).toBe("280px");
    expect(screen.queryByRole("button", { name: /Resetar larguras/i })).toBeNull();

    const turmasProfsHandle = screen.getByRole("separator", {
      name: /Redimensionar coluna Turmas \/ Profs/i,
    });

    // Drag right (+120px) -> 400px and persists
    fireEvent.mouseDown(turmasProfsHandle, { clientX: 500 });
    fireEvent.mouseMove(window, { clientX: 620 });
    fireEvent.mouseUp(window);

    expect(getColWidth("turmasProfs")).toBe("400px");
    expect(useUiStore.getState().disciplinasColumnWidths.turmasProfs).toBe(400);
    expect(screen.getByRole("button", { name: /Resetar larguras/i })).toBeTruthy();

    // Drag far left -> clamps to minWidth (140px)
    fireEvent.mouseDown(turmasProfsHandle, { clientX: 500 });
    fireEvent.mouseMove(window, { clientX: 0 });
    fireEvent.mouseUp(window);

    expect(getColWidth("turmasProfs")).toBe("140px");

    // Keyboard ArrowRight increases by 16px -> 156px
    fireEvent.keyDown(turmasProfsHandle, { key: "ArrowRight" });
    expect(getColWidth("turmasProfs")).toBe("156px");

    // Double-click resets only that column back to 280px and clears custom state
    fireEvent.doubleClick(turmasProfsHandle);
    expect(getColWidth("turmasProfs")).toBe("280px");
    expect(screen.queryByRole("button", { name: /Resetar larguras/i })).toBeNull();

    // Resize Nome column via keyboard and click Resetar larguras
    const nomeHandle = screen.getByRole("separator", { name: /Redimensionar coluna Nome/i });
    fireEvent.keyDown(nomeHandle, { key: "ArrowRight" });
    expect(getColWidth("nome")).toBe("266px");

    fireEvent.click(screen.getByRole("button", { name: /Resetar larguras/i }));
    expect(getColWidth("nome")).toBe("250px");
    expect(useUiStore.getState().disciplinasColumnWidths).toEqual({});
  });

  it("displays Máx/Dia column (including Sem limite for 0) and saves updated max_aulas_por_dia_turma", async () => {
    apiFetchMock.mockResolvedValue({});
    const list: Disciplina[] = [
      ...DISCIPLINAS,
      {
        id: "T01_ART",
        code: "T01_ART",
        nome: "Artes",
        tipo: "TEORICA",
        duracao_minutos: 50,
        aulas_semanais: 2,
        regime: "ANUAL",
        semestre: "AMBOS",
        professores: ["P01"],
        turmas: ["T01"],
        salas_alternativas: [],
        exigir_aulas_consecutivas: false,
        max_aulas_por_dia_turma: 0,
      },
    ];
    renderTab(list);

    expect(screen.getByText("2/dia")).toBeTruthy();
    expect(screen.getByText("Sem limite")).toBeTruthy();

    fireEvent.click(screen.getByRole("button", { name: /Editar Física/i }));
    const maxDiaInput = screen.getByLabelText(/Máx\. Aulas\/Dia \(0 = sem limite\)/i) as HTMLInputElement;
    expect(maxDiaInput.value).toBe("2");

    fireEvent.change(maxDiaInput, { target: { value: "0" } });
    fireEvent.click(screen.getByRole("button", { name: /^Salvar$/i }));

    await waitFor(() => {
      expect(apiFetchMock).toHaveBeenCalledWith(
        "/projects/proj-1/disciplinas/T01_FIS",
        expect.objectContaining({
          method: "PUT",
          body: expect.any(String),
        })
      );
    });

    const callBody = JSON.parse(apiFetchMock.mock.calls[0][1].body as string) as Disciplina;
    expect(callBody.max_aulas_por_dia_turma).toBe(0);
  });
});
