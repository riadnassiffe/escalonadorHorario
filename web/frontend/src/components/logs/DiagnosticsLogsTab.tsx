import type { OptimizationJob, TimetableResponse } from "../../types";

interface Props {
  jobs: OptimizationJob[];
  timetable?: TimetableResponse;
}

export function DiagnosticsLogsTab({ jobs, timetable }: Props) {
  const latestJob = jobs[0];

  return (
    <div className="grid gap-4 lg:grid-cols-3">
      <div className="rounded-xl border border-slate-200 bg-white p-4 dark:border-slate-800 dark:bg-slate-900">
        <h3 className="text-sm font-bold uppercase text-slate-500">Histórico de Execuções (Celery)</h3>
        <div className="mt-3 space-y-2">
          {jobs.length === 0 ? (
            <p className="text-xs text-slate-500">Nenhuma execução registrada.</p>
          ) : (
            jobs.map((j) => (
              <div key={j.id} className="rounded-lg border border-slate-200 p-2.5 text-xs dark:border-slate-800">
                <div className="flex items-center justify-between font-semibold">
                  <span>{j.algorithm}</span>
                  <span
                    className={`rounded px-1.5 py-0.5 text-[10px] ${
                      j.status === "COMPLETED"
                        ? "bg-emerald-100 text-emerald-700 dark:bg-emerald-950 dark:text-emerald-300"
                        : j.status === "RUNNING" || j.status === "PENDING"
                        ? "bg-blue-100 text-blue-700 dark:bg-blue-950 dark:text-blue-300"
                        : "bg-rose-100 text-rose-700 dark:bg-rose-950 dark:text-rose-300"
                    }`}
                  >
                    {j.status}
                  </span>
                </div>
                <div className="mt-1 text-slate-500 truncate">{j.progress_message}</div>
              </div>
            ))
          )}
        </div>
      </div>

      <div className="lg:col-span-2 rounded-xl border border-slate-200 bg-slate-950 p-4 text-slate-100 dark:border-slate-800">
        <h3 className="text-xs font-bold uppercase tracking-wider text-slate-400">
          Console de Diagnóstico e Logs do Solver
        </h3>
        <pre className="mt-3 max-h-[480px] overflow-y-auto whitespace-pre-wrap font-mono text-xs leading-relaxed text-emerald-300">
          {[
            ...(timetable?.mensagens_diagnostico || []),
            ...(latestJob?.logs || []),
            latestJob?.error_detail ? `[ERRO] ${latestJob.error_detail}` : "",
          ]
            .filter(Boolean)
            .join("\n") || "Nenhum log disponível ainda."}
        </pre>
      </div>
    </div>
  );
}
