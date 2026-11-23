import type { TimetableResponse } from "../../types";

interface Props {
  timetable?: TimetableResponse;
}

export function KpiBanner({ timetable }: Props) {
  const stats = timetable?.estatisticas || {};
  const status = String(stats.status || "NAO_EXECUTADO");
  const conflictCount = Object.keys(timetable?.conflitos || {}).length;

  const statusColors: Record<string, string> = {
    OPTIMAL: "bg-emerald-100 text-emerald-800 dark:bg-emerald-950 dark:text-emerald-300",
    FEASIBLE: "bg-blue-100 text-blue-800 dark:bg-blue-950 dark:text-blue-300",
    INFEASIBLE: "bg-rose-100 text-rose-800 dark:bg-rose-950 dark:text-rose-300",
    DADOS_INVALIDOS: "bg-rose-100 text-rose-800 dark:bg-rose-950 dark:text-rose-300",
    NAO_EXECUTADO: "bg-slate-100 text-slate-700 dark:bg-slate-800 dark:text-slate-300",
  };

  return (
    <div className="grid grid-cols-2 gap-2.5 sm:grid-cols-4 lg:grid-cols-7">
      <div className="rounded-lg border border-slate-200 bg-white p-3 dark:border-slate-800 dark:bg-slate-900">
        <div className="text-[11px] font-medium uppercase text-slate-500">Status Solver</div>
        <span className={`mt-1 inline-block rounded-full px-2.5 py-0.5 text-xs font-bold ${statusColors[status] || statusColors.NAO_EXECUTADO}`}>
          {status}
        </span>
      </div>

      <div className="rounded-lg border border-slate-200 bg-white p-3 dark:border-slate-800 dark:bg-slate-900">
        <div className="text-[11px] font-medium uppercase text-slate-500">Tempo</div>
        <div className="mt-1 text-lg font-bold">{Number(stats.tempo_execucao_ms || 0).toFixed(1)} ms</div>
      </div>

      <div className="rounded-lg border border-slate-200 bg-white p-3 dark:border-slate-800 dark:bg-slate-900">
        <div className="text-[11px] font-medium uppercase text-slate-500">Aulas Alocadas</div>
        <div className="mt-1 text-lg font-bold">
          {stats.total_aulas_alocadas ?? timetable?.alocacoes.length ?? 0} / {stats.total_aulas_esperadas ?? 0}
        </div>
      </div>

      <div className="rounded-lg border border-slate-200 bg-white p-3 dark:border-slate-800 dark:bg-slate-900">
        <div className="text-[11px] font-medium uppercase text-slate-500">Janelas Profs (J)</div>
        <div className="mt-1 text-lg font-bold">{stats.penalidade_janelas_J ?? 0} min</div>
      </div>

      <div className="rounded-lg border border-slate-200 bg-white p-3 dark:border-slate-800 dark:bg-slate-900">
        <div className="text-[11px] font-medium uppercase text-slate-500">Dias Profs (D)</div>
        <div className="mt-1 text-lg font-bold">{stats.penalidade_dias_D ?? 0}</div>
      </div>

      <div className="rounded-lg border border-slate-200 bg-white p-3 dark:border-slate-800 dark:bg-slate-900">
        <div className="text-[11px] font-medium uppercase text-slate-500">Pref. Dias (W)</div>
        <div className="mt-1 text-lg font-bold">{stats.penalidade_preferencia_dias_W ?? 0}</div>
      </div>

      <div className="rounded-lg border border-slate-200 bg-white p-3 dark:border-slate-800 dark:bg-slate-900">
        <div className="text-[11px] font-medium uppercase text-slate-500">Choques</div>
        <div className={`mt-1 text-lg font-bold ${conflictCount > 0 ? "text-rose-600" : "text-emerald-600"}`}>
          {conflictCount} {conflictCount === 1 ? "aula" : "aulas"}
        </div>
      </div>
    </div>
  );
}
