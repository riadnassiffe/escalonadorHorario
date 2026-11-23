import { useDroppable } from "@dnd-kit/core";
import type { ReactNode } from "react";
import type { DayOfWeek, SlotConflictInfo } from "../../types";

interface Props {
  id: string;
  dia: DayOfWeek;
  startMin: number;
  endMin: number;
  isBreak?: boolean;
  isInactiveDay?: boolean;
  hasLessons: boolean;
  isDraggingAny: boolean;
  previewConflicts: SlotConflictInfo[];
  onContextMenu: (e: React.MouseEvent) => void;
  onClick: () => void;
  children?: ReactNode;
}

export function DroppableSlotCell({
  id,
  dia,
  startMin,
  endMin,
  isBreak = false,
  isInactiveDay = false,
  hasLessons,
  isDraggingAny,
  previewConflicts,
  onContextMenu,
  onClick,
  children,
}: Props) {
  const disabled = isBreak || isInactiveDay;
  const { setNodeRef, isOver } = useDroppable({
    id,
    disabled,
    data: { dia, startMin, endMin, hasLessons },
  });

  if (isBreak) {
    return (
      <td className="border border-slate-200 bg-amber-50/90 p-2 text-center text-xs font-semibold text-amber-800 dark:border-slate-800 dark:bg-amber-950/30 dark:text-amber-300">
        ☕ INTERVALO
      </td>
    );
  }

  let highlightClass = "bg-white dark:bg-slate-900";
  if (isInactiveDay) {
    highlightClass = "bg-slate-100 text-slate-400 dark:bg-slate-950";
  } else if (isDraggingAny) {
    if (previewConflicts.length === 0) {
      highlightClass = isOver
        ? "bg-emerald-100 ring-2 ring-emerald-500 dark:bg-emerald-950/60"
        : hasLessons
        ? "bg-blue-50/60 dark:bg-blue-950/20"
        : "bg-emerald-50/40 dark:bg-emerald-950/10";
    } else if (hasLessons) {
      highlightClass = isOver
        ? "bg-amber-100 ring-2 ring-amber-500 dark:bg-amber-950/50"
        : "bg-amber-50/40 dark:bg-amber-950/10";
    } else {
      highlightClass = isOver
        ? "bg-rose-100 ring-2 ring-rose-500 dark:bg-rose-950/50"
        : "bg-rose-50/30 dark:bg-rose-950/10";
    }
  }

  return (
    <td
      ref={setNodeRef}
      onClick={onClick}
      onContextMenu={(e) => {
        e.preventDefault();
        onContextMenu(e);
      }}
      className={`min-w-[160px] border border-slate-200 p-1.5 align-top transition-colors dark:border-slate-800 ${highlightClass}`}
    >
      <div className="flex flex-col gap-1.5 min-h-[54px]">
        {children}
        {isDraggingAny && previewConflicts.length > 0 && isOver && (
          <div className="rounded bg-rose-600/90 px-1.5 py-0.5 text-[10px] font-medium text-white">
            {previewConflicts[0].short_label}
          </div>
        )}
      </div>
    </td>
  );
}
