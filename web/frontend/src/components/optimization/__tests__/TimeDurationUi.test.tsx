import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { fireEvent, render, screen, waitFor } from "@testing-library/react";
import { beforeEach, describe, expect, it, vi } from "vitest";
import type { OptimizationConfig, TimetableResponse } from "../../../types";
import { KpiBanner } from "../../layout/KpiBanner";
import { ConfigTab } from "../ConfigTab";
import { OptimizeModal } from "../OptimizeModal";

const apiFetchMock = vi.fn();

vi.mock("../../../api/client", () => ({
  apiFetch: (...args: unknown[]) => apiFetchMock(...args),
}));

const BASE_CONFIG: OptimizationConfig = {
  algoritmo: "CP_SAT",
  escopo_semestre: "TODAS_SEM_FILTRO",
  sincronizar_anuais_entre_semestres: true,
  lambda_janelas: 8,
  lambda_dias: 5,
  lambda_preferencias: 3,
  lambda_preferencia_dias: 7,
  lambda_preferencia_turnos: 8,
  lambda_lacunas_turma: 2,
  lambda_aulas_consecutivas: 8,
  modelo_preferencia_dias: "POR_DIA_TRABALHADO",
  preenchimento_integral_jornada: false,
  distribuir_disciplinas_na_semana: true,
  exigir_aulas_consecutivas_rigido: true,
  tempo_limite_segundos: 90,
  sem_limite_tempo: false,
  max_memoria_mb: 2048,
  sem_limite_memoria: false,
  num_threads: 4,
  log_search_progress: false,
  usar_distribuicao_como_entrada: true,
};

describe("Time duration UI (H/M/S)", () => {
  beforeEach(() => {
    apiFetchMock.mockReset();
  });

  it("formats execution time in KpiBanner as hours, minutes and seconds", () => {
    const makeTimetable = (ms: number): TimetableResponse => ({
      version_id: "v1",
      version_number: 1,
      label: "v1",
      alocacoes: [],
      salas_teoricas_designadas: {},
      estatisticas: { status: "OPTIMAL", tempo_execucao_ms: ms },
      mensagens_diagnostico: [],
      conflitos: {},
    });

    const { rerender } = render(<KpiBanner timetable={makeTimetable(3723000)} />);
    expect(screen.getByText("01h 02m 03s")).toBeTruthy();

    rerender(<KpiBanner timetable={makeTimetable(450)} />);
    expect(screen.getByText("00h 00m 00.45s")).toBeTruthy();

    rerender(<KpiBanner timetable={makeTimetable(0)} />);
    expect(screen.getByText("00h 00m 00s")).toBeTruthy();
  });

  it("allows configuring time limit in H/M/S in OptimizeModal", () => {
    const onLaunch = vi.fn();
    render(<OptimizeModal config={BASE_CONFIG} onClose={vi.fn()} onLaunch={onLaunch} />);

    const hours = screen.getByLabelText("Horas") as HTMLInputElement;
    const minutes = screen.getByLabelText("Minutos") as HTMLInputElement;
    const seconds = screen.getByLabelText("Segundos") as HTMLInputElement;

    expect(hours.value).toBe("0");
    expect(minutes.value).toBe("1");
    expect(seconds.value).toBe("30");

    fireEvent.change(hours, { target: { value: "1" } });
    fireEvent.change(minutes, { target: { value: "15" } });
    fireEvent.change(seconds, { target: { value: "20" } });

    fireEvent.click(screen.getByRole("button", { name: /Iniciar Otimização/i }));
    expect(onLaunch).toHaveBeenCalledWith(
      expect.objectContaining({
        tempo_limite_segundos: 4520,
      })
    );
  });

  it("loads and saves time limit in H/M/S in ConfigTab", async () => {
    apiFetchMock.mockResolvedValue({ ...BASE_CONFIG, tempo_limite_segundos: 120 });
    const qc = new QueryClient({
      defaultOptions: { queries: { retry: false }, mutations: { retry: false } },
    });

    render(
      <QueryClientProvider client={qc}>
        <ConfigTab projectId="proj-1" config={{ ...BASE_CONFIG, tempo_limite_segundos: 3665 }} />
      </QueryClientProvider>
    );

    const hours = screen.getByLabelText("Horas") as HTMLInputElement;
    const minutes = screen.getByLabelText("Minutos") as HTMLInputElement;
    const seconds = screen.getByLabelText("Segundos") as HTMLInputElement;

    expect(hours.value).toBe("1");
    expect(minutes.value).toBe("1");
    expect(seconds.value).toBe("5");

    fireEvent.change(hours, { target: { value: "0" } });
    fireEvent.change(minutes, { target: { value: "2" } });
    fireEvent.change(seconds, { target: { value: "0" } });

    fireEvent.click(screen.getByRole("button", { name: /Salvar Parâmetros/i }));

    await waitFor(() => {
      expect(apiFetchMock).toHaveBeenCalledWith(
        "/projects/proj-1/config",
        expect.objectContaining({
          method: "PUT",
          body: expect.any(String),
        })
      );
    });

    const payload = JSON.parse(apiFetchMock.mock.calls[0][1].body as string) as OptimizationConfig;
    expect(payload.tempo_limite_segundos).toBe(120);
  });
});
