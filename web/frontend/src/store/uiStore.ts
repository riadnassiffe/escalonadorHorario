import { create } from "zustand";
import { persist } from "zustand/middleware";

interface UiState {
  darkMode: boolean;
  transposeGrid: boolean;
  activeTab: "grade" | "professores" | "turmas" | "salas" | "disciplinas" | "config" | "logs";
  toggleDarkMode: () => void;
  toggleTransposeGrid: () => void;
  setActiveTab: (tab: UiState["activeTab"]) => void;
}

export const useUiStore = create<UiState>()(
  persist(
    (set) => ({
      darkMode: false,
      transposeGrid: false,
      activeTab: "grade",
      toggleDarkMode: () => set((s) => ({ darkMode: !s.darkMode })),
      toggleTransposeGrid: () => set((s) => ({ transposeGrid: !s.transposeGrid })),
      setActiveTab: (activeTab) => set({ activeTab }),
    }),
    { name: "escalonador-ui" }
  )
);
