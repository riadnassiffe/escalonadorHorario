import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen, waitFor, within } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import { TurmasTab } from "../TurmasTab";
import type { Disciplina, Professor, Room, Turma } from "../../../types";

const apiFetchMock = vi.fn();

vi.mock("../../../api/client", () => ({
  apiFetch: (...args: unknown[]) => apiFetchMock(...args),
}));

vi.mock("../../../api/hooks", () => ({
  useSlotTemplates: () => ({ data: { slot_templates: {} } }),
}));

const SALAS: Room[] = [
  { id: "S101", code: "S101", nome: "Sala 101", tipo: "TEORICA", capacidade: 40, recursos: [] },
];

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

const TURMAS: Turma[] = [
  {
    id: "T01",
    code: "T01",
    nome: "1º Ano A",
    turno: "MANHA",
    dias_letivos: ["SEG", "TER"],
    jornada_padrao: { slots: [{ inicio: "08:00", fim: "08:50" }] },
    excecoes_por_dia: {},
    sala_teorica_id: "S101",
    salas_teoricas_permitidas: ["S101"],
  },
  {
    id: "T02",
    code: "T02",
    nome: "2º Ano B",
    turno: "MANHA",
    dias_letivos: ["SEG", "TER"],
    jornada_padrao: { slots: [{ inicio: "08:00", fim: "08:50" }] },
    excecoes_por_dia: {},
    sala_teorica_id: "S101",
    salas_teoricas_permitidas: ["S101"],
  },
];

const DISCIPLINAS: Disciplina[] = [
  {
    id: "T01_MAT",
    code: "T01_MAT",
    nome: "Matemática",
    tipo: "TEORICA",
    duracao_minutos: 50,
    aulas_semanais: 4,
    regime: "ANUAL",
    semestre: "AMBOS",
    professores: ["P01"],
    turmas: ["T01"],
    salas_alternativas: [],
    exigir_aulas_consecutivas: false,
    max_aulas_por_dia_turma: 2,
  },
  {
    id: "T01_FIS",
    code: "T01_FIS",
    nome: "Física",
    tipo: "TEORICA",
    duracao_minutos: 50,
    aulas_semanais: 3,
    regime: "ANUAL",
    semestre: "AMBOS",
    professores: ["P02"],
    turmas: ["T01"],
    salas_alternativas: [],
    exigir_aulas_consecutivas: false,
    max_aulas_por_dia_turma: 2,
  },
  {
    id: "T02_BIO",
    code: "T02_BIO",
    nome: "Biologia",
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

function renderTab() {
  const qc = new QueryClient({
    defaultOptions: { queries: { retry: false }, mutations: { retry: false } },
  });
  return render(
    <QueryClientProvider client={qc}>
      <TurmasTab
        projectId="proj-1"
        turmas={TURMAS}
        salas={SALAS}
        disciplinas={DISCIPLINAS}
        professores={PROFESSORES}
      />
    </QueryClientProvider>
  );
}

describe("TurmasTab - CopyTurmaModal disciplina selection", () => {
  beforeEach(() => {
    apiFetchMock.mockReset();
  });

  it("allows selecting specific linked disciplinas when copying a turma", async () => {
    apiFetchMock
      .mockResolvedValueOnce({ next_id: "T03" })
      .mockResolvedValueOnce({
        turma: { ...TURMAS[0], id: "T03", code: "T03", nome: "1º Ano A (Cópia)" },
        copied_disciplinas_count: 1,
        copied_disciplinas: [],
      });

    renderTab();

    fireEvent.click(screen.getByRole("button", { name: /Copiar 1º Ano A/i }));
    await screen.findByRole("heading", { name: "Copiar Turma" });

    const toggleAllCheckbox = screen.getByRole("checkbox", {
      name: /Copiar também as disciplinas vinculadas a esta turma \(2 disciplina\(s\)\)/i,
    });
    fireEvent.click(toggleAllCheckbox);

    const selector = screen.getByTestId("copy-disciplinas-selector");
    expect(within(selector).getByText("Selecionadas: 2 de 2 disciplina(s)")).toBeTruthy();
    expect(within(selector).getByText("Matemática")).toBeTruthy();
    expect(within(selector).getByText("Física")).toBeTruthy();
    expect(within(selector).queryByText("Biologia")).toBeNull();

    // Filter inside selector and uncheck Física
    fireEvent.change(within(selector).getByPlaceholderText(/Filtrar disciplinas/i), {
      target: { value: "Física" },
    });
    expect(within(selector).queryByText("Matemática")).toBeNull();
    const fisCheckbox = within(selector).getByRole("checkbox");
    fireEvent.click(fisCheckbox);

    fireEvent.change(within(selector).getByPlaceholderText(/Filtrar disciplinas/i), {
      target: { value: "" },
    });
    expect(within(selector).getByText("Selecionadas: 1 de 2 disciplina(s)")).toBeTruthy();

    fireEvent.click(screen.getByRole("button", { name: /^Copiar Turma$/i }));

    await waitFor(() => {
      expect(apiFetchMock).toHaveBeenCalledWith(
        "/projects/proj-1/turmas/T01/copy",
        expect.objectContaining({
          method: "POST",
          body: expect.any(String),
        })
      );
    });

    const body = JSON.parse(apiFetchMock.mock.calls[1][1].body as string) as Record<string, unknown>;
    expect(body.copy_disciplinas).toBe(true);
    expect(body.disciplina_codes).toEqual(["T01_MAT"]);
  });

  it("supports clearing and selecting all linked disciplinas and warns when none selected", async () => {
    apiFetchMock
      .mockResolvedValueOnce({ next_id: "T03" })
      .mockResolvedValueOnce({
        turma: { ...TURMAS[0], id: "T03", code: "T03", nome: "1º Ano A (Cópia)" },
        copied_disciplinas_count: 0,
        copied_disciplinas: [],
      });

    renderTab();

    fireEvent.click(screen.getByRole("button", { name: /Copiar 1º Ano A/i }));
    await screen.findByRole("heading", { name: "Copiar Turma" });

    fireEvent.click(
      screen.getByRole("checkbox", {
        name: /Copiar também as disciplinas vinculadas a esta turma/i,
      })
    );

    const selector = screen.getByTestId("copy-disciplinas-selector");
    fireEvent.click(within(selector).getByRole("button", { name: /Limpar seleção/i }));
    expect(within(selector).getByText("Selecionadas: 0 de 2 disciplina(s)")).toBeTruthy();
    expect(
      within(selector).getByText(/Nenhuma disciplina selecionada — apenas a turma será copiada/i)
    ).toBeTruthy();

    // Select all then clear again to verify both buttons and submit empty selection
    fireEvent.click(within(selector).getByRole("button", { name: /Selecionar todas/i }));
    expect(within(selector).getByText("Selecionadas: 2 de 2 disciplina(s)")).toBeTruthy();

    fireEvent.click(within(selector).getByRole("button", { name: /Limpar seleção/i }));
    fireEvent.click(screen.getByRole("button", { name: /^Copiar Turma$/i }));

    await waitFor(() => {
      expect(apiFetchMock).toHaveBeenCalledTimes(2);
    });
    const body = JSON.parse(apiFetchMock.mock.calls[1][1].body as string) as Record<string, unknown>;
    expect(body.copy_disciplinas).toBe(false);
    expect(body.disciplina_codes).toEqual([]);
  });
});
