import { create } from "zustand";

export type ViewPerspective = "TURMA" | "PROFESSOR" | "SALA" | "GERAL";

interface TimetableUiState {
  viewType: ViewPerspective;
  targetId: string;
  semesterFilter: 0 | 1 | 2;
  searchQuery: string;
  selectedAulaId: string | null;
  setViewType: (viewType: ViewPerspective) => void;
  setTargetId: (targetId: string) => void;
  setSemesterFilter: (semesterFilter: 0 | 1 | 2) => void;
  setSearchQuery: (searchQuery: string) => void;
  setSelectedAulaId: (selectedAulaId: string | null) => void;
}

export const useTimetableStore = create<TimetableUiState>((set) => ({
  viewType: "TURMA",
  targetId: "",
  semesterFilter: 0,
  searchQuery: "",
  selectedAulaId: null,
  setViewType: (viewType) => set({ viewType, targetId: "", selectedAulaId: null }),
  setTargetId: (targetId) => set({ targetId }),
  setSemesterFilter: (semesterFilter) => set({ semesterFilter }),
  setSearchQuery: (searchQuery) => set({ searchQuery }),
  setSelectedAulaId: (selectedAulaId) => set({ selectedAulaId }),
}));
