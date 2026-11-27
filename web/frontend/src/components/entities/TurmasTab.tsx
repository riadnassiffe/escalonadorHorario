import { useMemo, useState } from "react";
import { useMutation, useQueryClient } from "@tanstack/react-query";
import { AlertTriangle, CheckCircle2, Copy, Layers, Pencil, Plus, Search, Sparkles, Trash2, X } from "lucide-react";
import { apiFetch } from "../../api/client";
import { useSlotTemplates } from "../../api/hooks";
import { filterAndSortItems, normalizeText } from "../../domain/searchSort";
import {
  ALL_DAYS,
  computeImplicitBreaks,
  DAY_LABELS,
  DAY_SHORT_LABELS,
  DEFAULT_SHIFT_SLOTS,
  formatInterval,
  timeToMin,
  totalClassMinutes,
} from "../../domain/time";
import {
  generateBatchSlots,
  nextSuggestedSlot,
  replicateDaySchedule,
  summarizeTurmaSchedule,
  turmaToWeeklySchedule,
  validateDaySchedule,
  weeklyScheduleToTurmaFields,
  type WeeklyScheduleMap,
} from "../../domain/turmaSchedule";
import type { DayOfWeek, Disciplina, Professor, Room, TimeSlot, Turma, TurmaCopyResponse } from "../../types";
import { SearchSortTableBar } from "../common/SearchSortTableBar";

interface Props {
  projectId: string;
  turmas: Turma[];
  salas: Room[];
  disciplinas: Disciplina[];
  professores?: Professor[];
}

const SHIFT_PRESETS: Array<{ key: "MANHA" | "TARDE" | "INTEGRAL" | "NOITE"; label: string }> = [
  { key: "MANHA", label: "Manhã" },
  { key: "TARDE", label: "Tarde" },
  { key: "INTEGRAL", label: "Integral" },
  { key: "NOITE", label: "Noite" },
];

export function TurmasTab({ projectId, turmas, salas, disciplinas, professores = [] }: Props) {
  const qc = useQueryClient();
  const { data: slotTemplatesData } = useSlotTemplates(projectId);
  const [search, setSearch] = useState("");
  const [sortBy, setSortBy] = useState("code");
  const [asc, setAsc] = useState(true);
  const [editing, setEditing] = useState<Turma | null>(null);
  const [creating, setCreating] = useState(false);
  const [modalError, setModalError] = useState("");
  const [copying, setCopying] = useState<{ source: Turma; nextCode: string } | null>(null);
  const [copyError, setCopyError] = useState("");
  const [feedbackMsg, setFeedbackMsg] = useState("");

  const professoresByCode = useMemo(
    () => Object.fromEntries(professores.map((p) => [p.code, p])) as Record<string, Professor>,
    [professores]
  );

  const slotTemplates = useMemo<Record<string, TimeSlot[]>>(
    () => ({
      ...DEFAULT_SHIFT_SLOTS,
      ...(slotTemplatesData?.slot_templates || {}),
    }),
    [slotTemplatesData]
  );

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
      setModalError("");
    },
    onError: (err) => {
      setModalError(err instanceof Error ? err.message : "Erro ao salvar turma.");
    },
  });

  const copyMutation = useMutation({
    mutationFn: async (payload: {
      sourceCode: string;
      code: string;
      nome: string;
      copy_disciplinas: boolean;
      disciplina_codes?: string[];
      sala_teorica_id: string | null;
    }) =>
      apiFetch<TurmaCopyResponse>(
        `/projects/${projectId}/turmas/${encodeURIComponent(payload.sourceCode)}/copy`,
        {
          method: "POST",
          body: JSON.stringify({
            code: payload.code,
            nome: payload.nome,
            copy_disciplinas: payload.copy_disciplinas,
            ...(payload.disciplina_codes !== undefined
              ? { disciplina_codes: payload.disciplina_codes }
              : {}),
            sala_teorica_id: payload.sala_teorica_id,
          }),
        }
      ),
    onSuccess: (res) => {
      qc.invalidateQueries({ queryKey: ["turmas", projectId] });
      qc.invalidateQueries({ queryKey: ["disciplinas", projectId] });
      qc.invalidateQueries({ queryKey: ["project", projectId] });
      setCopying(null);
      setCopyError("");
      const discText =
        res.copied_disciplinas_count > 0
          ? ` (${res.copied_disciplinas_count} disciplina(s) duplicada(s))`
          : "";
      setFeedbackMsg(`Turma "${res.turma.nome}" (${res.turma.code}) criada com sucesso${discText}.`);
    },
    onError: (err) => {
      setCopyError(err instanceof Error ? err.message : "Erro ao copiar turma.");
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
    setModalError("");
    setEditing({
      id: res.next_id,
      code: res.next_id,
      nome: "",
      turno: "MANHA",
      dias_letivos: [...ALL_DAYS],
      jornada_padrao: { slots: structuredClone(slotTemplates.MANHA || DEFAULT_SHIFT_SLOTS.MANHA) },
      excecoes_por_dia: {},
      sala_teorica_id: null,
      salas_teoricas_permitidas: salas.filter((s) => s.tipo.toUpperCase() === "TEORICA").map((s) => s.code),
    });
    setCreating(true);
  };

  const openCopy = async (source: Turma) => {
    const res = await apiFetch<{ next_id: string }>(`/projects/${projectId}/turmas/next-id`);
    setCopyError("");
    setCopying({ source, nextCode: res.next_id });
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

      {feedbackMsg && (
        <div className="flex items-center justify-between gap-3 rounded-lg border border-emerald-200 bg-emerald-50 px-4 py-2.5 text-sm text-emerald-800 dark:border-emerald-900 dark:bg-emerald-950/40 dark:text-emerald-200">
          <div className="flex items-center gap-2">
            <CheckCircle2 className="h-4 w-4 shrink-0" />
            <span>{feedbackMsg}</span>
          </div>
          <button
            type="button"
            onClick={() => setFeedbackMsg("")}
            className="rounded p-1 hover:bg-emerald-100 dark:hover:bg-emerald-900"
          >
            <X className="h-4 w-4" />
          </button>
        </div>
      )}

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
              const summary = summarizeTurmaSchedule(t);
              return (
                <tr key={t.code} className="hover:bg-slate-50 dark:hover:bg-slate-800/40">
                  <td className="px-4 py-2.5 font-mono text-xs font-semibold">{t.code}</td>
                  <td className="px-4 py-2.5 font-medium">{t.nome}</td>
                  <td className="px-4 py-2.5">{t.turno}</td>
                  <td className="px-4 py-2.5">{t.sala_teorica_id || "Automática"}</td>
                  <td className="px-4 py-2.5 text-xs text-slate-500">
                    <div className="flex flex-wrap items-center gap-2">
                      <span>
                        {summary.totalSlots} slots/sem • {summary.activeDays} dia(s) • {summary.totalMinutes} min
                      </span>
                      {summary.hasCustomDays && (
                        <span className="rounded-full bg-indigo-100 px-2 py-0.5 text-[10px] font-semibold text-indigo-700 dark:bg-indigo-950 dark:text-indigo-300">
                          Horários por dia
                        </span>
                      )}
                    </div>
                  </td>
                  <td className="px-4 py-2.5 text-right whitespace-nowrap">
                    <button
                      type="button"
                      aria-label={`Copiar ${t.nome}`}
                      title="Copiar turma"
                      onClick={() => openCopy(t)}
                      className="mr-1.5 rounded p-1 text-emerald-600 hover:bg-emerald-50 dark:hover:bg-emerald-950"
                    >
                      <Copy className="h-4 w-4" />
                    </button>
                    <button
                      type="button"
                      title="Editar turma"
                      onClick={() => {
                        setModalError("");
                        setEditing(structuredClone(t));
                        setCreating(false);
                      }}
                      className="mr-1.5 rounded p-1 text-blue-600 hover:bg-blue-50 dark:hover:bg-blue-950"
                    >
                      <Pencil className="h-4 w-4" />
                    </button>
                    <button
                      type="button"
                      title="Remover turma"
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
          slotTemplates={slotTemplates}
          isNew={creating}
          saving={saveMutation.isPending}
          error={modalError}
          onClose={() => {
            setEditing(null);
            setCreating(false);
            setModalError("");
          }}
          onSave={(t) => saveMutation.mutate({ ...t, isNew: creating })}
        />
      )}

      {copying && (
        <CopyTurmaModal
          source={copying.source}
          nextCode={copying.nextCode}
          salas={salas}
          linkedDisciplinas={disciplinas.filter((d) => d.turmas.includes(copying.source.code))}
          professoresByCode={professoresByCode}
          saving={copyMutation.isPending}
          error={copyError}
          onClose={() => {
            setCopying(null);
            setCopyError("");
          }}
          onSubmit={(payload) =>
            copyMutation.mutate({
              sourceCode: copying.source.code,
              ...payload,
            })
          }
        />
      )}
    </div>
  );
}

function TurmaModal({
  turma,
  salas,
  slotTemplates,
  isNew,
  saving,
  error,
  onClose,
  onSave,
}: {
  turma: Turma;
  salas: Room[];
  slotTemplates: Record<string, TimeSlot[]>;
  isNew: boolean;
  saving: boolean;
  error: string;
  onClose: () => void;
  onSave: (t: Turma) => void;
}) {
  const [form, setForm] = useState<Turma>(turma);
  const [weekly, setWeekly] = useState<WeeklyScheduleMap>(() => turmaToWeeklySchedule(turma));
  const [activeDay, setActiveDay] = useState<DayOfWeek>(() => {
    const firstActive = ALL_DAYS.find((d) => turma.dias_letivos.includes(d));
    return firstActive || "SEG";
  });

  const [showReplicate, setShowReplicate] = useState(false);
  const [replicateTargets, setReplicateTargets] = useState<DayOfWeek[]>(() =>
    ALL_DAYS.filter((d) => d !== (ALL_DAYS.find((x) => turma.dias_letivos.includes(x)) || "SEG"))
  );

  const [showBatch, setShowBatch] = useState(false);
  const [batchStart, setBatchStart] = useState("08:00");
  const [batchCount, setBatchCount] = useState(5);
  const [batchDuration, setBatchDuration] = useState(50);
  const [batchHasBreak, setBatchHasBreak] = useState(true);
  const [batchBreakAfter, setBatchBreakAfter] = useState(3);
  const [batchBreakDuration, setBatchBreakDuration] = useState(15);

  const teoricas = salas.filter((s) => s.tipo.toUpperCase() === "TEORICA");
  const currentDay = weekly[activeDay];
  const currentValidation = validateDaySchedule(currentDay);
  const currentBreaks = computeImplicitBreaks(currentDay.slots);

  const validationsByDay = useMemo(() => {
    return Object.fromEntries(
      ALL_DAYS.map((d) => [d, validateDaySchedule(weekly[d])])
    ) as Record<DayOfWeek, ReturnType<typeof validateDaySchedule>>;
  }, [weekly]);

  const activeDaysCount = ALL_DAYS.filter((d) => weekly[d].active).length;
  const invalidActiveDays = ALL_DAYS.filter((d) => weekly[d].active && !validationsByDay[d].valid);
  const totalWeeklySlots = ALL_DAYS.reduce(
    (acc, d) => acc + (weekly[d].active ? weekly[d].slots.length : 0),
    0
  );
  const totalWeeklyMinutes = ALL_DAYS.reduce(
    (acc, d) => acc + (weekly[d].active ? totalClassMinutes(weekly[d].slots) : 0),
    0
  );
  const canSave =
    Boolean(form.nome.trim()) && activeDaysCount > 0 && invalidActiveDays.length === 0 && !saving;

  const getTemplateSlots = (shift: string): TimeSlot[] => {
    const tpl = slotTemplates[shift] || DEFAULT_SHIFT_SLOTS[shift] || DEFAULT_SHIFT_SLOTS.MANHA;
    return structuredClone(tpl);
  };

  const applyPresetToAllDays = (shift: "MANHA" | "TARDE" | "INTEGRAL" | "NOITE") => {
    const slots = getTemplateSlots(shift);
    const next = {} as WeeklyScheduleMap;
    for (const d of ALL_DAYS) {
      next[d] = { active: true, slots: structuredClone(slots) };
    }
    setWeekly(next);
    setForm((prev) => ({ ...prev, turno: shift }));
  };

  const applyPresetToDay = (day: DayOfWeek, shift: "MANHA" | "TARDE" | "INTEGRAL" | "NOITE") => {
    const slots = getTemplateSlots(shift);
    setWeekly((prev) => ({
      ...prev,
      [day]: { active: true, slots },
    }));
  };

  const updateDaySlot = (day: DayOfWeek, idx: number, field: keyof TimeSlot, val: string) => {
    setWeekly((prev) => {
      const nextSlots = [...prev[day].slots];
      nextSlots[idx] = { ...nextSlots[idx], [field]: val };
      return { ...prev, [day]: { ...prev[day], slots: nextSlots } };
    });
  };

  const addDaySlot = (day: DayOfWeek) => {
    setWeekly((prev) => {
      const suggested = nextSuggestedSlot(prev[day].slots);
      return {
        ...prev,
        [day]: { active: true, slots: [...prev[day].slots, suggested] },
      };
    });
  };

  const removeDaySlot = (day: DayOfWeek, idx: number) => {
    setWeekly((prev) => ({
      ...prev,
      [day]: {
        ...prev[day],
        slots: prev[day].slots.filter((_, i) => i !== idx),
      },
    }));
  };

  const toggleDayActive = (day: DayOfWeek) => {
    setWeekly((prev) => {
      const nextActive = !prev[day].active;
      const nextSlots =
        nextActive && prev[day].slots.length === 0 ? getTemplateSlots(form.turno) : prev[day].slots;
      return {
        ...prev,
        [day]: { active: nextActive, slots: nextSlots },
      };
    });
  };

  const openReplicatePanel = () => {
    setReplicateTargets(ALL_DAYS.filter((d) => d !== activeDay));
    setShowReplicate((v) => !v);
  };

  const toggleReplicateTarget = (day: DayOfWeek) => {
    setReplicateTargets((prev) =>
      prev.includes(day) ? prev.filter((x) => x !== day) : [...prev, day]
    );
  };

  const handleApplyReplicate = () => {
    if (replicateTargets.length === 0) return;
    setWeekly((prev) => replicateDaySchedule(prev, activeDay, replicateTargets));
    setShowReplicate(false);
  };

  const handleGenerateBatch = () => {
    const generated = generateBatchSlots({
      startTime: batchStart,
      count: batchCount,
      durationMin: batchDuration,
      hasBreak: batchHasBreak,
      breakAfterSlot: batchBreakAfter,
      breakDurationMin: batchBreakDuration,
    });
    if (generated.length === 0) return;
    setWeekly((prev) => ({
      ...prev,
      [activeDay]: { active: true, slots: generated },
    }));
    setShowBatch(false);
  };

  const handleSave = () => {
    const scheduleFields = weeklyScheduleToTurmaFields(weekly, form.turno);
    onSave({
      ...form,
      ...scheduleFields,
    });
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
      <div className="max-h-[92vh] w-full max-w-3xl overflow-y-auto rounded-xl bg-white p-5 shadow-2xl dark:bg-slate-900">
        <div className="flex items-center justify-between border-b border-slate-200 pb-3 dark:border-slate-800">
          <h3 className="text-base font-bold">{isNew ? "Nova Turma" : `Editar ${form.nome}`}</h3>
          <button type="button" onClick={onClose}>
            <X className="h-5 w-5" />
          </button>
        </div>

        <div className="mt-4 space-y-4 text-sm">
          {/* Identificação básica */}
          <div className="grid grid-cols-1 gap-3 sm:grid-cols-4">
            <div>
              <label className="text-xs font-medium text-slate-500">ID</label>
              <input
                disabled={!isNew}
                value={form.code}
                onChange={(e) => setForm({ ...form, code: e.target.value })}
                className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5 disabled:opacity-60 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
            <div className="sm:col-span-2">
              <label className="text-xs font-medium text-slate-500">Nome</label>
              <input
                value={form.nome}
                onChange={(e) => setForm({ ...form, nome: e.target.value })}
                placeholder="Ex: 1º Ano A - Ensino Médio"
                className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
            <div>
              <label className="text-xs font-medium text-slate-500">Turno Geral</label>
              <select
                value={form.turno}
                onChange={(e) => setForm({ ...form, turno: e.target.value })}
                className="mt-1 w-full rounded border border-slate-300 bg-white px-2 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              >
                <option value="MANHA">MANHA</option>
                <option value="TARDE">TARDE</option>
                <option value="NOITE">NOITE</option>
                <option value="INTEGRAL">INTEGRAL</option>
              </select>
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
                <option key={s.code} value={s.code}>
                  {s.nome} ({s.code})
                </option>
              ))}
            </select>
          </div>

          {/* Barra de modelos semanais */}
          <div className="rounded-lg border border-slate-200 bg-slate-50 p-2.5 dark:border-slate-800 dark:bg-slate-800/40">
            <div className="flex flex-wrap items-center justify-between gap-2">
              <span className="inline-flex items-center gap-1.5 text-xs font-semibold text-slate-700 dark:text-slate-200">
                <Sparkles className="h-3.5 w-3.5 text-blue-600" />
                Aplicar modelo em toda a semana:
              </span>
              <div className="flex flex-wrap gap-1.5">
                {SHIFT_PRESETS.map((preset) => (
                  <button
                    key={preset.key}
                    type="button"
                    onClick={() => applyPresetToAllDays(preset.key)}
                    className="rounded border border-slate-300 bg-white px-2.5 py-1 text-xs font-medium hover:bg-slate-100 dark:border-slate-700 dark:bg-slate-900 dark:hover:bg-slate-800"
                  >
                    {preset.label}
                  </button>
                ))}
              </div>
            </div>
          </div>

          {/* Abas por dia da semana */}
          <div className="rounded-lg border border-slate-200 dark:border-slate-800">
            <div className="grid grid-cols-5 border-b border-slate-200 bg-slate-50 dark:border-slate-800 dark:bg-slate-800/40">
              {ALL_DAYS.map((d) => {
                const dayState = weekly[d];
                const val = validationsByDay[d];
                const isCurrent = activeDay === d;
                return (
                  <button
                    key={d}
                    type="button"
                    onClick={() => {
                      setActiveDay(d);
                      setShowReplicate(false);
                    }}
                    className={`flex items-center justify-center gap-1.5 border-b-2 px-2 py-2.5 text-xs font-semibold transition ${
                      isCurrent
                        ? "border-blue-600 bg-white text-blue-600 dark:bg-slate-900 dark:text-blue-400"
                        : "border-transparent text-slate-600 hover:bg-white/60 dark:text-slate-400 dark:hover:bg-slate-900/60"
                    }`}
                  >
                    <span>{DAY_SHORT_LABELS[d]}</span>
                    {!dayState.active ? (
                      <span className="rounded bg-slate-200 px-1.5 py-0.5 text-[10px] font-normal text-slate-600 dark:bg-slate-700 dark:text-slate-300">
                        Folga
                      </span>
                    ) : val.valid ? (
                      <span className="rounded bg-emerald-100 px-1.5 py-0.5 text-[10px] font-semibold text-emerald-700 dark:bg-emerald-950 dark:text-emerald-300">
                        {dayState.slots.length}
                      </span>
                    ) : (
                      <AlertTriangle className="h-3.5 w-3.5 text-rose-600" />
                    )}
                  </button>
                );
              })}
            </div>

            <div className="space-y-3 p-3.5">
              {/* Cabeçalho do dia */}
              <div className="flex flex-wrap items-center justify-between gap-2">
                <label className="inline-flex cursor-pointer items-center gap-2 text-xs font-semibold">
                  <input
                    type="checkbox"
                    checked={currentDay.active}
                    onChange={() => toggleDayActive(activeDay)}
                    className="h-4 w-4 rounded"
                  />
                  Turma possui aulas na {DAY_LABELS[activeDay]}
                </label>

                <button
                  type="button"
                  onClick={openReplicatePanel}
                  className="inline-flex items-center gap-1.5 rounded-md border border-slate-300 bg-white px-2.5 py-1 text-xs font-medium text-slate-700 hover:bg-slate-50 dark:border-slate-700 dark:bg-slate-800 dark:text-slate-200 dark:hover:bg-slate-700"
                >
                  <Copy className="h-3.5 w-3.5 text-blue-600" />
                  Replicar para outros dias...
                </button>
              </div>

              {/* Painel de replicação entre dias */}
              {showReplicate && (
                <div className="rounded-lg border border-blue-200 bg-blue-50/70 p-3 dark:border-blue-900 dark:bg-blue-950/30">
                  <div className="flex flex-wrap items-center justify-between gap-2">
                    <span className="text-xs font-semibold text-blue-900 dark:text-blue-200">
                      Copiar horário de {DAY_LABELS[activeDay]} ({currentDay.active ? `${currentDay.slots.length} slot(s)` : "Folga"}) para:
                    </span>
                    <div className="flex gap-2 text-[11px]">
                      <button
                        type="button"
                        onClick={() => setReplicateTargets(ALL_DAYS.filter((d) => d !== activeDay))}
                        className="text-blue-700 hover:underline dark:text-blue-300"
                      >
                        Todos
                      </button>
                      <button
                        type="button"
                        onClick={() => setReplicateTargets([])}
                        className="text-slate-600 hover:underline dark:text-slate-400"
                      >
                        Limpar
                      </button>
                    </div>
                  </div>
                  <div className="mt-2 flex flex-wrap gap-2">
                    {ALL_DAYS.filter((d) => d !== activeDay).map((d) => (
                      <label
                        key={d}
                        className="inline-flex cursor-pointer items-center gap-1.5 rounded border border-blue-200 bg-white px-2.5 py-1 text-xs dark:border-blue-800 dark:bg-slate-900"
                      >
                        <input
                          type="checkbox"
                          checked={replicateTargets.includes(d)}
                          onChange={() => toggleReplicateTarget(d)}
                        />
                        {DAY_LABELS[d]}
                      </label>
                    ))}
                  </div>
                  <div className="mt-2.5 flex justify-end gap-2">
                    <button
                      type="button"
                      onClick={() => setShowReplicate(false)}
                      className="rounded border border-slate-300 bg-white px-2.5 py-1 text-xs dark:border-slate-700 dark:bg-slate-900"
                    >
                      Cancelar
                    </button>
                    <button
                      type="button"
                      disabled={replicateTargets.length === 0}
                      onClick={handleApplyReplicate}
                      className="rounded bg-blue-600 px-3 py-1 text-xs font-semibold text-white hover:bg-blue-700 disabled:opacity-50"
                    >
                      Aplicar replicação
                    </button>
                  </div>
                </div>
              )}

              {!currentDay.active ? (
                <div className="rounded-lg border border-dashed border-slate-300 p-4 text-center text-xs text-slate-500 dark:border-slate-700">
                  Turma não possui aulas neste dia (dia não letivo).
                </div>
              ) : (
                <>
                  {/* Barra de ações do dia */}
                  <div className="flex flex-wrap items-center justify-between gap-2 border-t border-slate-100 pt-2.5 dark:border-slate-800">
                    <div className="flex flex-wrap items-center gap-2">
                      <button
                        type="button"
                        onClick={() => addDaySlot(activeDay)}
                        className="inline-flex items-center gap-1 rounded border border-blue-200 bg-blue-50 px-2.5 py-1 text-xs font-semibold text-blue-700 hover:bg-blue-100 dark:border-blue-900 dark:bg-blue-950/50 dark:text-blue-300"
                      >
                        <Plus className="h-3.5 w-3.5" /> Adicionar Slot
                      </button>
                      <button
                        type="button"
                        onClick={() => setShowBatch((v) => !v)}
                        className="inline-flex items-center gap-1 rounded border border-slate-300 px-2.5 py-1 text-xs font-medium hover:bg-slate-50 dark:border-slate-700 dark:hover:bg-slate-800"
                      >
                        <Layers className="h-3.5 w-3.5" /> Gerar em Lote
                      </button>
                    </div>
                    <div className="flex flex-wrap items-center gap-1">
                      <span className="mr-1 text-[11px] text-slate-400">Modelo no dia:</span>
                      {SHIFT_PRESETS.map((preset) => (
                        <button
                          key={preset.key}
                          type="button"
                          onClick={() => applyPresetToDay(activeDay, preset.key)}
                          className="rounded border border-slate-200 px-2 py-0.5 text-[11px] hover:bg-slate-100 dark:border-slate-700 dark:hover:bg-slate-800"
                        >
                          {preset.label}
                        </button>
                      ))}
                    </div>
                  </div>

                  {/* Painel inline Gerar em Lote */}
                  {showBatch && (
                    <div className="rounded-lg border border-slate-200 bg-slate-50 p-3 dark:border-slate-700 dark:bg-slate-800/50">
                      <div className="mb-2 text-xs font-semibold">Gerar Slots em Lote para {DAY_LABELS[activeDay]}</div>
                      <div className="grid grid-cols-2 gap-2 text-xs sm:grid-cols-3">
                        <div>
                          <label className="text-[11px] text-slate-500">Horário Inicial</label>
                          <input
                            type="time"
                            value={batchStart}
                            onChange={(e) => setBatchStart(e.target.value)}
                            className="mt-1 w-full rounded border border-slate-300 bg-white px-2 py-1 dark:border-slate-700 dark:bg-slate-900"
                          />
                        </div>
                        <div>
                          <label className="text-[11px] text-slate-500">Qtd. de Slots</label>
                          <input
                            type="number"
                            min={1}
                            max={20}
                            value={batchCount}
                            onChange={(e) => setBatchCount(Number(e.target.value))}
                            className="mt-1 w-full rounded border border-slate-300 bg-white px-2 py-1 dark:border-slate-700 dark:bg-slate-900"
                          />
                        </div>
                        <div>
                          <label className="text-[11px] text-slate-500">Duração (min)</label>
                          <input
                            type="number"
                            min={10}
                            max={240}
                            value={batchDuration}
                            onChange={(e) => setBatchDuration(Number(e.target.value))}
                            className="mt-1 w-full rounded border border-slate-300 bg-white px-2 py-1 dark:border-slate-700 dark:bg-slate-900"
                          />
                        </div>
                      </div>
                      <div className="mt-2 flex flex-wrap items-center gap-3 text-xs">
                        <label className="inline-flex items-center gap-1.5">
                          <input
                            type="checkbox"
                            checked={batchHasBreak}
                            onChange={(e) => setBatchHasBreak(e.target.checked)}
                          />
                          Intervalo após o slot:
                        </label>
                        <input
                          type="number"
                          min={1}
                          max={19}
                          disabled={!batchHasBreak}
                          value={batchBreakAfter}
                          onChange={(e) => setBatchBreakAfter(Number(e.target.value))}
                          className="w-14 rounded border border-slate-300 bg-white px-2 py-0.5 disabled:opacity-50 dark:border-slate-700 dark:bg-slate-900"
                        />
                        <span className="text-slate-500">Duração (min):</span>
                        <input
                          type="number"
                          min={5}
                          max={180}
                          disabled={!batchHasBreak}
                          value={batchBreakDuration}
                          onChange={(e) => setBatchBreakDuration(Number(e.target.value))}
                          className="w-16 rounded border border-slate-300 bg-white px-2 py-0.5 disabled:opacity-50 dark:border-slate-700 dark:bg-slate-900"
                        />
                        <div className="ml-auto flex gap-2">
                          <button
                            type="button"
                            onClick={() => setShowBatch(false)}
                            className="rounded border border-slate-300 bg-white px-2.5 py-1 dark:border-slate-700 dark:bg-slate-900"
                          >
                            Fechar
                          </button>
                          <button
                            type="button"
                            onClick={handleGenerateBatch}
                            className="rounded bg-blue-600 px-3 py-1 font-semibold text-white hover:bg-blue-700"
                          >
                            Gerar Slots
                          </button>
                        </div>
                      </div>
                    </div>
                  )}

                  {/* Lista de slots do dia */}
                  <div className="grid grid-cols-1 gap-2 sm:grid-cols-2">
                    {currentDay.slots.map((s, idx) => {
                      const dur = s.inicio && s.fim ? timeToMin(s.fim) - timeToMin(s.inicio) : 0;
                      const slotInvalid = !s.inicio || !s.fim || dur <= 0;
                      return (
                        <div
                          key={idx}
                          className={`flex items-center justify-between rounded border px-2.5 py-1.5 ${
                            slotInvalid
                              ? "border-rose-300 bg-rose-50/50 dark:border-rose-800 dark:bg-rose-950/30"
                              : "border-slate-200 dark:border-slate-700"
                          }`}
                        >
                          <span className="text-xs font-mono text-slate-400">#{idx + 1}</span>
                          <div className="flex items-center gap-1">
                            <input
                              type="time"
                              value={s.inicio}
                              onChange={(e) => updateDaySlot(activeDay, idx, "inicio", e.target.value)}
                              className="bg-transparent text-xs"
                            />
                            <span>-</span>
                            <input
                              type="time"
                              value={s.fim}
                              onChange={(e) => updateDaySlot(activeDay, idx, "fim", e.target.value)}
                              className="bg-transparent text-xs"
                            />
                          </div>
                          <span
                            className={`text-[11px] font-medium ${
                              slotInvalid ? "text-rose-600" : "text-slate-500"
                            }`}
                          >
                            {dur > 0 ? `${dur}m` : "Inválido"}
                          </span>
                          <button
                            type="button"
                            onClick={() => removeDaySlot(activeDay, idx)}
                            className="text-rose-500 hover:text-rose-700"
                          >
                            <X className="h-3.5 w-3.5" />
                          </button>
                        </div>
                      );
                    })}
                  </div>

                  {/* Status e intervalos implícitos do dia */}
                  {!currentValidation.valid ? (
                    <div className="rounded bg-rose-50 px-3 py-2 text-xs font-medium text-rose-700 dark:bg-rose-950/40 dark:text-rose-300">
                      {currentValidation.error}
                    </div>
                  ) : (
                    <div className="flex flex-wrap items-center justify-between gap-2 text-xs text-slate-600 dark:text-slate-400">
                      <span>
                        {currentDay.slots.length} slot(s) • {totalClassMinutes(currentDay.slots)} min letivos no dia
                      </span>
                      {currentBreaks.length > 0 && (
                        <span className="text-amber-700 dark:text-amber-300">
                          Intervalos: {currentBreaks.map((b) => formatInterval(b.start_min, b.end_min)).join(" • ")}
                        </span>
                      )}
                    </div>
                  )}
                </>
              )}
            </div>
          </div>

          {/* Resumo Semanal */}
          <div className="rounded-lg border border-slate-200 bg-slate-50 px-3 py-2 text-xs text-slate-600 dark:border-slate-800 dark:bg-slate-800/40 dark:text-slate-300">
            <div className="flex flex-wrap items-center justify-between gap-2">
              <div className="flex flex-wrap gap-x-3 gap-y-1">
                {ALL_DAYS.map((d) => (
                  <span key={d}>
                    <strong>{DAY_SHORT_LABELS[d]}</strong>:{" "}
                    {!weekly[d].active
                      ? "Folga"
                      : validationsByDay[d].valid
                        ? `${weekly[d].slots.length} (${totalClassMinutes(weekly[d].slots)}m)`
                        : "Inválido"}
                  </span>
                ))}
              </div>
              <span className="font-semibold">
                Total: {totalWeeklySlots} slots • {totalWeeklyMinutes} min ({(totalWeeklyMinutes / 60).toFixed(1)} h)
              </span>
            </div>
          </div>

          {activeDaysCount === 0 && (
            <div className="rounded bg-rose-50 px-3 py-2 text-xs font-medium text-rose-700 dark:bg-rose-950/40 dark:text-rose-300">
              A turma deve funcionar em ao menos um dia da semana.
            </div>
          )}

          {invalidActiveDays.length > 0 && (
            <div className="rounded bg-rose-50 px-3 py-2 text-xs font-medium text-rose-700 dark:bg-rose-950/40 dark:text-rose-300">
              Corrija os horários inválidos em:{" "}
              {invalidActiveDays.map((d) => DAY_LABELS[d]).join(", ")}.
            </div>
          )}

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
            disabled={!canSave}
            onClick={handleSave}
            className="rounded-md bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-700 disabled:opacity-50"
          >
            {saving ? "Salvando..." : "Salvar"}
          </button>
        </div>
      </div>
    </div>
  );
}

function CopyTurmaModal({
  source,
  nextCode,
  salas,
  linkedDisciplinas,
  professoresByCode,
  saving,
  error,
  onClose,
  onSubmit,
}: {
  source: Turma;
  nextCode: string;
  salas: Room[];
  linkedDisciplinas: Disciplina[];
  professoresByCode: Record<string, Professor>;
  saving: boolean;
  error: string;
  onClose: () => void;
  onSubmit: (payload: {
    code: string;
    nome: string;
    copy_disciplinas: boolean;
    disciplina_codes?: string[];
    sala_teorica_id: string | null;
  }) => void;
}) {
  const [code, setCode] = useState(nextCode);
  const [nome, setNome] = useState(`${source.nome} (Cópia)`);
  const [salaTeoricaId, setSalaTeoricaId] = useState<string>("");
  const [copyDisciplinas, setCopyDisciplinas] = useState(false);
  const [selectedDiscCodes, setSelectedDiscCodes] = useState<string[]>(() =>
    linkedDisciplinas.map((d) => d.code)
  );
  const [discSearch, setDiscSearch] = useState("");

  const teoricas = salas.filter((s) => s.tipo.toUpperCase() === "TEORICA");
  const summary = summarizeTurmaSchedule(source);

  const visibleDisciplinas = useMemo(() => {
    const q = normalizeText(discSearch.trim());
    if (!q) return linkedDisciplinas;
    return linkedDisciplinas.filter((d) => {
      const profNames = d.professores.map((c) => professoresByCode[c]?.nome || c).join(" ");
      return normalizeText(`${d.code} ${d.nome} ${d.professores.join(" ")} ${profNames}`).includes(q);
    });
  }, [discSearch, linkedDisciplinas, professoresByCode]);

  const toggleDisciplina = (discCode: string) => {
    setSelectedDiscCodes((prev) =>
      prev.includes(discCode) ? prev.filter((c) => c !== discCode) : [...prev, discCode]
    );
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
      <div className="max-h-[90vh] w-full max-w-xl overflow-y-auto rounded-xl bg-white p-5 shadow-2xl dark:bg-slate-900">
        <div className="flex items-center justify-between border-b border-slate-200 pb-3 dark:border-slate-800">
          <h3 className="text-base font-bold">Copiar Turma</h3>
          <button type="button" onClick={onClose}>
            <X className="h-5 w-5" />
          </button>
        </div>

        <div className="mt-4 space-y-3.5 text-sm">
          <div className="rounded-lg border border-slate-200 bg-slate-50 p-3 text-xs dark:border-slate-800 dark:bg-slate-800/40">
            <div className="font-semibold text-slate-700 dark:text-slate-200">
              Origem: <span className="font-mono">{source.code}</span> — {source.nome}
            </div>
            <div className="mt-1 text-slate-500">
              Turno: {source.turno} • {summary.activeDays} dia(s) letivo(s) • {summary.totalSlots} slots/sem ({summary.totalMinutes} min)
            </div>
          </div>

          <div className="grid grid-cols-3 gap-3">
            <div>
              <label className="text-xs font-medium text-slate-500">Novo ID</label>
              <input
                value={code}
                onChange={(e) => setCode(e.target.value)}
                className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5 font-mono text-xs dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
            <div className="col-span-2">
              <label className="text-xs font-medium text-slate-500">Novo Nome da Turma</label>
              <input
                value={nome}
                onChange={(e) => setNome(e.target.value)}
                placeholder="Ex: 1º Ano B - Ensino Médio"
                className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
          </div>

          <div>
            <label className="text-xs font-medium text-slate-500">Sala Teórica Fixa</label>
            <select
              value={salaTeoricaId}
              onChange={(e) => setSalaTeoricaId(e.target.value)}
              className="mt-1 w-full rounded border border-slate-300 bg-white px-3 py-1.5 dark:border-slate-700 dark:bg-slate-800"
            >
              <option value="">Escolha automática pelo solver</option>
              {teoricas.map((s) => (
                <option key={s.code} value={s.code}>
                  {s.nome} ({s.code})
                </option>
              ))}
            </select>
          </div>

          <label
            className={`flex items-start gap-2.5 rounded-lg border border-slate-200 p-3 text-xs dark:border-slate-800 ${
              linkedDisciplinas.length === 0 ? "opacity-60" : "cursor-pointer hover:bg-slate-50 dark:hover:bg-slate-800/40"
            }`}
          >
            <input
              type="checkbox"
              disabled={linkedDisciplinas.length === 0}
              checked={copyDisciplinas}
              onChange={(e) => setCopyDisciplinas(e.target.checked)}
              className="mt-0.5 h-4 w-4 rounded"
            />
            <div>
              <span className="font-semibold">
                Copiar também as disciplinas vinculadas a esta turma ({linkedDisciplinas.length} disciplina(s))
              </span>
              <p className="mt-0.5 text-slate-500">
                Quando marcado, permite escolher quais disciplinas vinculadas a {source.code} serão duplicadas para a nova turma.
              </p>
            </div>
          </label>

          {copyDisciplinas && linkedDisciplinas.length > 0 && (
            <div
              data-testid="copy-disciplinas-selector"
              className="space-y-2.5 rounded-lg border border-slate-200 bg-slate-50/70 p-3 text-xs dark:border-slate-800 dark:bg-slate-800/30"
            >
              <div className="flex flex-wrap items-center justify-between gap-2">
                <span className="font-semibold text-slate-700 dark:text-slate-200">
                  Selecionadas: {selectedDiscCodes.length} de {linkedDisciplinas.length} disciplina(s)
                </span>
                <div className="flex gap-2">
                  <button
                    type="button"
                    onClick={() => setSelectedDiscCodes(linkedDisciplinas.map((d) => d.code))}
                    className="font-medium text-blue-600 hover:underline dark:text-blue-400"
                  >
                    Selecionar todas
                  </button>
                  <button
                    type="button"
                    onClick={() => setSelectedDiscCodes([])}
                    className="font-medium text-slate-600 hover:underline dark:text-slate-400"
                  >
                    Limpar seleção
                  </button>
                </div>
              </div>

              <div className="relative">
                <Search className="pointer-events-none absolute left-2.5 top-2 h-3.5 w-3.5 text-slate-400" />
                <input
                  type="text"
                  value={discSearch}
                  onChange={(e) => setDiscSearch(e.target.value)}
                  placeholder="Filtrar disciplinas por nome ou ID..."
                  className="w-full rounded border border-slate-300 bg-white py-1 pl-8 pr-2.5 text-xs dark:border-slate-700 dark:bg-slate-800"
                />
              </div>

              <div className="max-h-56 space-y-1.5 overflow-y-auto pr-1">
                {visibleDisciplinas.length === 0 ? (
                  <p className="py-2 text-center text-slate-400">Nenhuma disciplina encontrada.</p>
                ) : (
                  visibleDisciplinas.map((d) => {
                    const checked = selectedDiscCodes.includes(d.code);
                    const profText = d.professores
                      .map((c) => professoresByCode[c]?.nome || c)
                      .join(", ");
                    return (
                      <label
                        key={d.code}
                        className={`flex cursor-pointer items-start gap-2.5 rounded border px-2.5 py-2 transition ${
                          checked
                            ? "border-blue-300 bg-blue-50/70 dark:border-blue-800 dark:bg-blue-950/30"
                            : "border-slate-200 bg-white hover:bg-slate-50 dark:border-slate-700 dark:bg-slate-900 dark:hover:bg-slate-800"
                        }`}
                      >
                        <input
                          type="checkbox"
                          checked={checked}
                          onChange={() => toggleDisciplina(d.code)}
                          className="mt-0.5 h-3.5 w-3.5 rounded"
                        />
                        <div className="min-w-0 flex-1">
                          <div className="flex flex-wrap items-center gap-1.5">
                            <span className="font-semibold text-slate-800 dark:text-slate-100">{d.nome}</span>
                            <span className="rounded bg-slate-200/80 px-1.5 py-0.2 font-mono text-[10px] text-slate-700 dark:bg-slate-700 dark:text-slate-200">
                              {d.code}
                            </span>
                          </div>
                          <div className="mt-0.5 text-[11px] text-slate-500">
                            {d.aulas_semanais} aulas/sem • {d.duracao_minutos}m •{" "}
                            {d.regime === "ANUAL" ? "Anual" : `Semestral (${d.semestre}º)`}
                            {profText ? ` • Prof.: ${profText}` : ""}
                          </div>
                        </div>
                      </label>
                    );
                  })
                )}
              </div>

              {selectedDiscCodes.length === 0 && (
                <div className="rounded bg-amber-50 px-2.5 py-1.5 text-[11px] font-medium text-amber-800 dark:bg-amber-950/40 dark:text-amber-200">
                  Nenhuma disciplina selecionada — apenas a turma será copiada.
                </div>
              )}
            </div>
          )}

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
            disabled={!nome.trim() || saving}
            onClick={() =>
              onSubmit({
                code: code.trim(),
                nome: nome.trim(),
                copy_disciplinas: copyDisciplinas && selectedDiscCodes.length > 0,
                disciplina_codes: copyDisciplinas ? selectedDiscCodes : undefined,
                sala_teorica_id: salaTeoricaId || null,
              })
            }
            className="inline-flex items-center gap-1.5 rounded-md bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-700 disabled:opacity-50"
          >
            <Copy className="h-4 w-4" />
            {saving ? "Copiando..." : "Copiar Turma"}
          </button>
        </div>
      </div>
    </div>
  );
}
