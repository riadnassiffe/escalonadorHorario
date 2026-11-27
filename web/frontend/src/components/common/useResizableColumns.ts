import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import type { KeyboardEvent, MouseEvent, TouchEvent } from "react";

export interface ResizableColumnConfig<K extends string> {
  key: K;
  label: string;
  defaultWidth: number;
  minWidth: number;
  maxWidth?: number;
}

interface UseResizableColumnsOptions<K extends string> {
  columns: readonly ResizableColumnConfig<K>[];
  persistedWidths?: Record<string, number>;
  onPersistWidths: (widths: Record<string, number>) => void;
}

const DEFAULT_MAX_WIDTH = 720;

function clampWidth<K extends string>(value: number, col: ResizableColumnConfig<K>): number {
  const max = col.maxWidth ?? DEFAULT_MAX_WIDTH;
  return Math.min(max, Math.max(col.minWidth, Math.round(value)));
}

/**
 * Gerencia larguras interativas de colunas em tabelas `table-fixed` com `<colgroup>`,
 * suportando arrasto por mouse/toque, ajuste por teclado e restauração individual ou global.
 */
export function useResizableColumns<K extends string>({
  columns,
  persistedWidths,
  onPersistWidths,
}: UseResizableColumnsOptions<K>) {
  const [resizingKey, setResizingKey] = useState<K | null>(null);
  const headerRefs = useRef<Partial<Record<K, HTMLTableCellElement | null>>>({});
  const cleanupRef = useRef<(() => void) | null>(null);

  const columnsByKey = useMemo(
    () => Object.fromEntries(columns.map((col) => [col.key, col])) as Record<K, ResizableColumnConfig<K>>,
    [columns]
  );

  const widths = useMemo(() => {
    const resolved = {} as Record<K, number>;
    for (const col of columns) {
      const raw = persistedWidths?.[col.key];
      resolved[col.key] =
        typeof raw === "number" && Number.isFinite(raw) ? clampWidth(raw, col) : col.defaultWidth;
    }
    return resolved;
  }, [columns, persistedWidths]);

  const totalWidth = useMemo(
    () => columns.reduce((sum, col) => sum + widths[col.key], 0),
    [columns, widths]
  );

  const hasCustomWidths = useMemo(
    () => columns.some((col) => widths[col.key] !== col.defaultWidth),
    [columns, widths]
  );

  useEffect(() => {
    return () => {
      cleanupRef.current?.();
      cleanupRef.current = null;
    };
  }, []);

  const persistIfCustomized = useCallback(
    (nextWidths: Record<K, number>) => {
      const allDefault = columns.every((col) => nextWidths[col.key] === col.defaultWidth);
      onPersistWidths(allDefault ? {} : nextWidths);
    },
    [columns, onPersistWidths]
  );

  const registerHeaderRef = useCallback(
    (key: K) => (el: HTMLTableCellElement | null) => {
      headerRefs.current[key] = el;
    },
    []
  );

  const startResize = useCallback(
    (key: K, startX: number) => {
      cleanupRef.current?.();
      cleanupRef.current = null;

      // Em telas largas (minWidth: 100%), captura a largura real renderizada para evitar salto no início do arrasto.
      const measuredWidths = columns.map((col) => headerRefs.current[col.key]?.getBoundingClientRect().width ?? 0);
      const measuredSum = measuredWidths.reduce((sum, w) => sum + w, 0);
      const useMeasured = measuredWidths.every((w) => w > 0) && measuredSum > totalWidth + 1;

      const baseWidths = {} as Record<K, number>;
      columns.forEach((col, index) => {
        baseWidths[col.key] = useMeasured ? clampWidth(measuredWidths[index], col) : widths[col.key];
      });

      const col = columnsByKey[key];
      const startWidth = baseWidths[key];
      const prevCursor = document.body.style.cursor;
      const prevUserSelect = document.body.style.userSelect;

      document.body.style.cursor = "col-resize";
      document.body.style.userSelect = "none";
      setResizingKey(key);

      const applyDelta = (clientX: number) => {
        const nextWidth = clampWidth(startWidth + (clientX - startX), col);
        onPersistWidths({ ...baseWidths, [key]: nextWidth });
      };

      const onMouseMove = (event: globalThis.MouseEvent) => {
        applyDelta(event.clientX);
      };

      const onTouchMove = (event: globalThis.TouchEvent) => {
        if (event.touches[0]) {
          applyDelta(event.touches[0].clientX);
        }
      };

      const finish = () => {
        cleanup();
        setResizingKey(null);
      };

      const cleanup = () => {
        window.removeEventListener("mousemove", onMouseMove);
        window.removeEventListener("mouseup", finish);
        window.removeEventListener("touchmove", onTouchMove);
        window.removeEventListener("touchend", finish);
        window.removeEventListener("touchcancel", finish);
        document.body.style.cursor = prevCursor;
        document.body.style.userSelect = prevUserSelect;
        if (cleanupRef.current === cleanup) {
          cleanupRef.current = null;
        }
      };

      cleanupRef.current = cleanup;
      window.addEventListener("mousemove", onMouseMove);
      window.addEventListener("mouseup", finish);
      window.addEventListener("touchmove", onTouchMove);
      window.addEventListener("touchend", finish);
      window.addEventListener("touchcancel", finish);
    },
    [columns, columnsByKey, onPersistWidths, totalWidth, widths]
  );

  const resetColumn = useCallback(
    (key: K) => {
      const nextWidths = { ...widths, [key]: columnsByKey[key].defaultWidth };
      persistIfCustomized(nextWidths);
    },
    [columnsByKey, persistIfCustomized, widths]
  );

  const resetAll = useCallback(() => {
    onPersistWidths({});
  }, [onPersistWidths]);

  const adjustColumn = useCallback(
    (key: K, delta: number) => {
      const nextWidth = clampWidth(widths[key] + delta, columnsByKey[key]);
      persistIfCustomized({ ...widths, [key]: nextWidth });
    },
    [columnsByKey, persistIfCustomized, widths]
  );

  const getResizeHandleProps = useCallback(
    (key: K) => {
      const col = columnsByKey[key];
      return {
        role: "separator" as const,
        "aria-orientation": "vertical" as const,
        "aria-label": `Redimensionar coluna ${col.label}`,
        "aria-valuemin": col.minWidth,
        "aria-valuemax": col.maxWidth ?? DEFAULT_MAX_WIDTH,
        "aria-valuenow": widths[key],
        tabIndex: 0,
        title: "Arraste para redimensionar (duplo clique para restaurar)",
        onMouseDown: (e: MouseEvent<HTMLDivElement>) => {
          e.preventDefault();
          e.stopPropagation();
          startResize(key, e.clientX);
        },
        onTouchStart: (e: TouchEvent<HTMLDivElement>) => {
          if (e.touches[0]) {
            e.stopPropagation();
            startResize(key, e.touches[0].clientX);
          }
        },
        onDoubleClick: (e: MouseEvent<HTMLDivElement>) => {
          e.preventDefault();
          e.stopPropagation();
          resetColumn(key);
        },
        onKeyDown: (e: KeyboardEvent<HTMLDivElement>) => {
          const step = e.shiftKey ? 32 : 16;
          if (e.key === "ArrowLeft") {
            e.preventDefault();
            adjustColumn(key, -step);
          } else if (e.key === "ArrowRight") {
            e.preventDefault();
            adjustColumn(key, step);
          } else if (e.key === "Home") {
            e.preventDefault();
            resetColumn(key);
          }
        },
      };
    },
    [adjustColumn, columnsByKey, resetColumn, startResize, widths]
  );

  return {
    widths,
    totalWidth,
    hasCustomWidths,
    resizingKey,
    registerHeaderRef,
    getResizeHandleProps,
    resetColumn,
    resetAll,
  };
}
