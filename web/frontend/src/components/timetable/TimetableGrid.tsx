import {
  DndContext,
  DragOverlay,
  PointerSensor,
  useSensor,
  useSensors,
  type DragEndEvent,
  type DragStartEvent,
} from "@dnd-kit/core";
import { useMemo, useState } from "react";
import { ALL_DAYS, DAY_LABELS, formatInterval, minToTime } from "../../domain/time";
import { evaluateLessonAtSlot, evaluateLessonSwap, type DomainSnapshot } from "../../domain/conflictEngine";
import { normalizeText } from "../../domain/searchSort";
import type { AllocatedLesson, DayOfWeek, SlotConflictInfo } from "../../types";
import { DraggableLessonCard } from "./DraggableLessonCard";
import { DroppableSlotCell } from "./DroppableSlotCell";

interface TimeRow {
  startMin: number;
  endMin: number;
  isBreak: boolean;
}

interface Props {
  snapshot: DomainSnapshot;
  alocacoes: AllocatedLesson[];
  conflitos: Record<string, SlotConflictInfo[]>;
  rows: TimeRow[];
  inactiveDays?: Set<DayOfWeek>;
  transposeGrid: boolean;
  searchQuery: string;
  selectedAulaId: string | null;
  onSelectLesson: (aulaId: string | null) => void;
  onDoubleClickLesson: (lesson: AllocatedLesson) => void;
  onContextMenuLesson: (e: React.MouseEvent, lesson: AllocatedLesson) => void;
  onContextMenuEmpty: (e: React.MouseEvent, dia: DayOfWeek, startMin: number, endMin: number) => void;
  onDropLesson: (
    lesson: AllocatedLesson,
    targetDia: DayOfWeek,
    targetStartMin: number,
    targetEndMin: number,
    occupyingLessons: AllocatedLesson[],
    previewConflicts: SlotConflictInfo[]
  ) => void;
}

export function TimetableGrid({
  snapshot,
  alocacoes,
  conflitos,
  rows,
  inactiveDays = new Set(),
  transposeGrid,
  searchQuery,
  selectedAulaId,
  onSelectLesson,
  onDoubleClickLesson,
  onContextMenuLesson,
  onContextMenuEmpty,
  onDropLesson,
}: Props) {
  const [activeLesson, setActiveLesson] = useState<AllocatedLesson | null>(null);

  const sensors = useSensors(
    useSensor(PointerSensor, {
      activationConstraint: { distance: 6 },
    })
  );

  const normalizedQuery = normalizeText(searchQuery.trim());

  const cellLessonsMap = useMemo(() => {
    const map = new Map<string, AllocatedLesson[]>();
    for (const a of alocacoes) {
      const key = `${a.dia}:${a.start_min}:${a.end_min}`;
      const list = map.get(key) || [];
      list.push(a);
      map.set(key, list);
    }
    return map;
  }, [alocacoes]);

  const swappableAulaIds = useMemo(() => {
    const eligible = new Set<string>();
    if (!selectedAulaId) return eligible;
    if (!snapshot.alocacoes.some((a) => a.aula_id === selectedAulaId)) return eligible;
    for (const lesson of alocacoes) {
      if (lesson.aula_id === selectedAulaId) continue;
      if (evaluateLessonSwap(snapshot, selectedAulaId, lesson.aula_id).length === 0) {
        eligible.add(lesson.aula_id);
      }
    }
    return eligible;
  }, [alocacoes, selectedAulaId, snapshot]);

  const handleDragStart = (event: DragStartEvent) => {
    const lesson = event.active.data.current?.lesson as AllocatedLesson | undefined;
    if (lesson) {
      setActiveLesson(lesson);
      onSelectLesson(lesson.aula_id);
    }
  };

  const handleDragEnd = (event: DragEndEvent) => {
    const dragged = activeLesson;
    setActiveLesson(null);
    if (!dragged || !event.over) return;

    const data = event.over.data.current as {
      dia: DayOfWeek;
      startMin: number;
      endMin: number;
    };
    if (!data) return;
    if (dragged.dia === data.dia && dragged.start_min === data.startMin && dragged.end_min === data.endMin) {
      return;
    }

    const dur = dragged.end_min - dragged.start_min;
    const targetEnd = data.endMin || data.startMin + dur;
    const occupying = cellLessonsMap.get(`${data.dia}:${data.startMin}:${data.endMin}`) || [];
    const conflicts = evaluateLessonAtSlot(snapshot, dragged.aula_id, data.dia, data.startMin, targetEnd);
    onDropLesson(dragged, data.dia, data.startMin, targetEnd, occupying, conflicts);
  };

  const renderCell = (dia: DayOfWeek, row: TimeRow) => {
    const key = `${dia}:${row.startMin}:${row.endMin}`;
    const lessons = cellLessonsMap.get(key) || [];
    const isInactive = inactiveDays.has(dia);
    const previewConflicts =
      activeLesson && !row.isBreak && !isInactive
        ? evaluateLessonAtSlot(snapshot, activeLesson.aula_id, dia, row.startMin, row.endMin)
        : [];

    return (
      <DroppableSlotCell
        key={key}
        id={`slot:${key}`}
        dia={dia}
        startMin={row.startMin}
        endMin={row.endMin}
        isBreak={row.isBreak}
        isInactiveDay={isInactive}
        hasLessons={lessons.length > 0}
        isDraggingAny={Boolean(activeLesson)}
        previewConflicts={previewConflicts}
        onClick={() => onSelectLesson(null)}
        onContextMenu={(e) => {
          if (lessons.length === 0) {
            onContextMenuEmpty(e, dia, row.startMin, row.endMin);
          }
        }}
      >
        {lessons.map((lesson) => {
          const profNames = lesson.professores.map((id) => snapshot.professores[id]?.nome || id).join(", ");
          const turmaNames = lesson.turmas.map((id) => snapshot.turmas[id]?.nome || id).join(", ");
          const roomNames = lesson.salas.map((id) => snapshot.salas[id]?.nome || id).join(", ");
          const matchText = normalizeText(`${lesson.disciplina_nome} ${profNames} ${turmaNames} ${roomNames}`);
          const isMatch = Boolean(normalizedQuery) && matchText.includes(normalizedQuery);
          const dimmed = Boolean(normalizedQuery) && !isMatch;

          return (
            <DraggableLessonCard
              key={lesson.aula_id}
              lesson={lesson}
              profNames={profNames}
              turmaNames={turmaNames}
              roomNames={roomNames}
              conflicts={conflitos[lesson.aula_id] || []}
              isSelected={selectedAulaId === lesson.aula_id}
              canSwap={swappableAulaIds.has(lesson.aula_id)}
              isSearchMatch={isMatch}
              dimmed={dimmed}
              onSelect={() => onSelectLesson(lesson.aula_id)}
              onDoubleClick={() => onDoubleClickLesson(lesson)}
              onContextMenu={(e) => onContextMenuLesson(e, lesson)}
            />
          );
        })}
      </DroppableSlotCell>
    );
  };

  return (
    <DndContext sensors={sensors} onDragStart={handleDragStart} onDragEnd={handleDragEnd}>
      <div className="overflow-x-auto rounded-lg border border-slate-200 bg-white shadow-xs dark:border-slate-800 dark:bg-slate-900">
        {!transposeGrid ? (
          <table className="w-full border-collapse text-sm">
            <thead>
              <tr className="bg-slate-100 dark:bg-slate-800/80">
                <th className="w-28 border border-slate-200 p-2.5 text-center text-xs font-semibold uppercase text-slate-600 dark:border-slate-700 dark:text-slate-300">
                  Horário
                </th>
                {ALL_DAYS.map((d) => (
                  <th
                    key={d}
                    className="border border-slate-200 p-2.5 text-center text-xs font-semibold uppercase text-slate-700 dark:border-slate-700 dark:text-slate-200"
                  >
                    {DAY_LABELS[d]}
                  </th>
                ))}
              </tr>
            </thead>
            <tbody>
              {rows.length === 0 ? (
                <tr>
                  <td colSpan={6} className="p-8 text-center text-sm text-slate-500">
                    Nenhum horário ou aula alocada para os filtros selecionados.
                  </td>
                </tr>
              ) : (
                rows.map((row) => (
                  <tr key={`${row.startMin}-${row.endMin}`}>
                    <td className="border border-slate-200 bg-slate-50 p-2 text-center text-xs font-semibold whitespace-nowrap text-slate-700 dark:border-slate-800 dark:bg-slate-900/60 dark:text-slate-300">
                      {formatInterval(row.startMin, row.endMin)}
                    </td>
                    {ALL_DAYS.map((d) => renderCell(d, row))}
                  </tr>
                ))
              )}
            </tbody>
          </table>
        ) : (
          <table className="w-full border-collapse text-sm">
            <thead>
              <tr className="bg-slate-100 dark:bg-slate-800/80">
                <th className="w-32 border border-slate-200 p-2.5 text-center text-xs font-semibold uppercase text-slate-600 dark:border-slate-700 dark:text-slate-300">
                  Dia
                </th>
                {rows.map((row) => (
                  <th
                    key={`${row.startMin}-${row.endMin}`}
                    className="border border-slate-200 p-2 text-center text-xs font-semibold whitespace-nowrap text-slate-700 dark:border-slate-700 dark:text-slate-200"
                  >
                    {minToTime(row.startMin)}–{minToTime(row.endMin)}
                  </th>
                ))}
              </tr>
            </thead>
            <tbody>
              {ALL_DAYS.map((d) => (
                <tr key={d}>
                  <td className="border border-slate-200 bg-slate-50 p-2 text-center text-xs font-semibold text-slate-700 dark:border-slate-800 dark:bg-slate-900/60 dark:text-slate-300">
                    {DAY_LABELS[d]}
                  </td>
                  {rows.map((row) => renderCell(d, row))}
                </tr>
              ))}
            </tbody>
          </table>
        )}
      </div>

      <DragOverlay>
        {activeLesson ? (
          <div className="w-44 rounded-md border-2 border-blue-500 bg-white p-2 text-xs shadow-xl dark:bg-slate-900">
            <div className="font-bold text-slate-900 dark:text-slate-100">{activeLesson.disciplina_nome}</div>
            <div className="text-[11px] text-slate-500">
              {activeLesson.turmas.join(", ")} • {activeLesson.end_min - activeLesson.start_min}m
            </div>
          </div>
        ) : null}
      </DragOverlay>
    </DndContext>
  );
}
