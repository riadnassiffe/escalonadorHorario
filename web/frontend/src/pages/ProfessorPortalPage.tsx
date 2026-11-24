import { useEffect, useMemo, useState } from "react";
import { useNavigate } from "react-router-dom";
import {
  Calendar,
  CheckCircle2,
  ChevronDown,
  ChevronUp,
  KeyRound,
  Lock,
  LockOpen,
  LogOut,
  Moon,
  Plus,
  Save,
  Sliders,
  Sun,
  X,
} from "lucide-react";
import {
  useProfessorProfiles,
  useProfessorTimetable,
  useUpdateProfessorPreferences,
} from "../api/hooks";
import { ALL_DAYS, DAY_LABELS, DAY_SHORT_LABELS } from "../domain/time";
import { useAuthStore } from "../store/authStore";
import { useUiStore } from "../store/uiStore";
import type { DayOfWeek, ProfessorProfile, TimeSlot } from "../types";

const SHIFTS: Array<{ key: "MANHA" | "TARDE" | "NOITE"; label: string }> = [
  { key: "MANHA", label: "Manhã" },
  { key: "TARDE", label: "Tarde" },
  { key: "NOITE", label: "Noite" },
];

function prefLabel(val: number): string {
  if (val <= 2) return "Evitar";
  if (val <= 4) return "Baixa";
  if (val === 5) return "Neutro";
  if (val <= 8) return "Boa";
  return "Preferido";
}

function buildInitialTurnos(profile: ProfessorProfile): Record<string, string[]> {
  return Object.fromEntries(
    ALL_DAYS.map((d) => [d, [...(profile.turnos_pretendidos?.[d] || [])]])
  );
}

function buildInitialPrefs(profile: ProfessorProfile): Record<string, number> {
  return Object.fromEntries(
    ALL_DAYS.map((d) => [d, profile.preferencia_dias?.[d] ?? 5])
  );
}

function buildInitialDisp(profile: ProfessorProfile): Record<string, TimeSlot[]> {
  return Object.fromEntries(
    ALL_DAYS.map((d) => [d, (profile.disponibilidade?.[d] || []).map((s) => ({ ...s }))])
  );
}

export function ProfessorPortalPage() {
  const navigate = useNavigate();
  const { user, logout } = useAuthStore();
  const { darkMode, toggleDarkMode } = useUiStore();

  const { data: profiles = [], isLoading } = useProfessorProfiles();
  const [selectedProfId, setSelectedProfId] = useState<string>("");
  const [tab, setTab] = useState<"preferencias" | "grade">("preferencias");

  const activeProfile = useMemo(
    () => profiles.find((p) => p.professor_id === selectedProfId) || profiles[0] || null,
    [profiles, selectedProfId]
  );

  useEffect(() => {
    if (activeProfile && activeProfile.professor_id !== selectedProfId) {
      setSelectedProfId(activeProfile.professor_id);
    }
  }, [activeProfile, selectedProfId]);

  const [prefDias, setPrefDias] = useState<Record<string, number>>({});
  const [turnos, setTurnos] = useState<Record<string, string[]>>({});
  const [disp, setDisp] = useState<Record<string, TimeSlot[]>>({});
  const [pesoJanela, setPesoJanela] = useState<number>(8);
  const [showAdvanced, setShowAdvanced] = useState(false);
  const [dirty, setDirty] = useState(false);
  const [feedback, setFeedback] = useState<{ type: "ok" | "err"; msg: string } | null>(null);

  useEffect(() => {
    if (!activeProfile) return;
    setPrefDias(buildInitialPrefs(activeProfile));
    setTurnos(buildInitialTurnos(activeProfile));
    setDisp(buildInitialDisp(activeProfile));
    setPesoJanela(activeProfile.peso_janela ?? 8);
    setDirty(false);
    setFeedback(null);
  }, [activeProfile]);

  const updateMutation = useUpdateProfessorPreferences(activeProfile?.professor_id || "");
  const { data: timetable, isLoading: loadingTimetable } = useProfessorTimetable(
    tab === "grade" && activeProfile ? activeProfile.professor_id : null
  );

  const canEdit = Boolean(activeProfile?.allow_preferences_edit);

  const toggleShift = (day: DayOfWeek, shift: string) => {
    if (!canEdit) return;
    const current = turnos[day] || [];
    const next = current.includes(shift)
      ? current.filter((s) => s !== shift)
      : SHIFTS.map((s) => s.key).filter((s) => s === shift || current.includes(s));
    setTurnos({ ...turnos, [day]: next });
    setDirty(true);
    setFeedback(null);
  };

  const applyShiftPreset = (selected: string[]) => {
    if (!canEdit) return;
    setTurnos(Object.fromEntries(ALL_DAYS.map((d) => [d, [...selected]])));
    setDirty(true);
    setFeedback(null);
  };

  const addDisp = (day: DayOfWeek) => {
    if (!canEdit) return;
    const list = [...(disp[day] || []), { inicio: "07:30", fim: "12:00" }];
    setDisp({ ...disp, [day]: list });
    setDirty(true);
    setFeedback(null);
  };

  const updateDisp = (day: DayOfWeek, idx: number, field: keyof TimeSlot, val: string) => {
    if (!canEdit) return;
    const list = [...(disp[day] || [])];
    list[idx] = { ...list[idx], [field]: val };
    setDisp({ ...disp, [day]: list });
    setDirty(true);
    setFeedback(null);
  };

  const removeDisp = (day: DayOfWeek, idx: number) => {
    if (!canEdit) return;
    const list = (disp[day] || []).filter((_, i) => i !== idx);
    setDisp({ ...disp, [day]: list });
    setDirty(true);
    setFeedback(null);
  };

  const savePreferences = async () => {
    if (!activeProfile || !canEdit) return;
    setFeedback(null);
    try {
      await updateMutation.mutateAsync({
        preferencia_dias: prefDias,
        turnos_pretendidos: turnos,
        disponibilidade: disp,
        peso_janela: pesoJanela,
      });
      setDirty(false);
      setFeedback({ type: "ok", msg: "Preferências salvas com sucesso!" });
    } catch (err) {
      setFeedback({
        type: "err",
        msg: err instanceof Error ? err.message : "Erro ao salvar preferências.",
      });
    }
  };

  return (
    <div className="min-h-screen bg-slate-50 dark:bg-slate-950">
      <header className="sticky top-0 z-30 border-b border-slate-200 bg-white/95 px-6 py-3.5 backdrop-blur dark:border-slate-800 dark:bg-slate-900/95">
        <div className="mx-auto flex max-w-5xl flex-wrap items-center justify-between gap-3">
          <div className="flex items-center gap-3">
            <img src="/app_icon.svg" alt="Logo" className="h-8 w-8" />
            <div>
              <h1 className="text-base font-bold">Portal do Professor</h1>
              <p className="text-xs text-slate-500">
                {user?.institution_name} • {user?.full_name}
              </p>
            </div>
          </div>
          <div className="flex items-center gap-2">
            <button
              onClick={() => navigate("/change-password")}
              className="inline-flex items-center gap-1.5 rounded-lg border border-slate-300 px-3 py-1.5 text-xs font-medium hover:bg-slate-50 dark:border-slate-700 dark:hover:bg-slate-800"
            >
              <KeyRound className="h-3.5 w-3.5" /> Alterar Senha
            </button>
            <button
              onClick={toggleDarkMode}
              className="rounded-lg border border-slate-300 p-2 dark:border-slate-700"
            >
              {darkMode ? <Sun className="h-4 w-4" /> : <Moon className="h-4 w-4" />}
            </button>
            <button
              onClick={() => {
                logout();
                navigate("/login");
              }}
              className="inline-flex items-center gap-1.5 rounded-lg border border-slate-300 px-3 py-1.5 text-xs font-medium hover:bg-slate-50 dark:border-slate-700 dark:hover:bg-slate-800"
            >
              <LogOut className="h-4 w-4" /> Sair
            </button>
          </div>
        </div>
      </header>

      <main className="mx-auto max-w-5xl space-y-5 p-6">
        {isLoading ? (
          <p className="text-sm text-slate-500">Carregando seus perfis...</p>
        ) : !activeProfile ? (
          <div className="rounded-xl border border-slate-200 bg-white p-8 text-center dark:border-slate-800 dark:bg-slate-900">
            <h2 className="text-base font-bold">Nenhum projeto vinculado</h2>
            <p className="mt-1 text-sm text-slate-500">
              Sua conta ainda não está associada a nenhum projeto de grade horária. Solicite à coordenação o vínculo do seu e-mail ({user?.email}).
            </p>
          </div>
        ) : (
          <>
            <div className="flex flex-wrap items-center justify-between gap-3 rounded-xl border border-slate-200 bg-white p-4 dark:border-slate-800 dark:bg-slate-900">
              <div>
                <div className="text-xs font-medium text-slate-500">
                  Professor <span className="font-mono font-semibold">{activeProfile.code}</span> • Ano Letivo {activeProfile.academic_year}
                </div>
                <div className="text-base font-bold">{activeProfile.project_name}</div>
              </div>

              <div className="flex flex-wrap items-center gap-3">
                {profiles.length > 1 && (
                  <select
                    value={activeProfile.professor_id}
                    onChange={(e) => setSelectedProfId(e.target.value)}
                    className="rounded-lg border border-slate-300 bg-white px-3 py-1.5 text-sm dark:border-slate-700 dark:bg-slate-800"
                  >
                    {profiles.map((p) => (
                      <option key={p.professor_id} value={p.professor_id}>
                        {p.project_name} ({p.code})
                      </option>
                    ))}
                  </select>
                )}

                {canEdit ? (
                  <span className="inline-flex items-center gap-1.5 rounded-full bg-emerald-100 px-3 py-1 text-xs font-semibold text-emerald-800 dark:bg-emerald-950 dark:text-emerald-300">
                    <LockOpen className="h-3.5 w-3.5" /> Edição liberada
                  </span>
                ) : (
                  <span className="inline-flex items-center gap-1.5 rounded-full bg-amber-100 px-3 py-1 text-xs font-semibold text-amber-800 dark:bg-amber-950 dark:text-amber-300">
                    <Lock className="h-3.5 w-3.5" /> Edição bloqueada pelo administrador
                  </span>
                )}
              </div>
            </div>

            {!canEdit && (
              <div className="flex items-start gap-2.5 rounded-xl border border-amber-300 bg-amber-50 p-3.5 text-xs text-amber-900 dark:border-amber-800 dark:bg-amber-950/40 dark:text-amber-200">
                <Lock className="mt-0.5 h-4 w-4 shrink-0" />
                <div>
                  <strong>Edição de preferências bloqueada:</strong> o administrador da instituição bloqueou temporariamente alterações neste projeto. Você ainda pode consultar suas preferências atuais e visualizar sua grade horária.
                </div>
              </div>
            )}

            <div className="flex gap-2 border-b border-slate-200 dark:border-slate-800">
              <button
                onClick={() => setTab("preferencias")}
                className={`inline-flex items-center gap-1.5 rounded-t-lg px-4 py-2 text-sm font-medium transition ${
                  tab === "preferencias"
                    ? "border-b-2 border-blue-600 bg-white text-blue-600 dark:bg-slate-900 dark:text-blue-400"
                    : "text-slate-600 hover:text-slate-900 dark:text-slate-400"
                }`}
              >
                <Sliders className="h-4 w-4" /> Minhas Preferências
              </button>
              <button
                onClick={() => setTab("grade")}
                className={`inline-flex items-center gap-1.5 rounded-t-lg px-4 py-2 text-sm font-medium transition ${
                  tab === "grade"
                    ? "border-b-2 border-blue-600 bg-white text-blue-600 dark:bg-slate-900 dark:text-blue-400"
                    : "text-slate-600 hover:text-slate-900 dark:text-slate-400"
                }`}
              >
                <Calendar className="h-4 w-4" /> Minha Grade Horária
              </button>
            </div>

            {tab === "preferencias" ? (
              <div className="space-y-5">
                <section className="rounded-xl border border-slate-200 bg-white p-5 dark:border-slate-800 dark:bg-slate-900">
                  <div className="flex flex-wrap items-center justify-between gap-2">
                    <div>
                      <h2 className="text-sm font-bold">Turnos Pretendidos por Dia da Semana</h2>
                      <p className="text-xs text-slate-500">
                        Selecione os turnos em que você prefere lecionar em cada dia. Turnos desmarcados recebem penalidade na otimização.
                      </p>
                    </div>
                    <div className="flex flex-wrap gap-1.5 text-xs">
                      <button
                        type="button"
                        disabled={!canEdit}
                        onClick={() => applyShiftPreset(["MANHA", "TARDE", "NOITE"])}
                        className="rounded border border-slate-300 px-2.5 py-1 hover:bg-slate-50 disabled:opacity-50 dark:border-slate-700 dark:hover:bg-slate-800"
                      >
                        Todos
                      </button>
                      <button
                        type="button"
                        disabled={!canEdit}
                        onClick={() => applyShiftPreset(["MANHA"])}
                        className="rounded border border-slate-300 px-2.5 py-1 hover:bg-slate-50 disabled:opacity-50 dark:border-slate-700 dark:hover:bg-slate-800"
                      >
                        Apenas Manhã
                      </button>
                      <button
                        type="button"
                        disabled={!canEdit}
                        onClick={() => applyShiftPreset(["MANHA", "TARDE"])}
                        className="rounded border border-slate-300 px-2.5 py-1 hover:bg-slate-50 disabled:opacity-50 dark:border-slate-700 dark:hover:bg-slate-800"
                      >
                        Manhã e Tarde
                      </button>
                      <button
                        type="button"
                        disabled={!canEdit}
                        onClick={() => applyShiftPreset(["NOITE"])}
                        className="rounded border border-slate-300 px-2.5 py-1 hover:bg-slate-50 disabled:opacity-50 dark:border-slate-700 dark:hover:bg-slate-800"
                      >
                        Apenas Noite
                      </button>
                    </div>
                  </div>

                  <div className="mt-4 overflow-x-auto">
                    <table className="w-full text-left text-sm">
                      <thead className="bg-slate-50 text-xs uppercase text-slate-500 dark:bg-slate-800/60">
                        <tr>
                          <th className="px-4 py-2">Dia</th>
                          {SHIFTS.map((s) => (
                            <th key={s.key} className="px-4 py-2 text-center">
                              {s.label}
                            </th>
                          ))}
                        </tr>
                      </thead>
                      <tbody className="divide-y divide-slate-200 dark:divide-slate-800">
                        {ALL_DAYS.map((day) => (
                          <tr key={day}>
                            <td className="px-4 py-2.5 font-medium">{DAY_LABELS[day]}</td>
                            {SHIFTS.map((s) => {
                              const checked = (turnos[day] || []).includes(s.key);
                              return (
                                <td key={s.key} className="px-4 py-2.5 text-center">
                                  <input
                                    type="checkbox"
                                    disabled={!canEdit}
                                    checked={checked}
                                    onChange={() => toggleShift(day, s.key)}
                                    className="h-4 w-4 rounded border-slate-300 text-blue-600 disabled:opacity-50"
                                  />
                                </td>
                              );
                            })}
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                </section>

                <section className="rounded-xl border border-slate-200 bg-white p-5 dark:border-slate-800 dark:bg-slate-900">
                  <h2 className="text-sm font-bold">Preferência por Dia da Semana (0 a 10)</h2>
                  <p className="text-xs text-slate-500">
                    0 = dia menos desejado, 5 = neutro, 10 = maior preferência para alocação de aulas.
                  </p>
                  <div className="mt-4 grid gap-3 sm:grid-cols-5">
                    {ALL_DAYS.map((d) => {
                      const val = prefDias[d] ?? 5;
                      return (
                        <div key={d} className="rounded-lg border border-slate-200 p-3 text-center dark:border-slate-700">
                          <div className="text-xs font-bold">{DAY_SHORT_LABELS[d]}</div>
                          <div className="mt-1 text-lg font-bold text-blue-600 dark:text-blue-400">{val}</div>
                          <div className="text-[11px] text-slate-500">{prefLabel(val)}</div>
                          <input
                            type="range"
                            min={0}
                            max={10}
                            disabled={!canEdit}
                            value={val}
                            onChange={(e) => {
                              setPrefDias({ ...prefDias, [d]: Number(e.target.value) });
                              setDirty(true);
                              setFeedback(null);
                            }}
                            className="mt-2 w-full disabled:opacity-50"
                          />
                        </div>
                      );
                    })}
                  </div>
                </section>

                <section className="rounded-xl border border-slate-200 bg-white p-5 dark:border-slate-800 dark:bg-slate-900">
                  <button
                    type="button"
                    onClick={() => setShowAdvanced((s) => !s)}
                    className="flex w-full items-center justify-between text-left"
                  >
                    <div>
                      <h2 className="text-sm font-bold">Disponibilidade de Horários & Aversão a Janelas (Opcional)</h2>
                      <p className="text-xs text-slate-500">
                        Ajuste janelas de horários disponíveis por dia e sua tolerância a horários ociosos.
                      </p>
                    </div>
                    {showAdvanced ? <ChevronUp className="h-4 w-4" /> : <ChevronDown className="h-4 w-4" />}
                  </button>

                  {showAdvanced && (
                    <div className="mt-4 space-y-4 border-t border-slate-200 pt-4 dark:border-slate-800">
                      <div>
                        <label className="text-xs font-medium text-slate-500">
                          Aversão a Janelas ({pesoJanela}/10)
                        </label>
                        <input
                          type="range"
                          min={0}
                          max={10}
                          disabled={!canEdit}
                          value={pesoJanela}
                          onChange={(e) => {
                            setPesoJanela(Number(e.target.value));
                            setDirty(true);
                            setFeedback(null);
                          }}
                          className="mt-1 w-full max-w-xs disabled:opacity-50"
                        />
                      </div>

                      <div className="space-y-2">
                        {ALL_DAYS.map((d) => (
                          <div key={d} className="rounded border border-slate-200 p-2.5 dark:border-slate-800">
                            <div className="flex items-center justify-between">
                              <span className="text-xs font-bold">{DAY_LABELS[d]}</span>
                              <button
                                type="button"
                                disabled={!canEdit}
                                onClick={() => addDisp(d)}
                                className="inline-flex items-center gap-1 text-xs text-blue-600 hover:underline disabled:opacity-50"
                              >
                                <Plus className="h-3.5 w-3.5" /> Faixa
                              </button>
                            </div>
                            <div className="mt-1.5 flex flex-wrap gap-2">
                              {(disp[d] || []).length === 0 ? (
                                <span className="text-xs text-slate-400">Nenhuma faixa configurada</span>
                              ) : (
                                (disp[d] || []).map((slot, idx) => (
                                  <div
                                    key={idx}
                                    className="flex items-center gap-1 rounded bg-slate-100 px-2 py-1 dark:bg-slate-800"
                                  >
                                    <input
                                      type="time"
                                      disabled={!canEdit}
                                      value={slot.inicio}
                                      onChange={(e) => updateDisp(d, idx, "inicio", e.target.value)}
                                      className="bg-transparent text-xs disabled:opacity-60"
                                    />
                                    <span>-</span>
                                    <input
                                      type="time"
                                      disabled={!canEdit}
                                      value={slot.fim}
                                      onChange={(e) => updateDisp(d, idx, "fim", e.target.value)}
                                      className="bg-transparent text-xs disabled:opacity-60"
                                    />
                                    {canEdit && (
                                      <button
                                        type="button"
                                        onClick={() => removeDisp(d, idx)}
                                        className="text-rose-500"
                                      >
                                        <X className="h-3.5 w-3.5" />
                                      </button>
                                    )}
                                  </div>
                                ))
                              )}
                            </div>
                          </div>
                        ))}
                      </div>
                    </div>
                  )}
                </section>

                <div className="flex flex-wrap items-center justify-between gap-3 rounded-xl border border-slate-200 bg-white p-4 dark:border-slate-800 dark:bg-slate-900">
                  <div className="text-xs">
                    {feedback ? (
                      <span
                        className={`inline-flex items-center gap-1.5 font-medium ${
                          feedback.type === "ok"
                            ? "text-emerald-600 dark:text-emerald-400"
                            : "text-rose-600 dark:text-rose-400"
                        }`}
                      >
                        {feedback.type === "ok" && <CheckCircle2 className="h-4 w-4" />}
                        {feedback.msg}
                      </span>
                    ) : dirty ? (
                      <span className="font-medium text-amber-600 dark:text-amber-400">
                        Você possui alterações não salvas.
                      </span>
                    ) : (
                      <span className="text-slate-500">Tudo salvo.</span>
                    )}
                  </div>
                  <button
                    type="button"
                    disabled={!canEdit || !dirty || updateMutation.isPending}
                    onClick={savePreferences}
                    className="inline-flex items-center gap-2 rounded-lg bg-blue-600 px-5 py-2 text-sm font-semibold text-white hover:bg-blue-700 disabled:opacity-50"
                  >
                    <Save className="h-4 w-4" />
                    {updateMutation.isPending ? "Salvando..." : "Salvar Preferências"}
                  </button>
                </div>
              </div>
            ) : (
              <section className="rounded-xl border border-slate-200 bg-white p-5 dark:border-slate-800 dark:bg-slate-900">
                <div className="mb-4 flex flex-wrap items-center justify-between gap-2">
                  <div>
                    <h2 className="text-sm font-bold">Minha Grade Horária</h2>
                    <p className="text-xs text-slate-500">
                      {timetable?.version_number
                        ? `Versão ativa: v${timetable.version_number} — ${timetable.version_label}`
                        : "Nenhuma grade gerada até o momento."}
                    </p>
                  </div>
                  <span className="rounded bg-slate-100 px-2.5 py-1 text-xs font-medium dark:bg-slate-800">
                    {timetable?.alocacoes.length ?? 0} aula(s) alocada(s)
                  </span>
                </div>

                {loadingTimetable ? (
                  <p className="text-sm text-slate-500">Carregando grade horária...</p>
                ) : !timetable || timetable.alocacoes.length === 0 ? (
                  <p className="py-6 text-center text-sm text-slate-500">
                    Ainda não há aulas alocadas para você nesta grade.
                  </p>
                ) : (
                  <div className="grid gap-3 md:grid-cols-5">
                    {ALL_DAYS.map((day) => {
                      const dayLessons = timetable.alocacoes.filter((a) => a.dia === day);
                      return (
                        <div key={day} className="rounded-lg border border-slate-200 p-3 dark:border-slate-800">
                          <div className="mb-2 border-b border-slate-200 pb-1.5 text-center text-xs font-bold uppercase text-slate-600 dark:border-slate-800 dark:text-slate-300">
                            {DAY_LABELS[day]}
                          </div>
                          {dayLessons.length === 0 ? (
                            <p className="py-4 text-center text-xs text-slate-400">Sem aulas</p>
                          ) : (
                            <div className="space-y-2">
                              {dayLessons.map((lesson) => (
                                <div
                                  key={lesson.aula_id}
                                  className="rounded-lg border border-blue-200 bg-blue-50/70 p-2.5 text-xs dark:border-blue-900/60 dark:bg-blue-950/30"
                                >
                                  <div className="font-mono text-[11px] font-bold text-blue-700 dark:text-blue-300">
                                    {lesson.inicio} – {lesson.fim}
                                  </div>
                                  <div className="mt-0.5 font-semibold">{lesson.disciplina_nome}</div>
                                  <div className="mt-1 text-[11px] text-slate-600 dark:text-slate-300">
                                    Turma:{" "}
                                    {lesson.turmas
                                      .map((t) => timetable.turmas_map[t] || t)
                                      .join(", ") || "-"}
                                  </div>
                                  <div className="text-[11px] text-slate-600 dark:text-slate-300">
                                    Sala:{" "}
                                    {lesson.salas
                                      .map((s) => timetable.salas_map[s] || s)
                                      .join(", ") || "-"}
                                  </div>
                                  {lesson.regime === "SEMESTRAL" && (
                                    <div className="mt-1 text-[10px] font-medium text-slate-500">
                                      Semestre {lesson.semestre_alocado}
                                    </div>
                                  )}
                                </div>
                              ))}
                            </div>
                          )}
                        </div>
                      );
                    })}
                  </div>
                )}
              </section>
            )}
          </>
        )}
      </main>
    </div>
  );
}
