import { useDraggable } from "@dnd-kit/core";
import { CSS } from "@dnd-kit/utilities";
import { AlertTriangle, Pin } from "lucide-react";
import type { AllocatedLesson, SlotConflictInfo } from "../../types";

interface Props {
  lesson: AllocatedLesson;
  profNames: string;
  turmaNames: string;
  roomNames: string;
  conflicts?: SlotConflictInfo[];
  isSelected?: boolean;
  isSearchMatch?: boolean;
  dimmed?: boolean;
  onSelect: () => void;
  onDoubleClick: () => void;
  onContextMenu: (e: React.MouseEvent) => void;
}

export function DraggableLessonCard({
  lesson,
  profNames,
  turmaNames,
  roomNames,
  conflicts = [],
  isSelected = false,
  isSearchMatch = false,
  dimmed = false,
  onSelect,
  onDoubleClick,
  onContextMenu,
}: Props) {
  const { attributes, listeners, setNodeRef, transform, isDragging } = useDraggable({
    id: `lesson:${lesson.aula_id}`,
    data: { lesson },
  });

  const style = {
    transform: CSS.Translate.toString(transform),
  };

  const borderColor =
    lesson.tipo === "LABORATORIO"
      ? "border-l-pink-600 bg-pink-50/90 dark:bg-pink-950/40"
      : lesson.tipo === "ESPECIAL"
      ? "border-l-violet-600 bg-violet-50/90 dark:bg-violet-950/40"
      : "border-l-blue-600 bg-blue-50/90 dark:bg-blue-950/40";

  const regimeBadge =
    lesson.regime === "ANUAL"
      ? "Anual"
      : lesson.semestre === "2" || lesson.semestre_alocado === 2
      ? "2º Sem"
      : "1º Sem";

  return (
    <div
      ref={setNodeRef}
      style={style}
      {...listeners}
      {...attributes}
      onClick={(e) => {
        e.stopPropagation();
        onSelect();
      }}
      onDoubleClick={(e) => {
        e.stopPropagation();
        onDoubleClick();
      }}
      onContextMenu={(e) => {
        e.preventDefault();
        e.stopPropagation();
        onSelect();
        onContextMenu(e);
      }}
      className={`group relative cursor-grab select-none rounded-md border border-slate-200 border-l-4 p-1.5 text-xs shadow-xs transition active:cursor-grabbing dark:border-slate-700 ${borderColor} ${
        isDragging ? "opacity-30" : ""
      } ${isSelected ? "ring-2 ring-amber-500 dark:ring-amber-400" : ""} ${
        isSearchMatch ? "ring-2 ring-cyan-500" : ""
      } ${dimmed ? "opacity-35" : ""}`}
    >
      <div className="flex items-start justify-between gap-1">
        <span className="font-semibold leading-tight text-slate-900 dark:text-slate-100">
          {lesson.disciplina_nome}
        </span>
        <div className="flex shrink-0 items-center gap-1">
          {lesson.is_pinned && <Pin className="h-3 w-3 text-amber-600 dark:text-amber-400" />}
          {conflicts.length > 0 && (
            <span title={conflicts.map((c) => c.detail).join("\n")}>
              <AlertTriangle className="h-3.5 w-3.5 text-rose-600" />
            </span>
          )}
        </div>
      </div>

      <div className="mt-0.5 text-[11px] text-slate-600 dark:text-slate-300 truncate">
        {turmaNames}
      </div>
      <div className="text-[11px] text-slate-500 dark:text-slate-400 truncate">
        {profNames} • {roomNames || "Sem sala"}
      </div>

      <div className="mt-1 flex items-center justify-between gap-1">
        <span className="rounded bg-slate-200/80 px-1.5 py-0.2 text-[10px] font-medium text-slate-700 dark:bg-slate-800 dark:text-slate-300">
          {regimeBadge}
        </span>
        <span className="text-[10px] text-slate-400">
          {lesson.end_min - lesson.start_min}m
        </span>
      </div>

      {conflicts.length > 0 && (
        <div className="mt-1 rounded bg-rose-100 px-1 py-0.5 text-[10px] font-medium text-rose-700 dark:bg-rose-950 dark:text-rose-300 truncate">
          {conflicts[0].short_label}
        </div>
      )}
    </div>
  );
}
