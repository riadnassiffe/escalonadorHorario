import { useEffect, useMemo, useState } from "react";
import { AlertTriangle, ArrowLeftRight, Lock, Plus, Search, Trash2, Unlock } from "lucide-react";
import { useTimetableMutations, useUnallocated } from "../../api/hooks";
import type { DomainSnapshot } from "../../domain/conflictEngine";
import { computeImplicitBreaks, minToTime, timeToMin } from "../../domain/time";
import { useTimetableStore, type ViewPerspective } from "../../store/timetableStore";
import { useUiStore } from "../../store/uiStore";
import type { AllocatedLesson, DayOfWeek, SlotConflictInfo, TimetableResponse } from "../../types";
import { TimetableGrid } from "./TimetableGrid";
import {
  AddLessonManualModal,
  ChangeRoomModal,
  DropDecisionModal,
  MoveLessonModal,
  SwapLessonModal,
} from "./TimetableModals";

interface Props {
  projectId: string;
  snapshot: DomainSnapshot;
  timetable?: TimetableResponse;
}

export function TimetableView({ projectId, snapshot, timetable }: Props) {
  const {
    viewType,
    targetId,
    semesterFilter,
    searchQuery,
    selectedAulaId,
    setViewType,
    setTargetId,
    setSemesterFilter,
    setSearchQuery,
    setSelectedAulaId,
  } = useTimetableStore();
  const { transposeGrid, toggleTransposeGrid } = useUiStore();
  const mutations = useTimetableMutations(projectId);
  const { data: unallocatedData } = useUnallocated(projectId);

  const [moveModalLesson, setMoveModalLesson] = useState<AllocatedLesson | null>(null);
  const [swapModalLesson, setSwapModalLesson] = useState<AllocatedLesson | null>(null);
  const [roomModalLesson, setRoomModalLesson] = useState<AllocatedLesson | null>(null);
  const [addModalInit, setAddModalInit] = useState<{ dia?: DayOfWeek; startMin?: number; endMin?: number } | null>(null);
  const [dropDecision, setDropDecision] = useState<{
    dragged: AllocatedLesson;
    dia: DayOfWeek;
    startMin: number;
    endMin: number;
    occupying: AllocatedLesson[];
    conflicts: SlotConflictInfo[];
  } | null>(null);
  const [contextMenu, setContextMenu] = useState<{
    x: number;
    y: number;
    lesson?: AllocatedLesson;
    dia?: DayOfWeek;
    startMin?: number;
    endMin?: number;
  } | null>(null);
  const [showConflictsMenu, setShowConflictsMenu] = useState(false);

  const targetOptions = useMemo(() => {
    if (viewType === "TURMA" || viewType === "GERAL") {
      return Object.values(snapshot.turmas).map((t) => ({ id: t.code, label: `${t.nome} (${t.code})` }));
    }
    if (viewType === "PROFESSOR") {
      return Object.values(snapshot.professores).map((p) => ({ id: p.code, label: `${p.nome} (${p.code})` }));
    }
    return Object.values(snapshot.salas).map((r) => ({ id: r.code, label: `${r.nome} (${r.code})` }));
  }, [snapshot, viewType]);

  useEffect(() => {
    if (viewType !== "GERAL" && !targetId && targetOptions.length > 0) {
      setTargetId(targetOptions[0].id);
    }
  }, [setTargetId, targetId, targetOptions, viewType]);

  const filteredAlocs = useMemo(() => {
    const all = timetable?.alocacoes || [];
    return all.filter((a) => {
      if (semesterFilter === 1 && a.regime !== "ANUAL" && a.semestre !== "1" && a.semestre_alocado !== 1) return false;
      if (semesterFilter === 2 && a.regime !== "ANUAL" && a.semestre !== "2" && a.semestre_alocado !== 2) return false;
      if (viewType === "TURMA" && targetId) return a.turmas.includes(targetId);
      if (viewType === "PROFESSOR" && targetId) return a.professores.includes(targetId);
      if (viewType === "SALA" && targetId) return a.salas.includes(targetId);
      if (viewType === "GERAL" && targetId) return a.turmas.includes(targetId);
      return true;
    });
  }, [semesterFilter, targetId, timetable?.alocacoes, viewType]);

  const { rows, inactiveDays } = useMemo(() => {
    const intervalMap = new Map<string, { startMin: number; endMin: number; isBreak: boolean }>();
    const inactive = new Set<DayOfWeek>();

    for (const a of filteredAlocs) {
      intervalMap.set(`${a.start_min}:${a.end_min}`, { startMin: a.start_min, endMin: a.end_min, isBreak: false });
    }

    if (viewType === "TURMA" && targetId && snapshot.turmas[targetId]) {
      const turma = snapshot.turmas[targetId];
      for (const d of ["SEG", "TER", "QUA", "QUI", "SEX"] as DayOfWeek[]) {
        if (!turma.dias_letivos.includes(d)) inactive.add(d);
        const slots = turma.excecoes_por_dia?.[d]?.slots || turma.jornada_padrao?.slots || [];
        for (const s of slots) {
          const sm = timeToMin(s.inicio);
          const em = timeToMin(s.fim);
          intervalMap.set(`${sm}:${em}`, { startMin: sm, endMin: em, isBreak: false });
        }
        for (const br of computeImplicitBreaks(slots)) {
          const key = `${br.start_min}:${br.end_min}`;
          if (!intervalMap.has(key)) {
            intervalMap.set(key, { startMin: br.start_min, endMin: br.end_min, isBreak: true });
          }
        }
      }
    }

    const sorted = Array.from(intervalMap.values()).sort((a, b) => a.startMin - b.startMin || a.endMin - b.endMin);
    return { rows: sorted, inactiveDays: inactive };
  }, [filteredAlocs, snapshot.turmas, targetId, viewType]);

  const conflictsMap = timetable?.conflitos || {};
  const conflictEntries = Object.entries(conflictsMap);
  const selectedLesson = (timetable?.alocacoes || []).find((a) => a.aula_id === selectedAulaId) || null;

  return (
    <div className="space-y-3" onClick={() => setContextMenu(null)}>
      {/* Filter & Action Toolbar */}
      <div className="flex flex-wrap items-center gap-2.5 rounded-lg border border-slate-200 bg-white p-3 dark:border-slate-800 dark:bg-slate-900">
        <select
          value={viewType}
          onChange={(e) => setViewType(e.target.value as ViewPerspective)}
          className="rounded-md border border-slate-300 bg-white px-2.5 py-1.5 text-sm font-medium dark:border-slate-700 dark:bg-slate-800"
        >
          <option value="TURMA">Visão por Turma</option>
          <option value="PROFESSOR">Visão por Professor</option>
          <option value="SALA">Visão por Sala</option>
          <option value="GERAL">Visão Geral (Todas)</option>
        </select>

        <select
          value={targetId}
          onChange={(e) => setTargetId(e.target.value)}
          className="min-w-[200px] rounded-md border border-slate-300 bg-white px-2.5 py-1.5 text-sm dark:border-slate-700 dark:bg-slate-800"
        >
          {viewType === "GERAL" && <option value="">Todas as Turmas</option>}
          {targetOptions.map((opt) => (
            <option key={opt.id} value={opt.id}>{opt.label}</option>
          ))}
        </select>

        <select
          value={semesterFilter}
          onChange={(e) => setSemesterFilter(Number(e.target.value) as 0 | 1 | 2)}
          className="rounded-md border border-slate-300 bg-white px-2.5 py-1.5 text-sm dark:border-slate-700 dark:bg-slate-800"
        >
          <option value={0}>Ambos os Semestres</option>
          <option value={1}>1º Semestre (Anuais + 1º)</option>
          <option value={2}>2º Semestre (Anuais + 2º)</option>
        </select>

        <div className="relative flex-1 min-w-[180px]">
          <Search className="pointer-events-none absolute left-2.5 top-2 h-4 w-4 text-slate-400" />
          <input
            type="text"
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
            placeholder="Destacar disciplina, professor ou sala..."
            className="w-full rounded-md border border-slate-300 bg-slate-50 py-1.5 pl-8 pr-3 text-sm dark:border-slate-700 dark:bg-slate-800"
          />
        </div>

        <div className="flex flex-wrap items-center gap-1.5">
          {conflictEntries.length > 0 && (
            <div className="relative">
              <button
                type="button"
                onClick={() => setShowConflictsMenu((s) => !s)}
                className="inline-flex items-center gap-1 rounded-full bg-rose-100 px-3 py-1 text-xs font-semibold text-rose-700 dark:bg-rose-950 dark:text-rose-300"
              >
                <AlertTriangle className="h-3.5 w-3.5" />
                {conflictEntries.length} conflitos
              </button>
              {showConflictsMenu && (
                <div className="absolute right-0 z-40 mt-1 max-h-64 w-80 overflow-y-auto rounded-lg border border-slate-200 bg-white p-2 shadow-xl dark:border-slate-700 dark:bg-slate-900">
                  {conflictEntries.map(([aid, items]) => (
                    <button
                      key={aid}
                      onClick={() => {
                        setSelectedAulaId(aid);
                        setShowConflictsMenu(false);
                      }}
                      className="block w-full rounded p-1.5 text-left text-xs hover:bg-slate-100 dark:hover:bg-slate-800"
                    >
                      <div className="font-semibold text-rose-600">{aid}</div>
                      <div className="text-slate-600 dark:text-slate-400">{items[0].detail}</div>
                    </button>
                  ))}
                </div>
              )}
            </div>
          )}

          <button
            type="button"
            onClick={toggleTransposeGrid}
            className="inline-flex items-center gap-1 rounded-md border border-slate-300 px-2.5 py-1.5 text-xs font-medium hover:bg-slate-100 dark:border-slate-700 dark:hover:bg-slate-800"
          >
            <ArrowLeftRight className="h-3.5 w-3.5" />
            {transposeGrid ? "Horários × Dias" : "Dias × Horários"}
          </button>

          <button
            type="button"
            disabled={!selectedLesson}
            onClick={() => selectedLesson && setSwapModalLesson(selectedLesson)}
            className="inline-flex items-center gap-1 rounded-md border border-slate-300 px-2.5 py-1.5 text-xs font-medium hover:bg-slate-100 disabled:opacity-40 dark:border-slate-700 dark:hover:bg-slate-800"
          >
            Trocar (Swap)
          </button>

          <button
            type="button"
            onClick={() => setAddModalInit({})}
            className="inline-flex items-center gap-1 rounded-md bg-emerald-600 px-2.5 py-1.5 text-xs font-medium text-white hover:bg-emerald-700"
          >
            <Plus className="h-3.5 w-3.5" /> Alocar
          </button>

          <button
            type="button"
            onClick={() => mutations.pinAll.mutate(true)}
            title="Fixar todas as aulas"
            className="rounded-md border border-slate-300 p-1.5 hover:bg-slate-100 dark:border-slate-700 dark:hover:bg-slate-800"
          >
            <Lock className="h-3.5 w-3.5" />
          </button>
          <button
            type="button"
            onClick={() => mutations.pinAll.mutate(false)}
            title="Desafixar todas as aulas"
            className="rounded-md border border-slate-300 p-1.5 hover:bg-slate-100 dark:border-slate-700 dark:hover:bg-slate-800"
          >
            <Unlock className="h-3.5 w-3.5" />
          </button>
          <button
            type="button"
            onClick={() => {
              if (window.confirm("Limpar toda a grade horária atual?")) {
                mutations.clearTimetable.mutate();
              }
            }}
            title="Limpar Grade"
            className="rounded-md border border-rose-300 p-1.5 text-rose-600 hover:bg-rose-50 dark:border-rose-900 dark:hover:bg-rose-950"
          >
            <Trash2 className="h-3.5 w-3.5" />
          </button>
        </div>
      </div>

      {selectedLesson && (
        <div className="flex items-center justify-between rounded-md border border-amber-300 bg-amber-50 px-3 py-1.5 text-xs text-amber-900 dark:border-amber-800 dark:bg-amber-950/40 dark:text-amber-200">
          <span>
            <strong>Selecionada:</strong> {selectedLesson.disciplina_nome} ({selectedLesson.aula_id}) — {selectedLesson.dia} {selectedLesson.inicio}-{selectedLesson.fim}
          </span>
          <button onClick={() => setSelectedAulaId(null)} className="font-semibold underline">Limpar seleção</button>
        </div>
      )}

      <TimetableGrid
        snapshot={snapshot}
        alocacoes={filteredAlocs}
        conflitos={conflictsMap}
        rows={rows}
        inactiveDays={inactiveDays}
        transposeGrid={transposeGrid}
        searchQuery={searchQuery}
        selectedAulaId={selectedAulaId}
        onSelectLesson={setSelectedAulaId}
        onDoubleClickLesson={(lesson) => setSwapModalLesson(lesson)}
        onContextMenuLesson={(e, lesson) => setContextMenu({ x: e.clientX, y: e.clientY, lesson })}
        onContextMenuEmpty={(e, dia, startMin, endMin) => setContextMenu({ x: e.clientX, y: e.clientY, dia, startMin, endMin })}
        onDropLesson={(dragged, dia, startMin, endMin, occupying, conflicts) => {
          if (occupying.length === 0 && conflicts.length === 0) {
            mutations.moveLesson.mutate({
              aula_id: dragged.aula_id,
              dia,
              inicio: minToTime(startMin),
              fim: minToTime(endMin),
            });
          } else {
            setDropDecision({ dragged, dia, startMin, endMin, occupying, conflicts });
          }
        }}
      />

      {/* Context Menu */}
      {contextMenu && (
        <div
          style={{ top: contextMenu.y, left: contextMenu.x }}
          className="fixed z-50 w-56 rounded-lg border border-slate-200 bg-white py-1 text-xs shadow-xl dark:border-slate-700 dark:bg-slate-900"
          onClick={(e) => e.stopPropagation()}
        >
          {contextMenu.lesson ? (
            <>
              <button onClick={() => { setSwapModalLesson(contextMenu.lesson!); setContextMenu(null); }} className="block w-full px-3 py-1.5 text-left hover:bg-slate-100 dark:hover:bg-slate-800">🔄 Trocar Horário (Swap)...</button>
              <button onClick={() => { setMoveModalLesson(contextMenu.lesson!); setContextMenu(null); }} className="block w-full px-3 py-1.5 text-left hover:bg-slate-100 dark:hover:bg-slate-800">↔️ Mover Horário / Sala...</button>
              <button onClick={() => { setRoomModalLesson(contextMenu.lesson!); setContextMenu(null); }} className="block w-full px-3 py-1.5 text-left hover:bg-slate-100 dark:hover:bg-slate-800">🏢 Alterar Sala...</button>
              <button onClick={() => { mutations.pinLesson.mutate({ aula_id: contextMenu.lesson!.aula_id, pinned: !contextMenu.lesson!.is_pinned }); setContextMenu(null); }} className="block w-full px-3 py-1.5 text-left hover:bg-slate-100 dark:hover:bg-slate-800">
                {contextMenu.lesson.is_pinned ? "🔓 Desafixar Horário" : "📌 Fixar Horário (Pin)"}
              </button>
              <button onClick={() => { mutations.removeLesson.mutate(contextMenu.lesson!.aula_id); setContextMenu(null); }} className="block w-full px-3 py-1.5 text-left text-rose-600 hover:bg-rose-50 dark:hover:bg-rose-950">❌ Desalocar Aula</button>
            </>
          ) : (
            <button onClick={() => { setAddModalInit({ dia: contextMenu.dia, startMin: contextMenu.startMin, endMin: contextMenu.endMin }); setContextMenu(null); }} className="block w-full px-3 py-1.5 text-left hover:bg-slate-100 dark:hover:bg-slate-800">
              ➕ Alocar Disciplina Neste Horário...
            </button>
          )}
        </div>
      )}

      {moveModalLesson && (
        <MoveLessonModal
          lesson={moveModalLesson}
          snapshot={snapshot}
          onClose={() => setMoveModalLesson(null)}
          onConfirm={(dia, inicio, fim, salas, pin) => {
            mutations.moveLesson.mutate({ aula_id: moveModalLesson.aula_id, dia, inicio, fim, salas, pin_after_move: pin });
            setMoveModalLesson(null);
          }}
        />
      )}

      {swapModalLesson && (
        <SwapLessonModal
          sourceLesson={swapModalLesson}
          snapshot={snapshot}
          onClose={() => setSwapModalLesson(null)}
          onConfirm={(targetAulaId, pinBoth) => {
            mutations.swapLessons.mutate({ aula_a_id: swapModalLesson.aula_id, aula_b_id: targetAulaId, pin_both: pinBoth });
            setSwapModalLesson(null);
          }}
        />
      )}

      {roomModalLesson && (
        <ChangeRoomModal
          lesson={roomModalLesson}
          snapshot={snapshot}
          onClose={() => setRoomModalLesson(null)}
          onConfirm={(salas, swapWith, pinAfter) => {
            mutations.changeRoom.mutate({ aula_id: roomModalLesson.aula_id, salas, swap_with_aula_id: swapWith, pin_after: pinAfter });
            setRoomModalLesson(null);
          }}
        />
      )}

      {addModalInit && (
        <AddLessonManualModal
          unallocated={unallocatedData?.unallocated || []}
          snapshot={snapshot}
          initialDia={addModalInit.dia}
          initialStartMin={addModalInit.startMin}
          initialEndMin={addModalInit.endMin}
          onClose={() => setAddModalInit(null)}
          onConfirm={(payload) => {
            mutations.allocateManual.mutate(payload);
            setAddModalInit(null);
          }}
        />
      )}

      {dropDecision && (
        <DropDecisionModal
          dragged={dropDecision.dragged}
          targetDia={dropDecision.dia}
          targetStartMin={dropDecision.startMin}
          targetEndMin={dropDecision.endMin}
          occupying={dropDecision.occupying}
          conflicts={dropDecision.conflicts}
          snapshot={snapshot}
          onClose={() => setDropDecision(null)}
          onMove={() => {
            mutations.moveLesson.mutate({
              aula_id: dropDecision.dragged.aula_id,
              dia: dropDecision.dia,
              inicio: minToTime(dropDecision.startMin),
              fim: minToTime(dropDecision.endMin),
            });
            setDropDecision(null);
          }}
          onSwap={(targetAulaId) => {
            mutations.swapLessons.mutate({
              aula_a_id: dropDecision.dragged.aula_id,
              aula_b_id: targetAulaId,
            });
            setDropDecision(null);
          }}
        />
      )}
    </div>
  );
}
