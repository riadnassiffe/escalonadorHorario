import { useMemo, useState } from "react";
import { useMutation, useQueryClient } from "@tanstack/react-query";
import { Pencil, Plus, Trash2, X } from "lucide-react";
import { apiFetch } from "../../api/client";
import { filterAndSortItems } from "../../domain/searchSort";
import type { Disciplina, Professor, Room, Turma } from "../../types";
import { SearchSortTableBar } from "../common/SearchSortTableBar";

interface Props {
  projectId: string;
  disciplinas: Disciplina[];
  professores: Professor[];
  turmas: Turma[];
  salas: Room[];
}

export function DisciplinasTab({ projectId, disciplinas, professores, turmas, salas }: Props) {
  const qc = useQueryClient();
  const [search, setSearch] = useState("");
  const [sortBy, setSortBy] = useState("code");
  const [asc, setAsc] = useState(true);
  const [editing, setEditing] = useState<Disciplina | null>(null);
  const [creating, setCreating] = useState(false);

  const saveMutation = useMutation({
    mutationFn: async (d: Disciplina & { isNew: boolean }) => {
      if (d.isNew) {
        return apiFetch(`/projects/${projectId}/disciplinas`, { method: "POST", body: JSON.stringify(d) });
      }
      return apiFetch(`/projects/${projectId}/disciplinas/${encodeURIComponent(d.code)}`, {
        method: "PUT",
        body: JSON.stringify(d),
      });
    },
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ["disciplinas", projectId] });
      qc.invalidateQueries({ queryKey: ["project", projectId] });
      setEditing(null);
      setCreating(false);
    },
  });

  const deleteMutation = useMutation({
    mutationFn: (code: string) =>
      apiFetch(`/projects/${projectId}/disciplinas/${encodeURIComponent(code)}`, { method: "DELETE" }),
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ["disciplinas", projectId] });
      qc.invalidateQueries({ queryKey: ["project", projectId] });
    },
  });

  const filtered = useMemo(
    () =>
      filterAndSortItems(
        disciplinas,
        search,
        (d) => `${d.code} ${d.nome} ${d.turmas.join(" ")} ${d.professores.join(" ")}`,
        (d) => (sortBy === "nome" ? d.nome : sortBy === "aulas" ? d.aulas_semanais : d.code),
        asc
      ),
    [asc, disciplinas, search, sortBy]
  );

  const openNew = async () => {
    const res = await apiFetch<{ next_id: string }>(`/projects/${projectId}/disciplinas/next-id`);
    setEditing({
      id: res.next_id,
      code: res.next_id,
      nome: "",
      tipo: "TEORICA",
      duracao_minutos: 50,
      aulas_semanais: 4,
      regime: "ANUAL",
      semestre: "AMBOS",
      professores: professores[0] ? [professores[0].code] : [],
      turmas: turmas[0] ? [turmas[0].code] : [],
      turnos_permitidos: ["MANHA"],
      salas_alternativas: [],
      exigir_aulas_consecutivas: false,
      max_aulas_por_dia_turma: 2,
    });
    setCreating(true);
  };

  return (
    <div className="space-y-3">
      <div className="flex items-center justify-between gap-3">
        <SearchSortTableBar
          searchQuery={search}
          onSearchChange={setSearch}
          totalCount={disciplinas.length}
          filteredCount={filtered.length}
          entityLabel="disciplinas"
          sortOptions={[
            { value: "code", label: "ID" },
            { value: "nome", label: "Nome" },
            { value: "aulas", label: "Aulas/Sem" },
          ]}
          sortBy={sortBy}
          onSortByChange={setSortBy}
          ascending={asc}
          onToggleOrder={() => setAsc((s) => !s)}
        />
        <button
          onClick={openNew}
          className="inline-flex shrink-0 items-center gap-1.5 rounded-md bg-blue-600 px-3.5 py-2 text-sm font-medium text-white hover:bg-blue-700"
        >
          <Plus className="h-4 w-4" /> Adicionar Disciplina
        </button>
      </div>

      <div className="overflow-x-auto rounded-lg border border-slate-200 bg-white dark:border-slate-800 dark:bg-slate-900">
        <table className="w-full text-left text-sm">
          <thead className="bg-slate-50 text-xs uppercase text-slate-500 dark:bg-slate-800/60">
            <tr>
              <th className="px-4 py-2.5">ID</th>
              <th className="px-4 py-2.5">Nome</th>
              <th className="px-4 py-2.5">Regime</th>
              <th className="px-4 py-2.5">Duração / Semanal</th>
              <th className="px-4 py-2.5">Turmas / Profs</th>
              <th className="px-4 py-2.5 text-right">Ações</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-slate-200 dark:divide-slate-800">
            {filtered.map((d) => (
              <tr key={d.code} className="hover:bg-slate-50 dark:hover:bg-slate-800/40">
                <td className="px-4 py-2.5 font-mono text-xs font-semibold">{d.code}</td>
                <td className="px-4 py-2.5 font-medium">
                  {d.nome} <span className="text-xs text-slate-400">({d.tipo})</span>
                </td>
                <td className="px-4 py-2.5 text-xs">
                  {d.regime === "ANUAL" ? "Anual" : `Semestral (${d.semestre}º)`}
                </td>
                <td className="px-4 py-2.5 text-xs">
                  {d.duracao_minutos}m • {d.aulas_semanais} aulas/sem
                </td>
                <td className="px-4 py-2.5 text-xs text-slate-500">
                  {d.turmas.join(", ")} | {d.professores.join(", ")}
                </td>
                <td className="px-4 py-2.5 text-right">
                  <button onClick={() => { setEditing(structuredClone(d)); setCreating(false); }} className="mr-2 rounded p-1 text-blue-600 hover:bg-blue-50 dark:hover:bg-blue-950">
                    <Pencil className="h-4 w-4" />
                  </button>
                  <button
                    onClick={() => { if (window.confirm(`Remover disciplina ${d.nome}?`)) deleteMutation.mutate(d.code); }}
                    className="rounded p-1 text-rose-600 hover:bg-rose-50 dark:hover:bg-rose-950"
                  >
                    <Trash2 className="h-4 w-4" />
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {editing && (
        <DisciplinaModal
          disc={editing}
          professores={professores}
          turmas={turmas}
          salas={salas}
          isNew={creating}
          onClose={() => { setEditing(null); setCreating(false); }}
          onSave={(d) => saveMutation.mutate({ ...d, isNew: creating })}
        />
      )}
    </div>
  );
}

function DisciplinaModal({
  disc,
  professores,
  turmas,
  salas,
  isNew,
  onClose,
  onSave,
}: {
  disc: Disciplina;
  professores: Professor[];
  turmas: Turma[];
  salas: Room[];
  isNew: boolean;
  onClose: () => void;
  onSave: (d: Disciplina) => void;
}) {
  const [form, setForm] = useState<Disciplina>(disc);

  const toggleArr = (arr: string[], val: string) =>
    arr.includes(val) ? arr.filter((x) => x !== val) : [...arr, val];

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
      <div className="max-h-[90vh] w-full max-w-2xl overflow-y-auto rounded-xl bg-white p-5 shadow-2xl dark:bg-slate-900">
        <div className="flex items-center justify-between border-b border-slate-200 pb-3 dark:border-slate-800">
          <h3 className="text-base font-bold">{isNew ? "Nova Disciplina" : `Editar ${form.nome}`}</h3>
          <button onClick={onClose}><X className="h-5 w-5" /></button>
        </div>

        <div className="mt-4 space-y-3 text-sm">
          <div className="grid grid-cols-3 gap-3">
            <div>
              <label className="text-xs text-slate-500">ID</label>
              <input disabled={!isNew} value={form.code} onChange={(e) => setForm({ ...form, code: e.target.value })} className="mt-1 w-full rounded border border-slate-300 px-2.5 py-1.5 disabled:opacity-60 dark:border-slate-700 dark:bg-slate-800" />
            </div>
            <div className="col-span-2">
              <label className="text-xs text-slate-500">Nome</label>
              <input value={form.nome} onChange={(e) => setForm({ ...form, nome: e.target.value })} className="mt-1 w-full rounded border border-slate-300 px-2.5 py-1.5 dark:border-slate-700 dark:bg-slate-800" />
            </div>
          </div>

          <div className="grid grid-cols-4 gap-2">
            <div>
              <label className="text-xs text-slate-500">Tipo</label>
              <select value={form.tipo} onChange={(e) => setForm({ ...form, tipo: e.target.value })} className="mt-1 w-full rounded border border-slate-300 bg-white px-2 py-1.5 dark:border-slate-700 dark:bg-slate-800">
                <option value="TEORICA">TEORICA</option>
                <option value="LABORATORIO">LABORATORIO</option>
                <option value="ESPECIAL">ESPECIAL</option>
              </select>
            </div>
            <div>
              <label className="text-xs text-slate-500">Regime</label>
              <select value={form.regime} onChange={(e) => setForm({ ...form, regime: e.target.value, semestre: e.target.value === "ANUAL" ? "AMBOS" : "1" })} className="mt-1 w-full rounded border border-slate-300 bg-white px-2 py-1.5 dark:border-slate-700 dark:bg-slate-800">
                <option value="ANUAL">ANUAL</option>
                <option value="SEMESTRAL">SEMESTRAL</option>
              </select>
            </div>
            <div>
              <label className="text-xs text-slate-500">Semestre</label>
              <select disabled={form.regime === "ANUAL"} value={form.semestre} onChange={(e) => setForm({ ...form, semestre: e.target.value })} className="mt-1 w-full rounded border border-slate-300 bg-white px-2 py-1.5 disabled:opacity-50 dark:border-slate-700 dark:bg-slate-800">
                <option value="1">1º Semestre</option>
                <option value="2">2º Semestre</option>
                <option value="AMBOS">Ambos</option>
              </select>
            </div>
            <div>
              <label className="text-xs text-slate-500">Duração (min)</label>
              <input type="number" min={5} step={5} value={form.duracao_minutos} onChange={(e) => setForm({ ...form, duracao_minutos: Number(e.target.value) })} className="mt-1 w-full rounded border border-slate-300 px-2 py-1.5 dark:border-slate-700 dark:bg-slate-800" />
            </div>
          </div>

          <div className="grid grid-cols-3 gap-3">
            <div>
              <label className="text-xs text-slate-500">Aulas Semanais</label>
              <input type="number" min={1} value={form.aulas_semanais} onChange={(e) => setForm({ ...form, aulas_semanais: Number(e.target.value) })} className="mt-1 w-full rounded border border-slate-300 px-2.5 py-1.5 dark:border-slate-700 dark:bg-slate-800" />
            </div>
            <div>
              <label className="text-xs text-slate-500">Máx. Aulas/Dia Turma</label>
              <input type="number" min={0} value={form.max_aulas_por_dia_turma} onChange={(e) => setForm({ ...form, max_aulas_por_dia_turma: Number(e.target.value) })} className="mt-1 w-full rounded border border-slate-300 px-2.5 py-1.5 dark:border-slate-700 dark:bg-slate-800" />
            </div>
            <label className="mt-5 flex items-center gap-2 text-xs">
              <input type="checkbox" checked={form.exigir_aulas_consecutivas} onChange={(e) => setForm({ ...form, exigir_aulas_consecutivas: e.target.checked })} />
              Exigir bloco consecutivo
            </label>
          </div>

          <div>
            <div className="text-xs font-medium text-slate-500">Professores</div>
            <div className="mt-1 flex flex-wrap gap-1.5">
              {professores.map((p) => (
                <button
                  key={p.code}
                  type="button"
                  onClick={() => setForm({ ...form, professores: toggleArr(form.professores, p.code) })}
                  className={`rounded-full px-2.5 py-1 text-xs border ${
                    form.professores.includes(p.code)
                      ? "border-blue-600 bg-blue-600 text-white"
                      : "border-slate-300 dark:border-slate-700"
                  }`}
                >
                  {p.nome}
                </button>
              ))}
            </div>
          </div>

          <div>
            <div className="text-xs font-medium text-slate-500">Turmas</div>
            <div className="mt-1 flex flex-wrap gap-1.5">
              {turmas.map((t) => (
                <button
                  key={t.code}
                  type="button"
                  onClick={() => setForm({ ...form, turmas: toggleArr(form.turmas, t.code) })}
                  className={`rounded-full px-2.5 py-1 text-xs border ${
                    form.turmas.includes(t.code)
                      ? "border-blue-600 bg-blue-600 text-white"
                      : "border-slate-300 dark:border-slate-700"
                  }`}
                >
                  {t.nome}
                </button>
              ))}
            </div>
          </div>

          {form.tipo !== "TEORICA" && (
            <div>
              <div className="text-xs font-medium text-slate-500">Salas / Laboratórios Alternativos Permitidos</div>
              <div className="mt-1 flex flex-wrap gap-1.5">
                {salas.map((r) => {
                  const selected = form.salas_alternativas.some((combo) => combo.includes(r.code));
                  return (
                    <button
                      key={r.code}
                      type="button"
                      onClick={() => {
                        const next = selected
                          ? form.salas_alternativas.filter((c) => !c.includes(r.code))
                          : [...form.salas_alternativas, [r.code]];
                        setForm({ ...form, salas_alternativas: next });
                      }}
                      className={`rounded-full px-2.5 py-1 text-xs border ${
                        selected ? "border-pink-600 bg-pink-600 text-white" : "border-slate-300 dark:border-slate-700"
                      }`}
                    >
                      {r.nome} ({r.code})
                    </button>
                  );
                })}
              </div>
            </div>
          )}
        </div>

        <div className="mt-5 flex justify-end gap-2">
          <button onClick={onClose} className="rounded-md border border-slate-300 px-4 py-2 text-sm dark:border-slate-700">Cancelar</button>
          <button
            disabled={!form.nome.trim()}
            onClick={() => onSave(form)}
            className="rounded-md bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-700 disabled:opacity-50"
          >
            Salvar
          </button>
        </div>
      </div>
    </div>
  );
}
