import { useMemo, useState } from "react";
import { useMutation, useQueryClient } from "@tanstack/react-query";
import { Pencil, Plus, Trash2, X } from "lucide-react";
import { apiFetch } from "../../api/client";
import { filterAndSortItems } from "../../domain/searchSort";
import { ALL_DAYS, DAY_SHORT_LABELS } from "../../domain/time";
import type { DayOfWeek, Professor, TimeSlot } from "../../types";
import { SearchSortTableBar } from "../common/SearchSortTableBar";

interface Props {
  projectId: string;
  professores: Professor[];
}

export function ProfessoresTab({ projectId, professores }: Props) {
  const qc = useQueryClient();
  const [search, setSearch] = useState("");
  const [sortBy, setSortBy] = useState("code");
  const [asc, setAsc] = useState(true);
  const [editing, setEditing] = useState<Professor | null>(null);
  const [creating, setCreating] = useState(false);

  const saveMutation = useMutation({
    mutationFn: async (prof: Omit<Professor, "id"> & { isNew: boolean }) => {
      if (prof.isNew) {
        return apiFetch(`/projects/${projectId}/professores`, { method: "POST", body: JSON.stringify(prof) });
      }
      return apiFetch(`/projects/${projectId}/professores/${encodeURIComponent(prof.code)}`, {
        method: "PUT",
        body: JSON.stringify(prof),
      });
    },
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ["professores", projectId] });
      qc.invalidateQueries({ queryKey: ["project", projectId] });
      setEditing(null);
      setCreating(false);
    },
  });

  const deleteMutation = useMutation({
    mutationFn: (code: string) =>
      apiFetch(`/projects/${projectId}/professores/${encodeURIComponent(code)}`, { method: "DELETE" }),
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ["professores", projectId] });
      qc.invalidateQueries({ queryKey: ["project", projectId] });
    },
  });

  const filtered = useMemo(
    () =>
      filterAndSortItems(
        professores,
        search,
        (p) => `${p.code} ${p.nome}`,
        (p) => (sortBy === "nome" ? p.nome : sortBy === "janela" ? p.peso_janela : p.code),
        asc
      ),
    [asc, professores, search, sortBy]
  );

  const openNew = async () => {
    const res = await apiFetch<{ next_id: string }>(`/projects/${projectId}/professores/next-id`);
    setEditing({
      id: res.next_id,
      code: res.next_id,
      nome: "",
      preferencia_dias: { SEG: 5, TER: 5, QUA: 5, QUI: 5, SEX: 5 },
      disponibilidade: Object.fromEntries(ALL_DAYS.map((d) => [d, [{ inicio: "07:30", fim: "12:00" }]])),
      turnos_pretendidos: Object.fromEntries(ALL_DAYS.map((d) => [d, ["MANHA", "TARDE", "NOITE"]])),
      carga_maxima_diaria_minutos: 0,
      max_aulas_por_dia: 0,
      peso_janela: 8,
    });
    setCreating(true);
  };

  return (
    <div className="space-y-3">
      <div className="flex items-center justify-between gap-3">
        <SearchSortTableBar
          searchQuery={search}
          onSearchChange={setSearch}
          totalCount={professores.length}
          filteredCount={filtered.length}
          entityLabel="professores"
          sortOptions={[
            { value: "code", label: "ID" },
            { value: "nome", label: "Nome" },
            { value: "janela", label: "Aversão Janela" },
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
          <Plus className="h-4 w-4" /> Adicionar Professor
        </button>
      </div>

      <div className="overflow-x-auto rounded-lg border border-slate-200 bg-white dark:border-slate-800 dark:bg-slate-900">
        <table className="w-full text-left text-sm">
          <thead className="bg-slate-50 text-xs uppercase text-slate-500 dark:bg-slate-800/60">
            <tr>
              <th className="px-4 py-2.5">ID</th>
              <th className="px-4 py-2.5">Nome</th>
              <th className="px-4 py-2.5">Aversão Janela</th>
              <th className="px-4 py-2.5">Carga Máx/Dia</th>
              <th className="px-4 py-2.5">Preferência Dias (0-10)</th>
              <th className="px-4 py-2.5 text-right">Ações</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-slate-200 dark:divide-slate-800">
            {filtered.map((p) => (
              <tr key={p.code} className="hover:bg-slate-50 dark:hover:bg-slate-800/40">
                <td className="px-4 py-2.5 font-mono text-xs font-semibold">{p.code}</td>
                <td className="px-4 py-2.5 font-medium">{p.nome}</td>
                <td className="px-4 py-2.5">{p.peso_janela}/10</td>
                <td className="px-4 py-2.5">
                  {p.carga_maxima_diaria_minutos > 0 ? `${p.carga_maxima_diaria_minutos} min` : "Sem limite"}
                </td>
                <td className="px-4 py-2.5 text-xs text-slate-500">
                  {ALL_DAYS.map((d) => `${DAY_SHORT_LABELS[d]}:${p.preferencia_dias[d] ?? 5}`).join(" • ")}
                </td>
                <td className="px-4 py-2.5 text-right">
                  <button
                    onClick={() => {
                      setEditing(structuredClone(p));
                      setCreating(false);
                    }}
                    className="mr-2 rounded p-1 text-blue-600 hover:bg-blue-50 dark:hover:bg-blue-950"
                  >
                    <Pencil className="h-4 w-4" />
                  </button>
                  <button
                    onClick={() => {
                      if (window.confirm(`Remover professor ${p.nome}?`)) deleteMutation.mutate(p.code);
                    }}
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
        <ProfessorModal
          prof={editing}
          isNew={creating}
          onClose={() => {
            setEditing(null);
            setCreating(false);
          }}
          onSave={(data) => saveMutation.mutate({ ...data, isNew: creating })}
        />
      )}
    </div>
  );
}

function ProfessorModal({
  prof,
  isNew,
  onClose,
  onSave,
}: {
  prof: Professor;
  isNew: boolean;
  onClose: () => void;
  onSave: (p: Professor) => void;
}) {
  const [form, setForm] = useState<Professor>(prof);

  const updateDisp = (day: DayOfWeek, idx: number, field: keyof TimeSlot, val: string) => {
    const list = [...(form.disponibilidade[day] || [])];
    list[idx] = { ...list[idx], [field]: val };
    setForm({ ...form, disponibilidade: { ...form.disponibilidade, [day]: list } });
  };

  const addDisp = (day: DayOfWeek) => {
    const list = [...(form.disponibilidade[day] || []), { inicio: "08:00", fim: "12:00" }];
    setForm({ ...form, disponibilidade: { ...form.disponibilidade, [day]: list } });
  };

  const removeDisp = (day: DayOfWeek, idx: number) => {
    const list = (form.disponibilidade[day] || []).filter((_, i) => i !== idx);
    setForm({ ...form, disponibilidade: { ...form.disponibilidade, [day]: list } });
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
      <div className="max-h-[90vh] w-full max-w-2xl overflow-y-auto rounded-xl bg-white p-5 shadow-2xl dark:bg-slate-900">
        <div className="flex items-center justify-between border-b border-slate-200 pb-3 dark:border-slate-800">
          <h3 className="text-base font-bold">{isNew ? "Novo Professor" : `Editar ${form.nome}`}</h3>
          <button onClick={onClose}><X className="h-5 w-5" /></button>
        </div>

        <div className="mt-4 space-y-4 text-sm">
          <div className="grid grid-cols-3 gap-3">
            <div>
              <label className="text-xs font-medium text-slate-500">ID</label>
              <input
                disabled={!isNew}
                value={form.code}
                onChange={(e) => setForm({ ...form, code: e.target.value })}
                className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5 disabled:opacity-60 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
            <div className="col-span-2">
              <label className="text-xs font-medium text-slate-500">Nome Completo</label>
              <input
                value={form.nome}
                onChange={(e) => setForm({ ...form, nome: e.target.value })}
                className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
          </div>

          <div className="grid grid-cols-3 gap-3">
            <div>
              <label className="text-xs font-medium text-slate-500">Aversão a Janelas ({form.peso_janela})</label>
              <input
                type="range"
                min={0}
                max={10}
                value={form.peso_janela}
                onChange={(e) => setForm({ ...form, peso_janela: Number(e.target.value) })}
                className="mt-2 w-full"
              />
            </div>
            <div>
              <label className="text-xs font-medium text-slate-500">Carga Máx. Diária (min)</label>
              <input
                type="number"
                min={0}
                value={form.carga_maxima_diaria_minutos}
                onChange={(e) => setForm({ ...form, carga_maxima_diaria_minutos: Number(e.target.value) })}
                className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
            <div>
              <label className="text-xs font-medium text-slate-500">Máx. Aulas/Dia</label>
              <input
                type="number"
                min={0}
                value={form.max_aulas_por_dia}
                onChange={(e) => setForm({ ...form, max_aulas_por_dia: Number(e.target.value) })}
                className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
          </div>

          <div>
            <div className="mb-1.5 text-xs font-semibold uppercase text-slate-500">Preferência por Dia (0 = Menos, 10 = Muito)</div>
            <div className="grid grid-cols-5 gap-2">
              {ALL_DAYS.map((d) => (
                <div key={d} className="rounded border border-slate-200 p-2 text-center dark:border-slate-700">
                  <div className="text-xs font-semibold">{DAY_SHORT_LABELS[d]} ({form.preferencia_dias[d] ?? 5})</div>
                  <input
                    type="range"
                    min={0}
                    max={10}
                    value={form.preferencia_dias[d] ?? 5}
                    onChange={(e) =>
                      setForm({
                        ...form,
                        preferencia_dias: { ...form.preferencia_dias, [d]: Number(e.target.value) },
                      })
                    }
                    className="mt-1 w-full"
                  />
                </div>
              ))}
            </div>
          </div>

          <div>
            <div className="mb-1.5 text-xs font-semibold uppercase text-slate-500">Disponibilidade por Dia</div>
            <div className="space-y-2">
              {ALL_DAYS.map((d) => (
                <div key={d} className="rounded border border-slate-200 p-2 dark:border-slate-800">
                  <div className="flex items-center justify-between">
                    <span className="text-xs font-bold">{DAY_SHORT_LABELS[d]}</span>
                    <button type="button" onClick={() => addDisp(d)} className="text-xs text-blue-600 hover:underline">
                      + Faixa
                    </button>
                  </div>
                  <div className="mt-1 flex flex-wrap gap-2">
                    {(form.disponibilidade[d] || []).map((slot, idx) => (
                      <div key={idx} className="flex items-center gap-1 rounded bg-slate-100 px-2 py-1 dark:bg-slate-800">
                        <input type="time" value={slot.inicio} onChange={(e) => updateDisp(d, idx, "inicio", e.target.value)} className="bg-transparent text-xs" />
                        <span>-</span>
                        <input type="time" value={slot.fim} onChange={(e) => updateDisp(d, idx, "fim", e.target.value)} className="bg-transparent text-xs" />
                        <button type="button" onClick={() => removeDisp(d, idx)} className="text-rose-500"><X className="h-3.5 w-3.5" /></button>
                      </div>
                    ))}
                  </div>
                </div>
              ))}
            </div>
          </div>
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
