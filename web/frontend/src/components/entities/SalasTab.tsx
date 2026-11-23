import { useMemo, useState } from "react";
import { useMutation, useQueryClient } from "@tanstack/react-query";
import { Pencil, Plus, Trash2, X } from "lucide-react";
import { apiFetch } from "../../api/client";
import { filterAndSortItems } from "../../domain/searchSort";
import type { Room } from "../../types";
import { SearchSortTableBar } from "../common/SearchSortTableBar";

interface Props {
  projectId: string;
  salas: Room[];
}

export function SalasTab({ projectId, salas }: Props) {
  const qc = useQueryClient();
  const [search, setSearch] = useState("");
  const [sortBy, setSortBy] = useState("code");
  const [asc, setAsc] = useState(true);
  const [editing, setEditing] = useState<Room | null>(null);
  const [creating, setCreating] = useState(false);

  const saveMutation = useMutation({
    mutationFn: async (room: Room & { isNew: boolean }) => {
      if (room.isNew) {
        return apiFetch(`/projects/${projectId}/salas`, { method: "POST", body: JSON.stringify(room) });
      }
      return apiFetch(`/projects/${projectId}/salas/${encodeURIComponent(room.code)}`, {
        method: "PUT",
        body: JSON.stringify(room),
      });
    },
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ["salas", projectId] });
      qc.invalidateQueries({ queryKey: ["project", projectId] });
      setEditing(null);
      setCreating(false);
    },
  });

  const deleteMutation = useMutation({
    mutationFn: (code: string) =>
      apiFetch(`/projects/${projectId}/salas/${encodeURIComponent(code)}`, { method: "DELETE" }),
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ["salas", projectId] });
      qc.invalidateQueries({ queryKey: ["project", projectId] });
    },
  });

  const filtered = useMemo(
    () =>
      filterAndSortItems(
        salas,
        search,
        (r) => `${r.code} ${r.nome} ${r.tipo} ${r.recursos.join(" ")}`,
        (r) => (sortBy === "nome" ? r.nome : sortBy === "capacidade" ? r.capacidade : r.code),
        asc
      ),
    [asc, salas, search, sortBy]
  );

  const openNew = async () => {
    const res = await apiFetch<{ next_id: string }>(`/projects/${projectId}/salas/next-id`);
    setEditing({ id: res.next_id, code: res.next_id, nome: "", tipo: "TEORICA", capacidade: 40, recursos: [] });
    setCreating(true);
  };

  return (
    <div className="space-y-3">
      <div className="flex items-center justify-between gap-3">
        <SearchSortTableBar
          searchQuery={search}
          onSearchChange={setSearch}
          totalCount={salas.length}
          filteredCount={filtered.length}
          entityLabel="salas"
          sortOptions={[
            { value: "code", label: "ID" },
            { value: "nome", label: "Nome" },
            { value: "capacidade", label: "Capacidade" },
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
          <Plus className="h-4 w-4" /> Adicionar Sala
        </button>
      </div>

      <div className="overflow-x-auto rounded-lg border border-slate-200 bg-white dark:border-slate-800 dark:bg-slate-900">
        <table className="w-full text-left text-sm">
          <thead className="bg-slate-50 text-xs uppercase text-slate-500 dark:bg-slate-800/60">
            <tr>
              <th className="px-4 py-2.5">ID</th>
              <th className="px-4 py-2.5">Nome</th>
              <th className="px-4 py-2.5">Tipo</th>
              <th className="px-4 py-2.5">Capacidade</th>
              <th className="px-4 py-2.5">Recursos</th>
              <th className="px-4 py-2.5 text-right">Ações</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-slate-200 dark:divide-slate-800">
            {filtered.map((r) => (
              <tr key={r.code} className="hover:bg-slate-50 dark:hover:bg-slate-800/40">
                <td className="px-4 py-2.5 font-mono text-xs font-semibold">{r.code}</td>
                <td className="px-4 py-2.5 font-medium">{r.nome}</td>
                <td className="px-4 py-2.5">{r.tipo}</td>
                <td className="px-4 py-2.5">{r.capacidade} alunos</td>
                <td className="px-4 py-2.5 text-xs text-slate-500">{r.recursos.join(", ") || "—"}</td>
                <td className="px-4 py-2.5 text-right">
                  <button onClick={() => { setEditing(structuredClone(r)); setCreating(false); }} className="mr-2 rounded p-1 text-blue-600 hover:bg-blue-50 dark:hover:bg-blue-950">
                    <Pencil className="h-4 w-4" />
                  </button>
                  <button
                    onClick={() => { if (window.confirm(`Remover sala ${r.nome}?`)) deleteMutation.mutate(r.code); }}
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
        <SalaModal
          room={editing}
          isNew={creating}
          onClose={() => { setEditing(null); setCreating(false); }}
          onSave={(r) => saveMutation.mutate({ ...r, isNew: creating })}
        />
      )}
    </div>
  );
}

function SalaModal({ room, isNew, onClose, onSave }: { room: Room; isNew: boolean; onClose: () => void; onSave: (r: Room) => void }) {
  const [form, setForm] = useState<Room>(room);
  const [recursosStr, setRecursosStr] = useState(room.recursos.join(", "));

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
      <div className="w-full max-w-md rounded-xl bg-white p-5 shadow-2xl dark:bg-slate-900">
        <div className="flex items-center justify-between border-b border-slate-200 pb-3 dark:border-slate-800">
          <h3 className="text-base font-bold">{isNew ? "Nova Sala" : `Editar ${form.nome}`}</h3>
          <button onClick={onClose}><X className="h-5 w-5" /></button>
        </div>
        <div className="mt-4 space-y-3 text-sm">
          <div className="grid grid-cols-3 gap-2">
            <div>
              <label className="text-xs text-slate-500">ID</label>
              <input disabled={!isNew} value={form.code} onChange={(e) => setForm({ ...form, code: e.target.value })} className="mt-1 w-full rounded border border-slate-300 px-2.5 py-1.5 disabled:opacity-60 dark:border-slate-700 dark:bg-slate-800" />
            </div>
            <div className="col-span-2">
              <label className="text-xs text-slate-500">Nome</label>
              <input value={form.nome} onChange={(e) => setForm({ ...form, nome: e.target.value })} className="mt-1 w-full rounded border border-slate-300 px-2.5 py-1.5 dark:border-slate-700 dark:bg-slate-800" />
            </div>
          </div>
          <div className="grid grid-cols-2 gap-2">
            <div>
              <label className="text-xs text-slate-500">Tipo</label>
              <select value={form.tipo} onChange={(e) => setForm({ ...form, tipo: e.target.value })} className="mt-1 w-full rounded border border-slate-300 bg-white px-2.5 py-1.5 dark:border-slate-700 dark:bg-slate-800">
                <option value="TEORICA">TEORICA</option>
                <option value="LABORATORIO">LABORATORIO</option>
                <option value="ESPECIAL">ESPECIAL</option>
              </select>
            </div>
            <div>
              <label className="text-xs text-slate-500">Capacidade</label>
              <input type="number" min={1} value={form.capacidade} onChange={(e) => setForm({ ...form, capacidade: Number(e.target.value) })} className="mt-1 w-full rounded border border-slate-300 px-2.5 py-1.5 dark:border-slate-700 dark:bg-slate-800" />
            </div>
          </div>
          <div>
            <label className="text-xs text-slate-500">Recursos (separados por vírgula)</label>
            <input value={recursosStr} onChange={(e) => setRecursosStr(e.target.value)} placeholder="Projetor, 30 PCs" className="mt-1 w-full rounded border border-slate-300 px-2.5 py-1.5 dark:border-slate-700 dark:bg-slate-800" />
          </div>
        </div>
        <div className="mt-5 flex justify-end gap-2">
          <button onClick={onClose} className="rounded-md border border-slate-300 px-4 py-2 text-sm dark:border-slate-700">Cancelar</button>
          <button
            disabled={!form.nome.trim()}
            onClick={() => onSave({ ...form, recursos: recursosStr.split(",").map((s) => s.trim()).filter(Boolean) })}
            className="rounded-md bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-700 disabled:opacity-50"
          >
            Salvar
          </button>
        </div>
      </div>
    </div>
  );
}
