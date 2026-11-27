import type { ReactNode } from "react";
import { ArrowDownAZ, ArrowUpZA, Search, X } from "lucide-react";

interface Props {
  searchQuery: string;
  onSearchChange: (q: string) => void;
  totalCount: number;
  filteredCount: number;
  entityLabel: string;
  sortOptions: Array<{ value: string; label: string }>;
  sortBy: string;
  onSortByChange: (v: string) => void;
  ascending: boolean;
  onToggleOrder: () => void;
  extraFilters?: ReactNode;
  hasActiveFilter?: boolean;
}

export function SearchSortTableBar({
  searchQuery,
  onSearchChange,
  totalCount,
  filteredCount,
  entityLabel,
  sortOptions,
  sortBy,
  onSortByChange,
  ascending,
  onToggleOrder,
  extraFilters,
  hasActiveFilter,
}: Props) {
  const isFiltered = hasActiveFilter ?? Boolean(searchQuery);

  return (
    <div className="flex flex-wrap items-center gap-3 rounded-lg border border-slate-200 bg-white p-3 dark:border-slate-700 dark:bg-slate-900">
      <div className="relative flex-1 min-w-[220px]">
        <Search className="pointer-events-none absolute left-3 top-2.5 h-4 w-4 text-slate-400" />
        <input
          type="text"
          value={searchQuery}
          onChange={(e) => onSearchChange(e.target.value)}
          placeholder={`Buscar ${entityLabel} (ignora acentos)...`}
          className="w-full rounded-md border border-slate-300 bg-slate-50 py-1.5 pl-9 pr-8 text-sm outline-none focus:border-blue-500 dark:border-slate-700 dark:bg-slate-800"
        />
        {searchQuery && (
          <button
            type="button"
            onClick={() => onSearchChange("")}
            className="absolute right-2 top-2 text-slate-400 hover:text-slate-600"
          >
            <X className="h-4 w-4" />
          </button>
        )}
      </div>

      {extraFilters}

      <span className="rounded-full bg-slate-100 px-3 py-1 text-xs font-medium text-slate-700 dark:bg-slate-800 dark:text-slate-300">
        {isFiltered ? `${filteredCount} de ${totalCount} encontrados` : `${totalCount} ${entityLabel}`}
      </span>

      <div className="flex items-center gap-2">
        <span className="text-xs text-slate-500">Ordenar:</span>
        <select
          value={sortBy}
          onChange={(e) => onSortByChange(e.target.value)}
          className="rounded-md border border-slate-300 bg-white px-2.5 py-1.5 text-sm dark:border-slate-700 dark:bg-slate-800"
        >
          {sortOptions.map((opt) => (
            <option key={opt.value} value={opt.value}>
              {opt.label}
            </option>
          ))}
        </select>
        <button
          type="button"
          onClick={onToggleOrder}
          className="inline-flex items-center gap-1 rounded-md border border-slate-300 px-2.5 py-1.5 text-xs font-medium hover:bg-slate-100 dark:border-slate-700 dark:hover:bg-slate-800"
        >
          {ascending ? <ArrowDownAZ className="h-4 w-4" /> : <ArrowUpZA className="h-4 w-4" />}
          {ascending ? "A-Z" : "Z-A"}
        </button>
      </div>
    </div>
  );
}
