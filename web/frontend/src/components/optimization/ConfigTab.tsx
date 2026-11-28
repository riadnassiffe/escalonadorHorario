import { useEffect, useState } from "react";
import { useMutation, useQueryClient } from "@tanstack/react-query";
import { Save } from "lucide-react";
import { apiFetch } from "../../api/client";
import type { OptimizationConfig } from "../../types";
import { DurationHmsInput } from "../common/DurationHmsInput";

interface Props {
  projectId: string;
  config?: OptimizationConfig;
}

export function ConfigTab({ projectId, config }: Props) {
  const qc = useQueryClient();
  const [form, setForm] = useState<OptimizationConfig | null>(config || null);
  const [savedMsg, setSavedMsg] = useState("");

  useEffect(() => {
    if (config) setForm(config);
  }, [config]);

  const mutation = useMutation({
    mutationFn: (payload: OptimizationConfig) =>
      apiFetch<OptimizationConfig>(`/projects/${projectId}/config`, {
        method: "PUT",
        body: JSON.stringify(payload),
      }),
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ["config", projectId] });
      setSavedMsg("Parâmetros salvos com sucesso.");
      setTimeout(() => setSavedMsg(""), 3000);
    },
  });

  if (!form) return null;

  const lambdas: Array<{ key: keyof OptimizationConfig; label: string }> = [
    { key: "lambda_janelas", label: "λ1 — Reduzir Janelas de Professores (J)" },
    { key: "lambda_dias", label: "λ2 — Concentrar Dias Trabalhados (D)" },
    { key: "lambda_preferencias", label: "λ3 — Preferências de Horário (P)" },
    { key: "lambda_preferencia_dias", label: "λ4 — Dias Preferidos dos Professores (W)" },
    { key: "lambda_preferencia_turnos", label: "λ5 — Turnos Pretendidos (T)" },
    { key: "lambda_lacunas_turma", label: "λ6 — Reduzir Lacunas nas Turmas (JG)" },
    { key: "lambda_aulas_consecutivas", label: "λ7 — Benefício Aulas Consecutivas (C)" },
  ];

  return (
    <div className="grid gap-4 lg:grid-cols-2">
      <div className="rounded-xl border border-slate-200 bg-white p-5 dark:border-slate-800 dark:bg-slate-900">
        <h3 className="text-sm font-bold uppercase tracking-wide text-slate-500">Pesos da Função Objetivo (λ)</h3>
        <div className="mt-4 space-y-3">
          {lambdas.map(({ key, label }) => (
            <div key={key}>
              <div className="flex justify-between text-xs font-medium">
                <span>{label}</span>
                <span className="font-mono font-bold text-blue-600">{String(form[key])}</span>
              </div>
              <input
                type="range"
                min={0}
                max={20}
                value={Number(form[key])}
                onChange={(e) => setForm({ ...form, [key]: Number(e.target.value) })}
                className="mt-1 w-full"
              />
            </div>
          ))}
        </div>
      </div>

      <div className="space-y-4">
        <div className="rounded-xl border border-slate-200 bg-white p-5 dark:border-slate-800 dark:bg-slate-900">
          <h3 className="text-sm font-bold uppercase tracking-wide text-slate-500">Estratégias e Restrições</h3>
          <div className="mt-3 space-y-3 text-sm">
            <div>
              <label className="text-xs text-slate-500">Modelo de Preferência de Dias (W)</label>
              <select
                value={form.modelo_preferencia_dias}
                onChange={(e) => setForm({ ...form, modelo_preferencia_dias: e.target.value })}
                className="mt-1 w-full rounded border border-slate-300 bg-white px-3 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              >
                <option value="POR_DIA_TRABALHADO">Por Dia Trabalhado</option>
                <option value="POR_MINUTO_AULA">Por Minuto de Aula</option>
              </select>
            </div>
            <label className="flex items-center gap-2">
              <input
                type="checkbox"
                checked={form.preenchimento_integral_jornada}
                onChange={(e) => setForm({ ...form, preenchimento_integral_jornada: e.target.checked })}
              />
              Preenchimento integral da jornada (R10)
            </label>
            <label className="flex items-center gap-2">
              <input
                type="checkbox"
                checked={form.distribuir_disciplinas_na_semana}
                onChange={(e) => setForm({ ...form, distribuir_disciplinas_na_semana: e.target.checked })}
              />
              Limitar aulas da mesma disciplina por dia (R10 — usa o máximo configurado em cada disciplina)
            </label>
            <label className="flex items-center gap-2">
              <input
                type="checkbox"
                checked={form.exigir_aulas_consecutivas_rigido ?? true}
                onChange={(e) => setForm({ ...form, exigir_aulas_consecutivas_rigido: e.target.checked })}
              />
              Exigir aulas consecutivas como restrição rígida (R11)
            </label>
          </div>
        </div>

        <div className="rounded-xl border border-slate-200 bg-white p-5 dark:border-slate-800 dark:bg-slate-900">
          <h3 className="text-sm font-bold uppercase tracking-wide text-slate-500">Recursos & Warm-Start</h3>
          <div className="mt-3 space-y-3 text-sm">
            <div>
              <label className="text-xs text-slate-500">Tempo Limite (h / min / s)</label>
              <DurationHmsInput
                valueSeconds={form.tempo_limite_segundos}
                onChangeSeconds={(sec) => setForm({ ...form, tempo_limite_segundos: sec })}
                idPrefix="config-time-limit"
              />
            </div>
            <div className="grid grid-cols-2 gap-3">
              <div>
                <label className="text-xs text-slate-500">Memória Máx (MB)</label>
                <input
                  type="number"
                  min={256}
                  value={form.max_memoria_mb}
                  onChange={(e) => setForm({ ...form, max_memoria_mb: Number(e.target.value) })}
                  className="mt-1 w-full rounded border border-slate-300 px-2.5 py-1.5 dark:border-slate-700 dark:bg-slate-800"
                />
              </div>
              <div>
                <label className="text-xs text-slate-500">Threads CPU</label>
                <input
                  type="number"
                  min={1}
                  value={form.num_threads}
                  onChange={(e) => setForm({ ...form, num_threads: Number(e.target.value) })}
                  className="mt-1 w-full rounded border border-slate-300 px-2.5 py-1.5 dark:border-slate-700 dark:bg-slate-800"
                />
              </div>
            </div>
          </div>
          <label className="mt-3 flex items-center gap-2 text-sm">
            <input
              type="checkbox"
              checked={form.usar_distribuicao_como_entrada}
              onChange={(e) => setForm({ ...form, usar_distribuicao_como_entrada: e.target.checked })}
            />
            Usar grade atual como solução inicial (Warm Start)
          </label>
        </div>

        <div className="flex items-center justify-end gap-3">
          {savedMsg && <span className="text-xs font-medium text-emerald-600">{savedMsg}</span>}
          <button
            onClick={() => mutation.mutate(form)}
            className="inline-flex items-center gap-2 rounded-lg bg-blue-600 px-5 py-2.5 text-sm font-semibold text-white hover:bg-blue-700"
          >
            <Save className="h-4 w-4" /> Salvar Parâmetros
          </button>
        </div>
      </div>
    </div>
  );
}
