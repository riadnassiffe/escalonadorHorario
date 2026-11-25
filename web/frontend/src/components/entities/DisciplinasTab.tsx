import { useEffect, useMemo, useState } from "react";
import { useMutation, useQueryClient } from "@tanstack/react-query";
import { AlertTriangle, ChevronDown, ChevronUp, Eye, EyeOff, Info, Pencil, Plus, Search, Trash2, X } from "lucide-react";
import { apiFetch } from "../../api/client";
import {
  computeDisciplinaRoomOptions,
  describeDisciplinaRooms,
  flattenSalasAlternativas,
  toggleRoomInSalasAlternativas,
} from "../../domain/disciplinaRooms";
import { filterAndSortItems, normalizeText } from "../../domain/searchSort";
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
  const [modalError, setModalError] = useState("");

  const turmasByCode = useMemo(
    () => Object.fromEntries(turmas.map((t) => [t.code, t])) as Record<string, Turma>,
    [turmas]
  );
  const salasByCode = useMemo(
    () => Object.fromEntries(salas.map((r) => [r.code, r])) as Record<string, Room>,
    [salas]
  );

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
      setModalError("");
    },
    onError: (err) => {
      setModalError(err instanceof Error ? err.message : "Erro ao salvar disciplina.");
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
        (d) => {
          const explicitRoomCodes = flattenSalasAlternativas(d.salas_alternativas);
          const roomNames = explicitRoomCodes.map((c) => salasByCode[c]?.nome || c).join(" ");
          const roomDesc = describeDisciplinaRooms(d, turmasByCode, salasByCode).label;
          return `${d.code} ${d.nome} ${d.turmas.join(" ")} ${d.professores.join(" ")} ${explicitRoomCodes.join(" ")} ${roomNames} ${roomDesc}`;
        },
        (d) => (sortBy === "nome" ? d.nome : sortBy === "aulas" ? d.aulas_semanais : d.code),
        asc
      ),
    [asc, disciplinas, salasByCode, search, sortBy, turmasByCode]
  );

  const openNew = async () => {
    const res = await apiFetch<{ next_id: string }>(`/projects/${projectId}/disciplinas/next-id`);
    setModalError("");
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
              <th className="px-4 py-2.5">Salas</th>
              <th className="px-4 py-2.5 text-right">Ações</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-slate-200 dark:divide-slate-800">
            {filtered.map((d) => {
              const roomInfo = describeDisciplinaRooms(d, turmasByCode, salasByCode);
              return (
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
                  <td className="px-4 py-2.5 text-xs">
                    {roomInfo.isExplicit ? (
                      <span className="inline-flex rounded-full bg-pink-100 px-2 py-0.5 text-[11px] font-medium text-pink-700 dark:bg-pink-950 dark:text-pink-300">
                        {roomInfo.label}
                      </span>
                    ) : (
                      <span className="text-slate-500">{roomInfo.label}</span>
                    )}
                  </td>
                  <td className="px-4 py-2.5 text-right whitespace-nowrap">
                    <button
                      aria-label={`Editar ${d.nome}`}
                      onClick={() => {
                        setModalError("");
                        setEditing(structuredClone(d));
                        setCreating(false);
                      }}
                      className="mr-2 rounded p-1 text-blue-600 hover:bg-blue-50 dark:hover:bg-blue-950"
                    >
                      <Pencil className="h-4 w-4" />
                    </button>
                    <button
                      aria-label={`Remover ${d.nome}`}
                      onClick={() => {
                        if (window.confirm(`Remover disciplina ${d.nome}?`)) deleteMutation.mutate(d.code);
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
        <DisciplinaModal
          disc={editing}
          professores={professores}
          turmas={turmas}
          salas={salas}
          isNew={creating}
          saving={saveMutation.isPending}
          error={modalError}
          onClose={() => {
            setEditing(null);
            setCreating(false);
            setModalError("");
          }}
          onSave={(d) => saveMutation.mutate({ ...d, isNew: creating })}
        />
      )}
    </div>
  );
}

interface SelectableEntityItem {
  code: string;
  nome: string;
  sublabel?: string;
}

function CollapsibleEntitySelector({
  label,
  singularEmptyLabel,
  items,
  selectedCodes,
  onToggle,
  defaultExpanded,
  searchPlaceholder,
  sectionTestId,
}: {
  label: string;
  singularEmptyLabel: string;
  items: SelectableEntityItem[];
  selectedCodes: string[];
  onToggle: (code: string) => void;
  defaultExpanded: boolean;
  searchPlaceholder: string;
  sectionTestId?: string;
}) {
  const [expanded, setExpanded] = useState(defaultExpanded);
  const [query, setQuery] = useState("");

  const itemsByCode = useMemo(
    () => Object.fromEntries(items.map((i) => [i.code, i])) as Record<string, SelectableEntityItem>,
    [items]
  );

  const filteredItems = useMemo(() => {
    const q = normalizeText(query.trim());
    if (!q) return items;
    return items.filter((item) =>
      normalizeText(`${item.code} ${item.nome} ${item.sublabel || ""}`).includes(q)
    );
  }, [items, query]);

  return (
    <div
      data-testid={sectionTestId}
      className="rounded-lg border border-slate-200 p-3 dark:border-slate-800"
    >
      <div className="flex flex-wrap items-center justify-between gap-2">
        <span className="text-xs font-semibold text-slate-700 dark:text-slate-200">
          {label} ({selectedCodes.length})
        </span>
        <button
          type="button"
          onClick={() => setExpanded((prev) => !prev)}
          aria-expanded={expanded}
          className="inline-flex items-center gap-1 rounded border border-slate-300 bg-white px-2 py-0.5 text-[11px] font-medium text-slate-700 hover:bg-slate-50 dark:border-slate-700 dark:bg-slate-800 dark:text-slate-200 dark:hover:bg-slate-700"
        >
          {expanded ? <EyeOff className="h-3 w-3" /> : <Eye className="h-3 w-3" />}
          {expanded ? "Esconder selecionáveis" : `Mostrar selecionáveis (${items.length})`}
        </button>
      </div>

      {/* Resumo dos itens já selecionados (sempre visível) */}
      <div className="mt-2 flex flex-wrap gap-1.5">
        {selectedCodes.length === 0 ? (
          <span className="text-xs italic text-slate-400">{singularEmptyLabel}</span>
        ) : (
          selectedCodes.map((code) => {
            const item = itemsByCode[code];
            const displayName = item ? item.nome : code;
            return (
              <span
                key={code}
                className="inline-flex items-center gap-1 rounded-full bg-blue-600 px-2.5 py-0.5 text-xs font-medium text-white"
              >
                <span>{displayName}</span>
                <button
                  type="button"
                  aria-label={`Remover ${displayName}`}
                  onClick={() => onToggle(code)}
                  className="rounded-full p-0.5 hover:bg-blue-700"
                >
                  <X className="h-3 w-3" />
                </button>
              </span>
            );
          })
        )}
      </div>

      {/* Lista completa selecionável (recolhível + busca rápida) */}
      {expanded && (
        <div className="mt-2.5 space-y-2 border-t border-slate-100 pt-2.5 dark:border-slate-800">
          <div className="relative">
            <Search className="pointer-events-none absolute left-2.5 top-2 h-3.5 w-3.5 text-slate-400" />
            <input
              type="text"
              value={query}
              onChange={(e) => setQuery(e.target.value)}
              placeholder={searchPlaceholder}
              className="w-full rounded border border-slate-300 bg-white py-1 pl-8 pr-2.5 text-xs dark:border-slate-700 dark:bg-slate-800"
            />
          </div>
          <div className="flex max-h-36 flex-wrap gap-1.5 overflow-y-auto pr-1">
            {filteredItems.length === 0 ? (
              <span className="text-xs text-slate-400">Nenhum registro encontrado.</span>
            ) : (
              filteredItems.map((item) => {
                const isSelected = selectedCodes.includes(item.code);
                return (
                  <button
                    key={item.code}
                    type="button"
                    onClick={() => onToggle(item.code)}
                    className={`rounded-full border px-2.5 py-1 text-xs transition ${
                      isSelected
                        ? "border-blue-600 bg-blue-600 text-white"
                        : "border-slate-300 hover:bg-slate-50 dark:border-slate-700 dark:hover:bg-slate-800"
                    }`}
                  >
                    {item.nome}
                    {item.sublabel ? ` (${item.sublabel})` : ""}
                  </button>
                );
              })
            )}
          </div>
        </div>
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
  saving,
  error,
  onClose,
  onSave,
}: {
  disc: Disciplina;
  professores: Professor[];
  turmas: Turma[];
  salas: Room[];
  isNew: boolean;
  saving: boolean;
  error: string;
  onClose: () => void;
  onSave: (d: Disciplina) => void;
}) {
  const [form, setForm] = useState<Disciplina>(disc);

  const selectedRoomCodes = useMemo(
    () => flattenSalasAlternativas(form.salas_alternativas),
    [form.salas_alternativas]
  );

  const roomOptions = useMemo(
    () => computeDisciplinaRoomOptions(form.turmas, turmas, salas, selectedRoomCodes),
    [form.turmas, salas, selectedRoomCodes, turmas]
  );

  const [showOtherRooms, setShowOtherRooms] = useState<boolean>(() => roomOptions.hasSelectedOtherRoom);

  useEffect(() => {
    if (roomOptions.hasSelectedOtherRoom) {
      setShowOtherRooms(true);
    }
  }, [roomOptions.hasSelectedOtherRoom]);

  const toggleArr = (arr: string[], val: string) =>
    arr.includes(val) ? arr.filter((x) => x !== val) : [...arr, val];

  const handleToggleRoom = (roomCode: string) => {
    setForm((prev) => ({
      ...prev,
      salas_alternativas: toggleRoomInSalasAlternativas(prev.salas_alternativas, roomCode),
    }));
  };

  const hasSelectedTheoreticalInLab = useMemo(() => {
    if (form.tipo !== "LABORATORIO") return false;
    return selectedRoomCodes.some((code) => {
      const r = salas.find((item) => item.code === code);
      return r && r.tipo.toUpperCase() === "TEORICA";
    });
  }, [form.tipo, salas, selectedRoomCodes]);

  const professorItems = useMemo<SelectableEntityItem[]>(
    () => professores.map((p) => ({ code: p.code, nome: p.nome, sublabel: p.code })),
    [professores]
  );

  const turmaItems = useMemo<SelectableEntityItem[]>(
    () =>
      turmas.map((t) => ({
        code: t.code,
        nome: t.nome,
        sublabel: t.sala_teorica_id ? `Sala ${t.sala_teorica_id}` : t.turno,
      })),
    [turmas]
  );

  const roomStatusText =
    selectedRoomCodes.length > 0
      ? `${selectedRoomCodes.length} sala(s) específica(s)`
      : form.tipo === "TEORICA"
        ? "Sala padrão da turma"
        : "Qualquer lab/especial";

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
      <div className="max-h-[90vh] w-full max-w-2xl overflow-y-auto rounded-xl bg-white p-5 shadow-2xl dark:bg-slate-900">
        <div className="flex items-center justify-between border-b border-slate-200 pb-3 dark:border-slate-800">
          <h3 className="text-base font-bold">{isNew ? "Nova Disciplina" : `Editar ${form.nome}`}</h3>
          <button type="button" onClick={onClose} aria-label="Fechar modal">
            <X className="h-5 w-5" />
          </button>
        </div>

        <div className="mt-4 space-y-3 text-sm">
          <div className="grid grid-cols-3 gap-3">
            <div>
              <label className="text-xs text-slate-500">ID</label>
              <input
                disabled={!isNew}
                value={form.code}
                onChange={(e) => setForm({ ...form, code: e.target.value })}
                className="mt-1 w-full rounded border border-slate-300 px-2.5 py-1.5 disabled:opacity-60 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
            <div className="col-span-2">
              <label className="text-xs text-slate-500">Nome</label>
              <input
                value={form.nome}
                onChange={(e) => setForm({ ...form, nome: e.target.value })}
                className="mt-1 w-full rounded border border-slate-300 px-2.5 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
          </div>

          <div className="grid grid-cols-4 gap-2">
            <div>
              <label className="text-xs text-slate-500">Tipo</label>
              <select
                value={form.tipo}
                onChange={(e) => setForm({ ...form, tipo: e.target.value })}
                className="mt-1 w-full rounded border border-slate-300 bg-white px-2 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              >
                <option value="TEORICA">TEORICA</option>
                <option value="LABORATORIO">LABORATORIO</option>
                <option value="ESPECIAL">ESPECIAL</option>
              </select>
            </div>
            <div>
              <label className="text-xs text-slate-500">Regime</label>
              <select
                value={form.regime}
                onChange={(e) =>
                  setForm({
                    ...form,
                    regime: e.target.value,
                    semestre: e.target.value === "ANUAL" ? "AMBOS" : "1",
                  })
                }
                className="mt-1 w-full rounded border border-slate-300 bg-white px-2 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              >
                <option value="ANUAL">ANUAL</option>
                <option value="SEMESTRAL">SEMESTRAL</option>
              </select>
            </div>
            <div>
              <label className="text-xs text-slate-500">Semestre</label>
              <select
                disabled={form.regime === "ANUAL"}
                value={form.semestre}
                onChange={(e) => setForm({ ...form, semestre: e.target.value })}
                className="mt-1 w-full rounded border border-slate-300 bg-white px-2 py-1.5 disabled:opacity-50 dark:border-slate-700 dark:bg-slate-800"
              >
                <option value="1">1º Semestre</option>
                <option value="2">2º Semestre</option>
                <option value="AMBOS">Ambos</option>
              </select>
            </div>
            <div>
              <label className="text-xs text-slate-500">Duração (min)</label>
              <input
                type="number"
                min={5}
                step={5}
                value={form.duracao_minutos}
                onChange={(e) => setForm({ ...form, duracao_minutos: Number(e.target.value) })}
                className="mt-1 w-full rounded border border-slate-300 px-2 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
          </div>

          <div className="grid grid-cols-3 gap-3">
            <div>
              <label className="text-xs text-slate-500">Aulas Semanais</label>
              <input
                type="number"
                min={1}
                value={form.aulas_semanais}
                onChange={(e) => setForm({ ...form, aulas_semanais: Number(e.target.value) })}
                className="mt-1 w-full rounded border border-slate-300 px-2.5 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
            <div>
              <label className="text-xs text-slate-500">Máx. Aulas/Dia Turma</label>
              <input
                type="number"
                min={0}
                value={form.max_aulas_por_dia_turma}
                onChange={(e) => setForm({ ...form, max_aulas_por_dia_turma: Number(e.target.value) })}
                className="mt-1 w-full rounded border border-slate-300 px-2.5 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
            <label className="mt-5 flex items-center gap-2 text-xs">
              <input
                type="checkbox"
                checked={form.exigir_aulas_consecutivas}
                onChange={(e) => setForm({ ...form, exigir_aulas_consecutivas: e.target.checked })}
              />
              Exigir bloco consecutivo
            </label>
          </div>

          {/* Professores (recolhido na edição, expandido na criação) */}
          <CollapsibleEntitySelector
            label="Professores"
            singularEmptyLabel="Nenhum professor selecionado."
            items={professorItems}
            selectedCodes={form.professores}
            onToggle={(code) => setForm({ ...form, professores: toggleArr(form.professores, code) })}
            defaultExpanded={isNew}
            searchPlaceholder="Filtrar professores por nome ou ID..."
            sectionTestId="selector-professores"
          />

          {/* Turmas (recolhido na edição, expandido na criação) */}
          <CollapsibleEntitySelector
            label="Turmas"
            singularEmptyLabel="Nenhuma turma selecionada."
            items={turmaItems}
            selectedCodes={form.turmas}
            onToggle={(code) => setForm({ ...form, turmas: toggleArr(form.turmas, code) })}
            defaultExpanded={isNew}
            searchPlaceholder="Filtrar turmas por nome ou ID..."
            sectionTestId="selector-turmas"
          />

          {/* Salas Disponíveis para a(s) turma(s) selecionada(s) */}
          <div
            data-testid="salas-disponiveis-section"
            className="space-y-2.5 rounded-lg border border-slate-200 p-3 dark:border-slate-800"
          >
            <div className="flex flex-wrap items-center justify-between gap-2">
              <div className="flex items-center gap-2">
                <span className="text-xs font-semibold text-slate-700 dark:text-slate-200">
                  Salas Disponíveis
                </span>
                <span
                  className={`rounded-full px-2 py-0.5 text-[11px] font-medium ${
                    selectedRoomCodes.length > 0
                      ? "bg-pink-100 text-pink-700 dark:bg-pink-950 dark:text-pink-300"
                      : "bg-slate-100 text-slate-600 dark:bg-slate-800 dark:text-slate-300"
                  }`}
                >
                  {roomStatusText}
                </span>
              </div>
              {selectedRoomCodes.length > 0 && (
                <button
                  type="button"
                  onClick={() => setForm({ ...form, salas_alternativas: [] })}
                  className="text-[11px] font-medium text-slate-500 underline hover:text-slate-700 dark:text-slate-400"
                >
                  Limpar seleção
                </button>
              )}
            </div>

            <div className="flex items-start gap-2 rounded-md bg-slate-50 px-2.5 py-2 text-xs text-slate-600 dark:bg-slate-800/50 dark:text-slate-300">
              <Info className="mt-0.5 h-3.5 w-3.5 shrink-0 text-blue-600" />
              <span>
                {form.tipo === "TEORICA" ? (
                  selectedRoomCodes.length === 0 ? (
                    <>
                      Sem seleção manual, o otimizador usará a sala teórica da turma:{" "}
                      <strong>{roomOptions.turmaDefaultSummary}</strong>. Se esta disciplina precisar
                      ocorrer em laboratório ou outra sala específica, selecione abaixo.
                    </>
                  ) : (
                    <>
                      Seleção explícita ativa: o otimizador alocará esta disciplina apenas na(s)
                      sala(s) selecionada(s), substituindo a sala padrão da turma.
                    </>
                  )
                ) : selectedRoomCodes.length === 0 ? (
                  <>
                    Nenhuma sala específica marcada — o otimizador poderá escolher qualquer
                    laboratório ou sala especial disponível.
                  </>
                ) : (
                  <>
                    O otimizador restringirá esta disciplina às salas selecionadas abaixo.
                  </>
                )}
              </span>
            </div>

            {hasSelectedTheoreticalInLab && (
              <div className="flex items-start gap-2 rounded-md border border-amber-200 bg-amber-50 px-2.5 py-1.5 text-xs text-amber-800 dark:border-amber-900 dark:bg-amber-950/40 dark:text-amber-200">
                <AlertTriangle className="mt-0.5 h-3.5 w-3.5 shrink-0" />
                <span>
                  Disciplina marcada como LABORATORIO possui sala teórica selecionada. O otimizador
                  respeitará sua escolha, mas verificadores manuais de laboratório podem alertar sobre
                  sala teórica.
                </span>
              </div>
            )}

            {/* Grupo 1: Sala(s) da(s) turma(s) selecionada(s) */}
            <div>
              <div className="text-[11px] font-medium text-slate-500">
                Sala(s) da(s) Turma(s) selecionada(s)
              </div>
              {form.turmas.length === 0 ? (
                <p className="mt-1 text-xs italic text-slate-400">
                  Selecione ao menos uma turma acima para ver as salas vinculadas a ela.
                </p>
              ) : roomOptions.turmaRooms.length === 0 ? (
                <p className="mt-1 text-xs italic text-slate-400">
                  Nenhuma sala teórica vinculada às turmas selecionadas.
                </p>
              ) : (
                <div className="mt-1.5 flex flex-wrap gap-1.5">
                  {roomOptions.turmaRooms.map(({ room, isFixed }) => {
                    const selected = selectedRoomCodes.includes(room.code);
                    return (
                      <button
                        key={room.code}
                        type="button"
                        onClick={() => handleToggleRoom(room.code)}
                        className={`inline-flex items-center gap-1.5 rounded-full border px-2.5 py-1 text-xs transition ${
                          selected
                            ? "border-pink-600 bg-pink-600 text-white"
                            : "border-slate-300 bg-white hover:bg-slate-50 dark:border-slate-700 dark:bg-slate-900 dark:hover:bg-slate-800"
                        }`}
                      >
                        <span>
                          {room.nome} ({room.code})
                        </span>
                        <span
                          className={`rounded px-1 text-[10px] font-semibold ${
                            selected
                              ? "bg-pink-700 text-white"
                              : isFixed
                                ? "bg-emerald-100 text-emerald-700 dark:bg-emerald-950 dark:text-emerald-300"
                                : "bg-blue-100 text-blue-700 dark:bg-blue-950 dark:text-blue-300"
                          }`}
                        >
                          {isFixed ? "Fixa" : "Permitida"}
                        </span>
                        <span className={`text-[10px] ${selected ? "text-pink-100" : "text-slate-400"}`}>
                          {room.capacidade} lug.
                        </span>
                      </button>
                    );
                  })}
                </div>
              )}
            </div>

            {/* Grupo 2: Laboratórios e Salas Especiais */}
            <div>
              <div className="text-[11px] font-medium text-slate-500">
                Laboratórios e Salas Especiais
              </div>
              {roomOptions.labAndSpecialRooms.length === 0 ? (
                <p className="mt-1 text-xs italic text-slate-400">
                  Nenhum laboratório ou sala especial cadastrado.
                </p>
              ) : (
                <div className="mt-1.5 flex flex-wrap gap-1.5">
                  {roomOptions.labAndSpecialRooms.map((r) => {
                    const selected = selectedRoomCodes.includes(r.code);
                    return (
                      <button
                        key={r.code}
                        type="button"
                        onClick={() => handleToggleRoom(r.code)}
                        className={`inline-flex items-center gap-1.5 rounded-full border px-2.5 py-1 text-xs transition ${
                          selected
                            ? "border-pink-600 bg-pink-600 text-white"
                            : "border-slate-300 hover:bg-slate-50 dark:border-slate-700 dark:hover:bg-slate-800"
                        }`}
                      >
                        <span>
                          {r.nome} ({r.code})
                        </span>
                        <span
                          className={`rounded px-1 text-[10px] font-semibold ${
                            selected
                              ? "bg-pink-700 text-white"
                              : "bg-slate-100 text-slate-600 dark:bg-slate-800 dark:text-slate-300"
                          }`}
                        >
                          {r.tipo}
                        </span>
                        <span className={`text-[10px] ${selected ? "text-pink-100" : "text-slate-400"}`}>
                          {r.capacidade} lug.
                        </span>
                      </button>
                    );
                  })}
                </div>
              )}
            </div>

            {/* Grupo 3: Outras salas teóricas (sob demanda) */}
            {roomOptions.otherTheoreticalRooms.length > 0 && (
              <div className="border-t border-slate-100 pt-2 dark:border-slate-800">
                <button
                  type="button"
                  onClick={() => setShowOtherRooms((v) => !v)}
                  className="inline-flex items-center gap-1 text-[11px] font-medium text-blue-600 hover:underline dark:text-blue-400"
                >
                  {showOtherRooms ? (
                    <ChevronUp className="h-3.5 w-3.5" />
                  ) : (
                    <ChevronDown className="h-3.5 w-3.5" />
                  )}
                  {showOtherRooms
                    ? "Esconder outras salas teóricas"
                    : `Mostrar outras salas teóricas (${roomOptions.otherTheoreticalRooms.length})`}
                </button>
                {showOtherRooms && (
                  <div className="mt-1.5 flex flex-wrap gap-1.5">
                    {roomOptions.otherTheoreticalRooms.map((r) => {
                      const selected = selectedRoomCodes.includes(r.code);
                      return (
                        <button
                          key={r.code}
                          type="button"
                          onClick={() => handleToggleRoom(r.code)}
                          className={`inline-flex items-center gap-1.5 rounded-full border px-2.5 py-1 text-xs transition ${
                            selected
                              ? "border-pink-600 bg-pink-600 text-white"
                              : "border-slate-300 hover:bg-slate-50 dark:border-slate-700 dark:hover:bg-slate-800"
                          }`}
                        >
                          <span>
                            {r.nome} ({r.code})
                          </span>
                          <span className={`text-[10px] ${selected ? "text-pink-100" : "text-slate-400"}`}>
                            {r.capacidade} lug.
                          </span>
                        </button>
                      );
                    })}
                  </div>
                )}
              </div>
            )}
          </div>

          {error && (
            <div className="rounded bg-rose-100 p-2 text-xs text-rose-700 dark:bg-rose-950 dark:text-rose-300">
              {error}
            </div>
          )}
        </div>

        <div className="mt-5 flex justify-end gap-2">
          <button
            type="button"
            onClick={onClose}
            className="rounded-md border border-slate-300 px-4 py-2 text-sm dark:border-slate-700"
          >
            Cancelar
          </button>
          <button
            type="button"
            disabled={!form.nome.trim() || saving}
            onClick={() => onSave(form)}
            className="rounded-md bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-700 disabled:opacity-50"
          >
            {saving ? "Salvando..." : "Salvar"}
          </button>
        </div>
      </div>
    </div>
  );
}
