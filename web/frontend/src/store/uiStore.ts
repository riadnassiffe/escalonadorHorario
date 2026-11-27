import { create } from "zustand";
import { persist } from "zustand/middleware";

interface UiState {
  darkMode: boolean;
  transposeGrid: boolean;
  activeTab: "grade" | "professores" | "turmas" | "salas" | "disciplinas" | "config" | "logs";
  disciplinasColumnWidths: Record<string, number>;
  toggleDarkMode: () => void;
  toggleTransposeGrid: () => void;
  setActiveTab: (tab: UiState["activeTab"]) => void;
  setDisciplinasColumnWidths: (widths: Record<string, number>) => void;
  resetDisciplinasColumnWidths: () => void;
}

export const useUiStore = create<UiState>()(
  persist(
    (set) => ({
      darkMode: false,
      transposeGrid: false,
      activeTab: "grade",
      disciplinasColumnWidths: {},
      toggleDarkMode: () => set((s) => ({ darkMode: !s.darkMode })),
      toggleTransposeGrid: () => set((s) => ({ transposeGrid: !s.transposeGrid })),
      setActiveTab: (activeTab) => set({ activeTab }),
      setDisciplinasColumnWidths: (disciplinasColumnWidths) => set({ disciplinasColumnWidths }),
      resetDisciplinasColumnWidths: () => set({ disciplinasColumnWidths: {} }),
    }),
    { name: "escalonador-ui" }
  )
);
