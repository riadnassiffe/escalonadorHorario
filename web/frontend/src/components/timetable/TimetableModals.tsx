import { useMemo, useState } from "react";
import { AlertTriangle, CheckCircle2, X } from "lucide-react";
import { evaluateLessonAtSlot, evaluateLessonSwap, type DomainSnapshot } from "../../domain/conflictEngine";
import { ALL_DAYS, DAY_LABELS, formatInterval, minToTime, timeToMin } from "../../domain/time";
import type { AllocatedLesson, DayOfWeek, SlotConflictInfo, UnallocatedLesson } from "../../types";

export function MoveLessonModal({
  lesson,
  snapshot,
  onClose,
  onConfirm,
}: {
  lesson: AllocatedLesson;
  snapshot: DomainSnapshot;
  onClose: () => void;
  onConfirm: (dia: DayOfWeek, inicio: string, fim: string, salas: string[], pin: boolean) => void;
}) {
  const [dia, setDia] = useState<DayOfWeek>(lesson.dia);
  const [slotKey, setSlotKey] = useState(`${lesson.inicio}-${lesson.fim}`);
  const [sala, setSala] = useState(lesson.salas[0] || "");
  const [pin, setPin] = useState(lesson.is_pinned);

  const availableSlots = useMemo(() => {
    const dur = lesson.end_min - lesson.start_min;
    const set = new Map<string, { inicio: string; fim: string }>();
    set.set(`${lesson.inicio}-${lesson.fim}`, { inicio: lesson.inicio, fim: lesson.fim });
    for (const tId of lesson.turmas) {
      const t = snapshot.turmas[tId];
      if (!t) continue;
      const slots = t.excecoes_por_dia?.[dia]?.slots || t.jornada_padrao?.slots || [];
      for (const s of slots) {
        if (timeToMin(s.fim) - timeToMin(s.inicio) === dur) {
          set.set(`${s.inicio}-${s.fim}`, s);
        }
      }
    }
    return Array.from(set.values()).sort((a, b) => timeToMin(a.inicio) - timeToMin(b.inicio));
  }, [dia, lesson, snapshot.turmas]);

  const [inicio, fim] = slotKey.split("-");
  const conflicts = useMemo(
    () =>
      evaluateLessonAtSlot(
        snapshot,
        lesson.aula_id,
        dia,
        timeToMin(inicio),
        timeToMin(fim),
        [],
        sala ? [sala] : undefined
      ),
    [dia, fim, inicio, lesson.aula_id, sala, snapshot]
  );

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
      <div className="w-full max-w-lg rounded-xl bg-white p-5 shadow-2xl dark:bg-slate-900">
        <div className="flex items-center justify-between border-b border-slate-200 pb-3 dark:border-slate-800">
          <h3 className="text-base font-bold">Mover Horário / Sala — {lesson.disciplina_nome}</h3>
          <button onClick={onClose} className="text-slate-400 hover:text-slate-600"><X className="h-5 w-5" /></button>
        </div>
        <div className="mt-4 space-y-3 text-sm">
          <div>
            <label className="block text-xs font-medium text-slate-500">Dia da Semana</label>
            <select value={dia} onChange={(e) => setDia(e.target.value as DayOfWeek)} className="mt-1 w-full rounded-md border border-slate-300 bg-white px-3 py-2 dark:border-slate-700 dark:bg-slate-800">
              {ALL_DAYS.map((d) => <option key={d} value={d}>{DAY_LABELS[d]}</option>)}
            </select>
          </div>
          <div>
            <label className="block text-xs font-medium text-slate-500">Horário Letivo</label>
            <select value={slotKey} onChange={(e) => setSlotKey(e.target.value)} className="mt-1 w-full rounded-md border border-slate-300 bg-white px-3 py-2 dark:border-slate-700 dark:bg-slate-800">
              {availableSlots.map((s) => (
                <option key={`${s.inicio}-${s.fim}`} value={`${s.inicio}-${s.fim}`}>{s.inicio} - {s.fim}</option>
              ))}
            </select>
          </div>
          <div>
            <label className="block text-xs font-medium text-slate-500">Sala</label>
            <select value={sala} onChange={(e) => setSala(e.target.value)} className="mt-1 w-full rounded-md border border-slate-300 bg-white px-3 py-2 dark:border-slate-700 dark:bg-slate-800">
              <option value="">(Manter atual)</option>
              {Object.values(snapshot.salas).map((r) => (
                <option key={r.code} value={r.code}>{r.nome} ({r.tipo})</option>
              ))}
            </select>
          </div>
          <label className="flex items-center gap-2 text-sm">
            <input type="checkbox" checked={pin} onChange={(e) => setPin(e.target.checked)} />
            Fixar aula (Pin) após movimentar
          </label>
          <ConflictBox conflicts={conflicts} />
        </div>
        <div className="mt-5 flex justify-end gap-2">
          <button onClick={onClose} className="rounded-md border border-slate-300 px-4 py-2 text-sm dark:border-slate-700">Cancelar</button>
          <button
            onClick={() => onConfirm(dia, inicio, fim, sala ? [sala] : lesson.salas, pin)}
            className="rounded-md bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-700"
          >
            Confirmar Movimentação
          </button>
        </div>
      </div>
    </div>
  );
}

export function SwapLessonModal({
  sourceLesson,
  snapshot,
  onClose,
  onConfirm,
}: {
  sourceLesson: AllocatedLesson;
  snapshot: DomainSnapshot;
  onClose: () => void;
  onConfirm: (targetAulaId: string, pinBoth: boolean) => void;
}) {
  const [filter, setFilter] = useState("");
  const candidates = useMemo(
    () =>
      snapshot.alocacoes.filter(
        (a) =>
          a.aula_id !== sourceLesson.aula_id &&
          `${a.disciplina_nome} ${a.turmas.join(" ")} ${a.professores.join(" ")} ${a.dia}`
            .toLowerCase()
            .includes(filter.toLowerCase())
      ),
    [filter, snapshot.alocacoes, sourceLesson.aula_id]
  );
  const [targetId, setTargetId] = useState(candidates[0]?.aula_id || "");
  const [pinBoth, setPinBoth] = useState(false);

  const conflicts = useMemo(
    () => (targetId ? evaluateLessonSwap(snapshot, sourceLesson.aula_id, targetId) : []),
    [snapshot, sourceLesson.aula_id, targetId]
  );

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
      <div className="w-full max-w-xl rounded-xl bg-white p-5 shadow-2xl dark:bg-slate-900">
        <div className="flex items-center justify-between border-b border-slate-200 pb-3 dark:border-slate-800">
          <h3 className="text-base font-bold">Trocar Horários (Swap)</h3>
          <button onClick={onClose} className="text-slate-400 hover:text-slate-600"><X className="h-5 w-5" /></button>
        </div>
        <div className="mt-3 rounded-lg bg-slate-100 p-2.5 text-xs dark:bg-slate-800">
          <strong>Origem:</strong> {sourceLesson.disciplina_nome} ({sourceLesson.turmas.join(", ")}) — {DAY_LABELS[sourceLesson.dia]} {sourceLesson.inicio}-{sourceLesson.fim}
        </div>
        <div className="mt-3 space-y-3 text-sm">
          <input
            type="text"
            value={filter}
            onChange={(e) => setFilter(e.target.value)}
            placeholder="Filtrar aula de destino por matéria, turma ou dia..."
            className="w-full rounded-md border border-slate-300 px-3 py-1.5 text-sm dark:border-slate-700 dark:bg-slate-800"
          />
          <select
            value={targetId}
            onChange={(e) => setTargetId(e.target.value)}
            size={6}
            className="w-full rounded-md border border-slate-300 bg-white p-1.5 text-xs dark:border-slate-700 dark:bg-slate-800"
          >
            {candidates.map((c) => (
              <option key={c.aula_id} value={c.aula_id}>
                {c.disciplina_nome} ({c.turmas.join(", ")}) — {c.dia} {c.inicio}-{c.fim} ({c.aula_id})
              </option>
            ))}
          </select>
          <label className="flex items-center gap-2 text-sm">
            <input type="checkbox" checked={pinBoth} onChange={(e) => setPinBoth(e.target.checked)} />
            Fixar (Pin) ambas as aulas após a troca
          </label>
          {targetId && <ConflictBox conflicts={conflicts} />}
        </div>
        <div className="mt-5 flex justify-end gap-2">
          <button onClick={onClose} className="rounded-md border border-slate-300 px-4 py-2 text-sm dark:border-slate-700">Cancelar</button>
          <button
            disabled={!targetId}
            onClick={() => onConfirm(targetId, pinBoth)}
            className="rounded-md bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-700 disabled:opacity-50"
          >
            Confirmar Troca
          </button>
        </div>
      </div>
    </div>
  );
}

export function ChangeRoomModal({
  lesson,
  snapshot,
  onClose,
  onConfirm,
}: {
  lesson: AllocatedLesson;
  snapshot: DomainSnapshot;
  onClose: () => void;
  onConfirm: (salas: string[], swapWithAulaId: string | null, pinAfter: boolean) => void;
}) {
  const [roomCode, setRoomCode] = useState(lesson.salas[0] || "");
  const [pinAfter, setPinAfter] = useState(lesson.is_pinned);

  const conflicts = useMemo(
    () =>
      roomCode
        ? evaluateLessonAtSlot(snapshot, lesson.aula_id, lesson.dia, lesson.start_min, lesson.end_min, [], [roomCode])
        : [],
    [lesson, roomCode, snapshot]
  );

  const occupyingAulaId = conflicts.find((c) => c.type === "CHOQUE_SALA" && c.related_aula_id)?.related_aula_id || null;

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
      <div className="w-full max-w-md rounded-xl bg-white p-5 shadow-2xl dark:bg-slate-900">
        <div className="flex items-center justify-between border-b border-slate-200 pb-3 dark:border-slate-800">
          <h3 className="text-base font-bold">Alterar Sala — {lesson.disciplina_nome}</h3>
          <button onClick={onClose} className="text-slate-400 hover:text-slate-600"><X className="h-5 w-5" /></button>
        </div>
        <div className="mt-4 space-y-3 text-sm">
          <select value={roomCode} onChange={(e) => setRoomCode(e.target.value)} className="w-full rounded-md border border-slate-300 bg-white px-3 py-2 dark:border-slate-700 dark:bg-slate-800">
            {Object.values(snapshot.salas).map((r) => (
              <option key={r.code} value={r.code}>{r.nome} ({r.tipo}, cap. {r.capacidade})</option>
            ))}
          </select>
          <label className="flex items-center gap-2 text-sm">
            <input type="checkbox" checked={pinAfter} onChange={(e) => setPinAfter(e.target.checked)} />
            Fixar aula após alteração
          </label>
          <ConflictBox conflicts={conflicts} />
        </div>
        <div className="mt-5 flex justify-end gap-2">
          <button onClick={onClose} className="rounded-md border border-slate-300 px-4 py-2 text-sm dark:border-slate-700">Cancelar</button>
          {occupyingAulaId && (
            <button
              onClick={() => onConfirm([roomCode], occupyingAulaId, pinAfter)}
              className="rounded-md bg-amber-600 px-3 py-2 text-sm font-medium text-white hover:bg-amber-700"
            >
              Trocar Salas (Swap)
            </button>
          )}
          <button
            disabled={!roomCode}
            onClick={() => onConfirm([roomCode], null, pinAfter)}
            className="rounded-md bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-700"
          >
            Aplicar Sala
          </button>
        </div>
      </div>
    </div>
  );
}

export function AddLessonManualModal({
  unallocated,
  snapshot,
  initialDia,
  initialStartMin,
  initialEndMin,
  onClose,
  onConfirm,
}: {
  unallocated: UnallocatedLesson[];
  snapshot: DomainSnapshot;
  initialDia?: DayOfWeek;
  initialStartMin?: number;
  initialEndMin?: number;
  onClose: () => void;
  onConfirm: (payload: {
    aula_id: string;
    disciplina_id: string;
    dia: DayOfWeek;
    inicio: string;
    fim: string;
    salas: string[];
    semestre_alocado: number;
    is_pinned: boolean;
  }) => void;
}) {
  const [selectedAulaId, setSelectedAulaId] = useState(unallocated[0]?.aula_id || "");
  const [dia, setDia] = useState<DayOfWeek>(initialDia || "SEG");
  const [inicio, setInicio] = useState(initialStartMin ? minToTime(initialStartMin) : "08:00");
  const [fim, setFim] = useState(initialEndMin ? minToTime(initialEndMin) : "08:50");
  const [sala, setSala] = useState(Object.keys(snapshot.salas)[0] || "");
  const [pin, setPin] = useState(true);

  const selected = unallocated.find((u) => u.aula_id === selectedAulaId);

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
      <div className="w-full max-w-lg rounded-xl bg-white p-5 shadow-2xl dark:bg-slate-900">
        <div className="flex items-center justify-between border-b border-slate-200 pb-3 dark:border-slate-800">
          <h3 className="text-base font-bold">Alocar Disciplina Manualmente</h3>
          <button onClick={onClose} className="text-slate-400 hover:text-slate-600"><X className="h-5 w-5" /></button>
        </div>
        {unallocated.length === 0 ? (
          <p className="py-6 text-center text-sm text-slate-500">Todas as ocorrências de aulas já estão alocadas na grade.</p>
        ) : (
          <div className="mt-4 space-y-3 text-sm">
            <div>
              <label className="block text-xs font-medium text-slate-500">Ocorrência de Aula</label>
              <select value={selectedAulaId} onChange={(e) => setSelectedAulaId(e.target.value)} className="mt-1 w-full rounded-md border border-slate-300 bg-white px-3 py-2 dark:border-slate-700 dark:bg-slate-800">
                {unallocated.map((u) => (
                  <option key={u.aula_id} value={u.aula_id}>
                    {u.disciplina_nome} ({u.turmas.join(", ")}) — {u.duracao_minutos}m [{u.aula_id}]
                  </option>
                ))}
              </select>
            </div>
            <div className="grid grid-cols-3 gap-2">
              <div>
                <label className="block text-xs font-medium text-slate-500">Dia</label>
                <select value={dia} onChange={(e) => setDia(e.target.value as DayOfWeek)} className="mt-1 w-full rounded-md border border-slate-300 bg-white px-2 py-2 dark:border-slate-700 dark:bg-slate-800">
                  {ALL_DAYS.map((d) => <option key={d} value={d}>{DAY_LABELS[d]}</option>)}
                </select>
              </div>
              <div>
                <label className="block text-xs font-medium text-slate-500">Início</label>
                <input type="time" value={inicio} onChange={(e) => setInicio(e.target.value)} className="mt-1 w-full rounded-md border border-slate-300 px-2 py-1.5 dark:border-slate-700 dark:bg-slate-800" />
              </div>
              <div>
                <label className="block text-xs font-medium text-slate-500">Fim</label>
                <input type="time" value={fim} onChange={(e) => setFim(e.target.value)} className="mt-1 w-full rounded-md border border-slate-300 px-2 py-1.5 dark:border-slate-700 dark:bg-slate-800" />
              </div>
            </div>
            <div>
              <label className="block text-xs font-medium text-slate-500">Sala</label>
              <select value={sala} onChange={(e) => setSala(e.target.value)} className="mt-1 w-full rounded-md border border-slate-300 bg-white px-3 py-2 dark:border-slate-700 dark:bg-slate-800">
                {Object.values(snapshot.salas).map((r) => (
                  <option key={r.code} value={r.code}>{r.nome} ({r.tipo})</option>
                ))}
              </select>
            </div>
            <label className="flex items-center gap-2 text-sm">
              <input type="checkbox" checked={pin} onChange={(e) => setPin(e.target.checked)} />
              Fixar aula alocada (Pin)
            </label>
          </div>
        )}
        <div className="mt-5 flex justify-end gap-2">
          <button onClick={onClose} className="rounded-md border border-slate-300 px-4 py-2 text-sm dark:border-slate-700">Fechar</button>
          {selected && (
            <button
              onClick={() =>
                onConfirm({
                  aula_id: selected.aula_id,
                  disciplina_id: selected.disciplina_id,
                  dia,
                  inicio,
                  fim,
                  salas: sala ? [sala] : [],
                  semestre_alocado: selected.semestre === "2" ? 2 : 1,
                  is_pinned: pin,
                })
              }
              className="rounded-md bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-700"
            >
              Alocar na Grade
            </button>
          )}
        </div>
      </div>
    </div>
  );
}

export function DropDecisionModal({
  dragged,
  targetDia,
  targetStartMin,
  targetEndMin,
  occupying,
  conflicts,
  snapshot,
  onClose,
  onMove,
  onSwap,
}: {
  dragged: AllocatedLesson;
  targetDia: DayOfWeek;
  targetStartMin: number;
  targetEndMin: number;
  occupying: AllocatedLesson[];
  conflicts: SlotConflictInfo[];
  snapshot: DomainSnapshot;
  onClose: () => void;
  onMove: () => void;
  onSwap: (targetAulaId: string) => void;
}) {
  const [swapTarget, setSwapTarget] = useState(occupying[0]?.aula_id || "");
  const swapConflicts = useMemo(
    () => (swapTarget ? evaluateLessonSwap(snapshot, dragged.aula_id, swapTarget) : []),
    [dragged.aula_id, snapshot, swapTarget]
  );

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
      <div className="w-full max-w-lg rounded-xl bg-white p-5 shadow-2xl dark:bg-slate-900">
        <div className="flex items-center justify-between border-b border-slate-200 pb-3 dark:border-slate-800">
          <h3 className="text-base font-bold">Confirmar Ação no Horário ({DAY_LABELS[targetDia]} {formatInterval(targetStartMin, targetEndMin)})</h3>
          <button onClick={onClose} className="text-slate-400 hover:text-slate-600"><X className="h-5 w-5" /></button>
        </div>

        <div className="mt-4 space-y-3 text-sm">
          {occupying.length > 0 && (
            <div className="space-y-2 rounded-lg border border-blue-200 bg-blue-50/70 p-3 dark:border-blue-900 dark:bg-blue-950/30">
              <div className="font-semibold text-blue-900 dark:text-blue-200">
                Horário ocupado — Deseja trocar (Swap) com:
              </div>
              <select
                value={swapTarget}
                onChange={(e) => setSwapTarget(e.target.value)}
                className="w-full rounded border border-slate-300 bg-white px-2.5 py-1.5 text-xs dark:border-slate-700 dark:bg-slate-800"
              >
                {occupying.map((o) => (
                  <option key={o.aula_id} value={o.aula_id}>
                    {o.disciplina_nome} ({o.turmas.join(", ")})
                  </option>
                ))}
              </select>
              <ConflictBox conflicts={swapConflicts} />
            </div>
          )}

          {conflicts.length > 0 && (
            <div>
              <div className="mb-1 text-xs font-semibold text-slate-500">Conflitos se mover diretamente:</div>
              <ConflictBox conflicts={conflicts} />
            </div>
          )}
        </div>

        <div className="mt-5 flex flex-wrap justify-end gap-2">
          <button onClick={onClose} className="rounded-md border border-slate-300 px-4 py-2 text-sm dark:border-slate-700">Cancelar</button>
          {occupying.length > 0 && swapTarget && (
            <button
              onClick={() => onSwap(swapTarget)}
              className="rounded-md bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-700"
            >
              Trocar Horários (Swap)
            </button>
          )}
          <button
            onClick={onMove}
            className="rounded-md bg-amber-600 px-4 py-2 text-sm font-medium text-white hover:bg-amber-700"
          >
            Mover Mesmo Assim
          </button>
        </div>
      </div>
    </div>
  );
}

function ConflictBox({ conflicts }: { conflicts: SlotConflictInfo[] }) {
  if (conflicts.length === 0) {
    return (
      <div className="flex items-center gap-2 rounded-lg border border-emerald-200 bg-emerald-50 p-2.5 text-xs text-emerald-800 dark:border-emerald-900 dark:bg-emerald-950/40 dark:text-emerald-300">
        <CheckCircle2 className="h-4 w-4 shrink-0" />
        Horário totalmente livre, sem choques ou violações!
      </div>
    );
  }
  return (
    <div className="space-y-1 rounded-lg border border-rose-200 bg-rose-50 p-2.5 text-xs text-rose-800 dark:border-rose-900 dark:bg-rose-950/40 dark:text-rose-300">
      {conflicts.map((c, idx) => (
        <div key={idx} className="flex items-start gap-1.5">
          <AlertTriangle className="mt-0.5 h-3.5 w-3.5 shrink-0" />
          <span>{c.detail}</span>
        </div>
      ))}
    </div>
  );
}
