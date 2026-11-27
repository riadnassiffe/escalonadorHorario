import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import { ProfessoresTab } from "../ProfessoresTab";
import { useAuthStore } from "../../../store/authStore";
import { useTimetableStore } from "../../../store/timetableStore";
import type { Professor } from "../../../types";

const apiFetchMock = vi.fn();

vi.mock("../../../api/client", () => ({
  apiFetch: (...args: unknown[]) => apiFetchMock(...args),
}));

const PROFESSORES: Professor[] = [
  {
    id: "P01",
    code: "P01",
    nome: "Carlos Silva",
    email: "carlos@escola.edu.br",
    has_user_account: true,
    must_change_password: false,
    preferencia_dias: { SEG: 5, TER: 5, QUA: 5, QUI: 5, SEX: 5 },
    disponibilidade: { SEG: [{ inicio: "08:00", fim: "12:00" }] },
    turnos_pretendidos: { SEG: ["MANHA"] },
    carga_maxima_diaria_minutos: 0,
    max_aulas_por_dia: 0,
    peso_janela: 8,
  },
  {
    id: "P02",
    code: "P02",
    nome: "Ana Souza",
    email: "ana@escola.edu.br",
    has_user_account: true,
    must_change_password: false,
    preferencia_dias: { SEG: 5, TER: 5, QUA: 5, QUI: 5, SEX: 5 },
    disponibilidade: { SEG: [{ inicio: "08:00", fim: "12:00" }] },
    turnos_pretendidos: { SEG: ["MANHA"] },
    carga_maxima_diaria_minutos: 0,
    max_aulas_por_dia: 0,
    peso_janela: 8,
  },
];

function renderTab() {
  const qc = new QueryClient({
    defaultOptions: { queries: { retry: false }, mutations: { retry: false } },
  });
  const invalidateSpy = vi.spyOn(qc, "invalidateQueries");
  const utils = render(
    <QueryClientProvider client={qc}>
      <ProfessoresTab projectId="proj-1" professores={PROFESSORES} />
    </QueryClientProvider>
  );
  return { ...utils, invalidateSpy };
}

describe("ProfessoresTab", () => {
  beforeEach(() => {
    apiFetchMock.mockReset();
    useAuthStore.setState({
      user: {
        id: "user-1",
        institution_id: "inst-1",
        institution_name: "Escola Teste",
        email: "admin@escola.edu.br",
        full_name: "Admin",
        role: "ADMIN",
        must_change_password: false,
        is_active: true,
        created_at: "2026-01-01T00:00:00Z",
      },
    });
    useTimetableStore.setState({
      viewType: "TURMA",
      targetId: "",
      semesterFilter: 0,
      searchQuery: "",
      selectedAulaId: null,
    });
  });

  it("enables ID editing, uses original code in PUT URL, invalidates related caches, and updates timetable targetId", async () => {
    useTimetableStore.setState({ viewType: "PROFESSOR", targetId: "P01" });
    apiFetchMock.mockResolvedValueOnce({
      ...PROFESSORES[0],
      id: "PROF_CARLOS",
      code: "PROF_CARLOS",
    });

    const { invalidateSpy } = renderTab();

    fireEvent.click(screen.getByRole("button", { name: /Editar Carlos Silva/i }));
    const idInput = screen.getByLabelText(/^ID$/i) as HTMLInputElement;
    expect(idInput.disabled).toBe(false);
    expect(idInput.value).toBe("P01");

    fireEvent.change(idInput, { target: { value: "  PROF_CARLOS  " } });
    expect(
      screen.getByText(/Ao alterar o ID, disciplinas e grades vinculadas serão atualizadas automaticamente/i)
    ).toBeTruthy();

    fireEvent.click(screen.getByRole("button", { name: /^Salvar$/i }));

    await waitFor(() => {
      expect(apiFetchMock).toHaveBeenCalledWith(
        "/projects/proj-1/professores/P01",
        expect.objectContaining({
          method: "PUT",
          body: expect.any(String),
        })
      );
    });

    const sentBody = JSON.parse(apiFetchMock.mock.calls[0][1].body as string) as Record<string, unknown>;
    expect(sentBody.code).toBe("PROF_CARLOS");
    expect(sentBody).not.toHaveProperty("isNew");
    expect(sentBody).not.toHaveProperty("originalCode");

    await waitFor(() => {
      expect(invalidateSpy).toHaveBeenCalledWith({ queryKey: ["professores", "proj-1"] });
      expect(invalidateSpy).toHaveBeenCalledWith({ queryKey: ["project", "proj-1"] });
      expect(invalidateSpy).toHaveBeenCalledWith({ queryKey: ["disciplinas", "proj-1"] });
      expect(invalidateSpy).toHaveBeenCalledWith({ queryKey: ["timetable", "proj-1"] });
      expect(invalidateSpy).toHaveBeenCalledWith({ queryKey: ["unallocated", "proj-1"] });
    });

    expect(useTimetableStore.getState().targetId).toBe("PROF_CARLOS");
  });

  it("disables Salvar when ID is empty or whitespace", () => {
    renderTab();

    fireEvent.click(screen.getByRole("button", { name: /Editar Carlos Silva/i }));
    const idInput = screen.getByLabelText(/^ID$/i) as HTMLInputElement;
    const saveBtn = screen.getByRole("button", { name: /^Salvar$/i }) as HTMLButtonElement;

    expect(saveBtn.disabled).toBe(false);

    fireEvent.change(idInput, { target: { value: "   " } });
    expect(saveBtn.disabled).toBe(true);
  });

  it("displays API error inside the modal without closing it on failure", async () => {
    apiFetchMock.mockRejectedValueOnce(new Error("Professor 'P02' já existe."));
    renderTab();

    fireEvent.click(screen.getByRole("button", { name: /Editar Carlos Silva/i }));
    fireEvent.change(screen.getByLabelText(/^ID$/i), { target: { value: "P02" } });
    fireEvent.click(screen.getByRole("button", { name: /^Salvar$/i }));

    expect(await screen.findByText("Professor 'P02' já existe.")).toBeTruthy();
    expect(screen.getByLabelText(/^ID$/i)).toBeTruthy();
  });
});
