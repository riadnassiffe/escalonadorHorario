import { useState } from "react";
import { Play, X } from "lucide-react";
import type { OptimizationConfig } from "../../types";
import { DurationHmsInput } from "../common/DurationHmsInput";

const ALGO_INFO: Record<string, { label: string; desc: string }> = {
  CP_SAT: {
    label: "Google OR-Tools CP-SAT (Recomendado)",
    desc: "Programação por restrições exata com aprendizado de cláusulas (CDCL). Melhor equilíbrio entre qualidade ótima e velocidade.",
  },
  HIGHS: {
    label: "HiGHS (MILP Alta Performance)",
    desc: "Programação Linear Inteira Mista com Dual Simplex paralelo e Branch-and-Cut.",
  },
  SCIP: {
    label: "SCIP (Constraint Integer Programming)",
    desc: "Solver exato Branch-Cut-and-Price com suporte a cortes de clique e Warm Start.",
  },
  CUOPT: {
    label: "NVIDIA cuOpt (GPU-Accelerated MILP)",
    desc: "Otimização acelerada por GPU NVIDIA CUDA com fallback automático para Dual Simplex.",
  },
  GREEDY_HEURISTIC: {
    label: "Heurística Construtiva Gulosa (Rápido)",
    desc: "Alocação imediata por grau de restrição. Ideal para rascunhos rápidos ou Warm Start.",
  },
};

interface Props {
  config?: OptimizationConfig;
  onClose: () => void;
  onLaunch: (payload: Record<string, any>) => void;
}

export function OptimizeModal({ config, onClose, onLaunch }: Props) {
  const [algoritmo, setAlgoritmo] = useState(config?.algoritmo || "CP_SAT");
  const [escopo, setEscopo] = useState(config?.escopo_semestre || "TODAS_SEM_FILTRO");
  const [syncAnuais, setSyncAnuais] = useState(config?.sincronizar_anuais_entre_semestres ?? true);
  const [timeLimit, setTimeLimit] = useState(config?.tempo_limite_segundos ?? 30);
  const [maxMem, setMaxMem] = useState(config?.max_memoria_mb ?? 2048);
  const [warmStart, setWarmStart] = useState(config?.usar_distribuicao_como_entrada ?? true);

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
      <div className="w-full max-w-lg rounded-xl bg-white p-5 shadow-2xl dark:bg-slate-900">
        <div className="flex items-center justify-between border-b border-slate-200 pb-3 dark:border-slate-800">
          <h3 className="text-base font-bold">⚡ Executar Otimização Assíncrona (Celery)</h3>
          <button onClick={onClose}><X className="h-5 w-5" /></button>
        </div>

        <div className="mt-4 space-y-3 text-sm">
          <div>
            <label className="text-xs font-medium text-slate-500">Algoritmo / Motor de Resolução</label>
            <select value={algoritmo} onChange={(e) => setAlgoritmo(e.target.value)} className="mt-1 w-full rounded-md border border-slate-300 bg-white px-3 py-2 dark:border-slate-700 dark:bg-slate-800">
              {Object.entries(ALGO_INFO).map(([k, v]) => (
                <option key={k} value={k}>{v.label}</option>
              ))}
            </select>
            <p className="mt-1.5 rounded bg-slate-100 p-2 text-xs text-slate-600 dark:bg-slate-800 dark:text-slate-300">
              {ALGO_INFO[algoritmo]?.desc}
            </p>
          </div>

          <div>
            <label className="text-xs font-medium text-slate-500">Escopo Semestral</label>
            <select value={escopo} onChange={(e) => setEscopo(e.target.value)} className="mt-1 w-full rounded-md border border-slate-300 bg-white px-3 py-2 dark:border-slate-700 dark:bg-slate-800">
              <option value="TODAS_SEM_FILTRO">Todas as Disciplinas (Geral)</option>
              <option value="SEMESTRE_1">1º Semestre (Anuais + 1º Sem)</option>
              <option value="SEMESTRE_2">2º Semestre (Anuais + 2º Sem)</option>
              <option value="AMBOS_INTEGRADO">Ambos os Semestres (Integrado)</option>
            </select>
          </div>

          <div className="space-y-3">
            <div>
              <label className="text-xs font-medium text-slate-500">Tempo Limite (h / min / s)</label>
              <DurationHmsInput
                valueSeconds={timeLimit}
                onChangeSeconds={setTimeLimit}
                idPrefix="optimize-time-limit"
              />
            </div>
            <div>
              <label className="text-xs font-medium text-slate-500">Memória Máx (MB)</label>
              <input
                type="number"
                min={256}
                value={maxMem}
                onChange={(e) => setMaxMem(Number(e.target.value))}
                className="mt-1 w-full rounded-md border border-slate-300 px-3 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
          </div>

          <label className="flex items-center gap-2">
            <input type="checkbox" checked={syncAnuais} onChange={(e) => setSyncAnuais(e.target.checked)} />
            Sincronizar disciplinas anuais entre 1º e 2º semestres
          </label>
          <label className="flex items-center gap-2">
            <input type="checkbox" checked={warmStart} onChange={(e) => setWarmStart(e.target.checked)} />
            Usar grade atual como solução inicial (Warm Start)
          </label>
        </div>

        <div className="mt-5 flex justify-end gap-2">
          <button onClick={onClose} className="rounded-md border border-slate-300 px-4 py-2 text-sm dark:border-slate-700">Cancelar</button>
          <button
            onClick={() =>
              onLaunch({
                algoritmo,
                escopo_semestre: escopo,
                sincronizar_anuais_entre_semestres: syncAnuais,
                tempo_limite_segundos: timeLimit,
                max_memoria_mb: maxMem,
                usar_distribuicao_como_entrada: warmStart,
              })
            }
            className="inline-flex items-center gap-1.5 rounded-md bg-blue-600 px-4 py-2 text-sm font-semibold text-white hover:bg-blue-700"
          >
            <Play className="h-4 w-4" /> Iniciar Otimização
          </button>
        </div>
      </div>
    </div>
  );
}
