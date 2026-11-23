export function normalizeText(str: string): string {
  return str
    .normalize("NFD")
    .replace(/[\u0300-\u036f]/g, "")
    .toLowerCase();
}

const collator = new Intl.Collator("pt-BR", { numeric: true, sensitivity: "base" });

export function naturalCompare(a: string | number, b: string | number): number {
  return collator.compare(String(a ?? ""), String(b ?? ""));
}

export function filterAndSortItems<T>(
  items: T[],
  query: string,
  getSearchText: (item: T) => string,
  getSortValue: (item: T) => string | number,
  ascending: boolean
): T[] {
  const q = normalizeText(query.trim());
  const filtered = q ? items.filter((item) => normalizeText(getSearchText(item)).includes(q)) : [...items];
  filtered.sort((a, b) => {
    const cmp = naturalCompare(getSortValue(a), getSortValue(b));
    return ascending ? cmp : -cmp;
  });
  return filtered;
}
