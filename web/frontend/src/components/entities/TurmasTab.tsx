import { useMemo, useState } from "react";
import { useMutation, useQueryClient } from "@tanstack/react-query";
import { Pencil, Plus, Trash2, X } from "lucide-react";
import { apiFetch } from "../../api/client";
import { filterAndSortItems } from "../../domain/searchSort";
import {
  ALL_DAYS,
  computeImplicitBreaks,
  DAY_SHORT_LABELS,
  DEFAULT_SHIFT_SLOTS,
  formatInterval,
  totalClassMinutes,
} from "../../domain/time";
import type { Room, TimeSlot, Turma } from "../../types";
import { SearchSortTableBar } from "../common/SearchSortTableBar";

interface Props {
  projectId: string;
  turmas: Turma[];
  salas: Room[];
}

export function TurmasTab({ projectId, turmas, salas }: Props) {
  const qc = useQueryClient();
  const [search, setSearch] = useState("");
  const [sortBy, setSortBy] = useState("code");
  const [asc, setAsc] = useState(true);
  const [editing, setEditing] = useState<Turma | null>(null);
  const [creating, setCreating] = useState(false);

  const saveMutation = useMutation({
    mutationFn: async (turma: Turma & { isNew: boolean }) => {
      if (turma.isNew) {
        return apiFetch(`/projects/${projectId}/turmas`, { method: "POST", body: JSON.stringify(turma) });
      }
      return apiFetch(`/projects/${projectId}/turmas/${encodeURIComponent(turma.code)}`, {
        method: "PUT",
        body: JSON.stringify(turma),
      });
    },
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ["turmas", projectId] });
      qc.invalidateQueries({ queryKey: ["project", projectId] });
      setEditing(null);
      setCreating(false);
    },
  });

  const deleteMutation = useMutation({
    mutationFn: (code: string) =>
      apiFetch(`/projects/${projectId}/turmas/${encodeURIComponent(code)}`, { method: "DELETE" }),
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ["turmas", projectId] });
      qc.invalidateQueries({ queryKey: ["project", projectId] });
    },
  });

  const filtered = useMemo(
    () =>
      filterAndSortItems(
        turmas,
        search,
        (t) => `${t.code} ${t.nome} ${t.turno}`,
        (t) => (sortBy === "nome" ? t.nome : t.code),
        asc
      ),
    [asc, search, sortBy, turmas]
  );

  const openNew = async () => {
    const res = await apiFetch<{ next_id: string }>(`/projects/${projectId}/turmas/next-id`);
    setEditing({
      id: res.next_id,
      code: res.next_id,
      nome: "",
      turno: "MANHA",
      dias_letivos: [...ALL_DAYS],
      jornada_padrao: { slots: structuredClone(DEFAULT_SHIFT_SLOTS.MANHA) },
      excecoes_por_dia: {},
      sala_teorica_id: null,
      salas_teoricas_permitidas: salas.filter((s) => s.tipo === "TEORICA").map((s) => s.code),
    });
    setCreating(true);
  };

  return (
    <div className="space-y-3">
      <div className="flex items-center justify-between gap-3">
        <SearchSortTableBar
          searchQuery={search}
          onSearchChange={setSearch}
          totalCount={turmas.length}
          filteredCount={filtered.length}
          entityLabel="turmas"
          sortOptions={[
            { value: "code", label: "ID" },
            { value: "nome", label: "Nome" },
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
          <Plus className="h-4 w-4" /> Adicionar Turma
        </button>
      </div>

      <div className="overflow-x-auto rounded-lg border border-slate-200 bg-white dark:border-slate-800 dark:bg-slate-900">
        <table className="w-full text-left text-sm">
          <thead className="bg-slate-50 text-xs uppercase text-slate-500 dark:bg-slate-800/60">
            <tr>
              <th className="px-4 py-2.5">ID</th>
              <th className="px-4 py-2.5">Nome</th>
              <th className="px-4 py-2.5">Turno</th>
              <th className="px-4 py-2.5">Sala Fixa</th>
              <th className="px-4 py-2.5">Slots / Minutos Letivos</th>
              <th className="px-4 py-2.5 text-right">Ações</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-slate-200 dark:divide-slate-800">
            {filtered.map((t) => {
              const slots = t.jornada_padrao?.slots || [];
              return (
                <tr key={t.code} className="hover:bg-slate-50 dark:hover:bg-slate-800/40">
                  <td className="px-4 py-2.5 font-mono text-xs font-semibold">{t.code}</td>
                  <td className="px-4 py-2.5 font-medium">{t.nome}</td>
                  <td className="px-4 py-2.5">{t.turno}</td>
                  <td className="px-4 py-2.5">{t.sala_teorica_id || "Automática"}</td>
                  <td className="px-4 py-2.5 text-xs text-slate-500">
                    {slots.length} slots • {totalClassMinutes(slots)} min/dia
                  </td>
                  <td className="px-4 py-2.5 text-right">
                    <button
                      onClick={() => {
                        setEditing(structuredClone(t));
                        setCreating(false);
                      }}
                      className="mr-2 rounded p-1 text-blue-600 hover:bg-blue-50 dark:hover:bg-blue-950"
                    >
                      <Pencil className="h-4 w-4" />
                    </button>
                    <button
                      onClick={() => {
                        if (window.confirm(`Remover turma ${t.nome}?`)) deleteMutation.mutate(t.code);
                      }}
                      className="rounded p-1 text-rose-600 hover:bg-rose-50 dark:hover:bg-rose-950"
                    >
                      <Trash2 className="h-4 w-4" />
                    </button>
                  </td>
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>

      {editing && (
        <TurmaModal
          turma={editing}
          salas={salas}
          isNew={creating}
          onClose={() => {
            setEditing(null);
            setCreating(false);
          }}
          onSave={(t) => saveMutation.mutate({ ...t, isNew: creating })}
        />
      )}
    </div>
  );
}

function TurmaModal({
  turma,
  salas,
  isNew,
  onClose,
  onSave,
}: {
  turma: Turma;
  salas: Room[];
  isNew: boolean;
  onClose: () => void;
  onSave: (t: Turma) => void;
}) {
  const [form, setForm] = useState<Turma>(turma);
  const teoricas = salas.filter((s) => s.tipo.toUpperCase() === "TEORICA");
  const slots = form.jornada_padrao?.slots || [];
  const breaks = computeImplicitBreaks(slots);

  const updateSlot = (idx: number, field: keyof TimeSlot, val: string) => {
    const next = [...slots];
    next[idx] = { ...next[idx], [field]: val };
    setForm({ ...form, jornada_padrao: { slots: next } });
  };

  const toggleDay = (d: string) => {
    const has = form.dias_letivos.includes(d);
    const next = has ? form.dias_letivos.filter((x) => x !== d) : [...form.dias_letivos, d];
    setForm({ ...form, dias_letivos: next });
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
      <div className="max-h-[90vh] w-full max-w-2xl overflow-y-auto rounded-xl bg-white p-5 shadow-2xl dark:bg-slate-900">
        <div className="flex items-center justify-between border-b border-slate-200 pb-3 dark:border-slate-800">
          <h3 className="text-base font-bold">{isNew ? "Nova Turma" : `Editar ${form.nome}`}</h3>
          <button onClick={onClose}><X className="h-5 w-5" /></button>
        </div>

        <div className="mt-4 space-y-4 text-sm">
          <div className="grid grid-cols-4 gap-3">
            <div>
              <label className="text-xs font-medium text-slate-500">ID</label>
              <input disabled={!isNew} value={form.code} onChange={(e) => setForm({ ...form, code: e.target.value })} className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5 disabled:opacity-60 dark:border-slate-700 dark:bg-slate-800" />
            </div>
            <div className="col-span-2">
              <label className="text-xs font-medium text-slate-500">Nome</label>
              <input value={form.nome} onChange={(e) => setForm({ ...form, nome: e.target.value })} className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5 dark:border-slate-700 dark:bg-slate-800" />
            </div>
            <div>
              <label className="text-xs font-medium text-slate-500">Turno</label>
              <select value={form.turno} onChange={(e) => setForm({ ...form, turno: e.target.value })} className="mt-1 w-full rounded border border-slate-300 bg-white px-2 py-1.5 dark:border-slate-700 dark:bg-slate-800">
                <option value="MANHA">MANHA</option>
                <option value="TARDE">TARDE</option>
                <option value="NOITE">NOITE</option>
                <option value="INTEGRAL">INTEGRAL</option>
              </select>
            </div>
          </div>

          <div>
            <label className="text-xs font-medium text-slate-500">Dias Letivos</label>
            <div className="mt-1 flex gap-2">
              {ALL_DAYS.map((d) => (
                <label key={d} className="inline-flex items-center gap-1.5 rounded border border-slate-200 px-2.5 py-1 text-xs dark:border-slate-700">
                  <input type="checkbox" checked={form.dias_letivos.includes(d)} onChange={() => toggleDay(d)} />
                  {DAY_SHORT_LABELS[d]}
                </label>
              ))}
            </div>
          </div>

          <div>
            <label className="text-xs font-medium text-slate-500">Sala Teórica Fixa (ou Automática)</label>
            <select
              value={form.sala_teorica_id || ""}
              onChange={(e) => setForm({ ...form, sala_teorica_id: e.target.value || null })}
              className="mt-1 w-full rounded border border-slate-300 bg-white px-3 py-1.5 dark:border-slate-700 dark:bg-slate-800"
            >
              <option value="">Escolha automática pelo solver</option>
              {teoricas.map((s) => (
                <option key={s.code} value={s.code}>{s.nome} ({s.code})</option>
              ))}
            </select>
          </div>

          <div>
            <div className="flex items-center justify-between">
              <span className="text-xs font-semibold uppercase text-slate-500">
                Slots Letivos Padrão ({totalClassMinutes(slots)} min/dia)
              </span>
              <div className="flex gap-2">
                <button
                  type="button"
                  onClick={() => setForm({ ...form, jornada_padrao: { slots: structuredClone(DEFAULT_SHIFT_SLOTS[form.turno] || DEFAULT_SHIFT_SLOTS.MANHA) } })}
                  className="text-xs text-slate-500 hover:underline"
                >
                  Carregar padrão {form.turno}
                </button>
                <button
                  type="button"
                  onClick={() => setForm({ ...form, jornada_padrao: { slots: [...slots, { inicio: "08:00", fim: "08:50" }] } })}
                  className="text-xs font-semibold text-blue-600 hover:underline"
                >
                  + Adicionar Slot
                </button>
              </div>
            </div>
            <div className="mt-2 grid grid-cols-2 gap-2">
              {slots.map((s, idx) => (
                <div key={idx} className="flex items-center justify-between rounded border border-slate-200 px-2.5 py-1.5 dark:border-slate-700">
                  <span className="text-xs text-slate-400">#{idx + 1}</span>
                  <div className="flex items-center gap-1">
                    <input type="time" value={s.inicio} onChange={(e) => updateSlot(idx, "inicio", e.target.value)} className="bg-transparent text-xs" />
                    <span>-</span>
                    <input type="time" value={s.fim} onChange={(e) => updateSlot(idx, "fim", e.target.value)} className="bg-transparent text-xs" />
                  </div>
                  <button
                    type="button"
                    onClick={() => setForm({ ...form, jornada_padrao: { slots: slots.filter((_, i) => i !== idx) } })}
                    className="text-rose-500"
                  >
                    <X className="h-3.5 w-3.5" />
                  </button>
                </div>
              ))}
            </div>
            {breaks.length > 0 && (
              <div className="mt-2 text-xs text-amber-700 dark:text-amber-300">
                Intervalos implícitos detectados: {breaks.map((b) => formatInterval(b.start_min, b.end_min)).join(" • ")}
              </div>
            )}
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
