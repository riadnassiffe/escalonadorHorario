import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { render, screen } from "@testing-library/react";
import { MemoryRouter, Route, Routes } from "react-router-dom";
import { describe, expect, it, vi } from "vitest";
import { ProjectWorkspacePage } from "../ProjectWorkspacePage";

vi.mock("../../api/client", () => ({
  apiFetch: vi.fn(),
  apiDownload: vi.fn(),
}));

const noopMutation = { mutate: vi.fn(), isPending: false };

vi.mock("../../api/hooks", () => ({
  useProject: () => ({
    data: {
      id: "proj-1",
      institution_id: "inst-1",
      name: "Projeto Teste",
      description: "",
      academic_year: "2026",
      active_timetable_version_id: null,
      allow_professor_preferences_edit: true,
      created_at: "2026-01-01T00:00:00Z",
      updated_at: "2026-01-01T00:00:00Z",
      counts: { professores: 0, salas: 0, turmas: 0, disciplinas: 0 },
    },
  }),
  useProfessores: () => ({ data: [] }),
  useSalas: () => ({ data: [] }),
  useTurmas: () => ({ data: [] }),
  useDisciplinas: () => ({ data: [] }),
  useConfig: () => ({ data: undefined }),
  useTimetable: () => ({
    data: {
      version_id: null,
      version_number: null,
      label: null,
      alocacoes: [],
      salas_teoricas_designadas: {},
      estatisticas: {},
      mensagens_diagnostico: [],
      conflitos: {},
    },
  }),
  useTimetableVersions: () => ({ data: [] }),
  useProjectJobs: () => ({ data: [] }),
  useJobStream: () => null,
  useInvalidateWorkspace: () => vi.fn(),
  useUnallocated: () => ({ data: { unallocated: [] } }),
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

describe("ProjectWorkspacePage & global cursor styles", () => {
  it("renders workspace action buttons without the Exemplo button", () => {
    const qc = new QueryClient({
      defaultOptions: { queries: { retry: false }, mutations: { retry: false } },
    });

    render(
      <QueryClientProvider client={qc}>
        <MemoryRouter initialEntries={["/projects/proj-1"]}>
          <Routes>
            <Route path="/projects/:projectId" element={<ProjectWorkspacePage />} />
          </Routes>
        </MemoryRouter>
      </QueryClientProvider>
    );

    expect(screen.getByRole("button", { name: /OTIMIZAR GRADE/i })).toBeTruthy();
    expect(screen.getByRole("button", { name: /HTML/i })).toBeTruthy();
    expect(screen.getByRole("button", { name: /CSV/i })).toBeTruthy();
    expect(screen.queryByRole("button", { name: /Exemplo/i })).toBeNull();
  });

  it("defines pointer cursor for links/buttons and not-allowed for disabled buttons in index.css", async () => {
    // @ts-expect-error -- node:fs is available in the Vitest runtime without @types/node
    const fs = await import("node:fs");
    const css = fs.readFileSync("src/index.css", "utf-8") as string;

    expect(css).toContain("@layer base");
    expect(css).toContain("a[href]");
    expect(css).toContain("button:not(:disabled)");
    expect(css).toContain("cursor: pointer;");
    expect(css).toContain("button:disabled");
    expect(css).toContain("cursor: not-allowed;");
  });
});
