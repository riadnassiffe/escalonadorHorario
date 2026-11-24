import { useEffect, useState } from "react";
import { useMutation, useQuery, useQueryClient } from "@tanstack/react-query";
import { apiFetch } from "./client";
import { useAuthStore } from "../store/authStore";
import type {
  Disciplina,
  OptimizationConfig,
  OptimizationJob,
  Professor,
  ProfessorProfile,
  ProfessorTimetableResponse,
  Project,
  Room,
  TimeSlot,
  TimetableResponse,
  TimetableVersionSummary,
  Turma,
  UnallocatedLesson,
} from "../types";

export function useProjects() {
  return useQuery({
    queryKey: ["projects"],
    queryFn: () => apiFetch<Project[]>("/projects"),
  });
}

export function useProject(projectId: string) {
  return useQuery({
    queryKey: ["project", projectId],
    queryFn: () => apiFetch<Project>(`/projects/${projectId}`),
    enabled: Boolean(projectId),
  });
}

export function useProfessores(projectId: string) {
  return useQuery({
    queryKey: ["professores", projectId],
    queryFn: () => apiFetch<Professor[]>(`/projects/${projectId}/professores`),
    enabled: Boolean(projectId),
  });
}

export function useSalas(projectId: string) {
  return useQuery({
    queryKey: ["salas", projectId],
    queryFn: () => apiFetch<Room[]>(`/projects/${projectId}/salas`),
    enabled: Boolean(projectId),
  });
}

export function useTurmas(projectId: string) {
  return useQuery({
    queryKey: ["turmas", projectId],
    queryFn: () => apiFetch<Turma[]>(`/projects/${projectId}/turmas`),
    enabled: Boolean(projectId),
  });
}

export function useDisciplinas(projectId: string) {
  return useQuery({
    queryKey: ["disciplinas", projectId],
    queryFn: () => apiFetch<Disciplina[]>(`/projects/${projectId}/disciplinas`),
    enabled: Boolean(projectId),
  });
}

export function useConfig(projectId: string) {
  return useQuery({
    queryKey: ["config", projectId],
    queryFn: () => apiFetch<OptimizationConfig>(`/projects/${projectId}/config`),
    enabled: Boolean(projectId),
  });
}

export function useSlotTemplates(projectId: string) {
  return useQuery({
    queryKey: ["slot-templates", projectId],
    queryFn: () => apiFetch<{ slot_templates: Record<string, TimeSlot[]> }>(`/projects/${projectId}/slot-templates`),
    enabled: Boolean(projectId),
  });
}

export function useTimetable(projectId: string, versionId?: string | null) {
  const qs = versionId ? `?version_id=${versionId}` : "";
  return useQuery({
    queryKey: ["timetable", projectId, versionId || "active"],
    queryFn: () => apiFetch<TimetableResponse>(`/projects/${projectId}/timetable${qs}`),
    enabled: Boolean(projectId),
  });
}

export function useTimetableVersions(projectId: string) {
  return useQuery({
    queryKey: ["timetable-versions", projectId],
    queryFn: () => apiFetch<TimetableVersionSummary[]>(`/projects/${projectId}/timetable/versions`),
    enabled: Boolean(projectId),
  });
}

export function useUnallocated(projectId: string) {
  return useQuery({
    queryKey: ["unallocated", projectId],
    queryFn: () => apiFetch<{ unallocated: UnallocatedLesson[] }>(`/projects/${projectId}/timetable/unallocated`),
    enabled: Boolean(projectId),
  });
}

export function useProjectJobs(projectId: string) {
  return useQuery({
    queryKey: ["jobs", projectId],
    queryFn: () => apiFetch<OptimizationJob[]>(`/projects/${projectId}/jobs`),
    enabled: Boolean(projectId),
    refetchInterval: (query) => {
      const rows = query.state.data || [];
      return rows.some((j) => j.status === "PENDING" || j.status === "RUNNING") ? 2000 : false;
    },
  });
}

export function useInvalidateWorkspace(projectId: string) {
  const qc = useQueryClient();
  return () => {
    qc.invalidateQueries({ queryKey: ["project", projectId] });
    qc.invalidateQueries({ queryKey: ["professores", projectId] });
    qc.invalidateQueries({ queryKey: ["salas", projectId] });
    qc.invalidateQueries({ queryKey: ["turmas", projectId] });
    qc.invalidateQueries({ queryKey: ["disciplinas", projectId] });
    qc.invalidateQueries({ queryKey: ["config", projectId] });
    qc.invalidateQueries({ queryKey: ["timetable", projectId] });
    qc.invalidateQueries({ queryKey: ["timetable-versions", projectId] });
    qc.invalidateQueries({ queryKey: ["unallocated", projectId] });
    qc.invalidateQueries({ queryKey: ["jobs", projectId] });
  };
}

export function useTimetableMutations(projectId: string) {
  const qc = useQueryClient();
  const refresh = () => {
    qc.invalidateQueries({ queryKey: ["timetable", projectId] });
    qc.invalidateQueries({ queryKey: ["timetable-versions", projectId] });
    qc.invalidateQueries({ queryKey: ["unallocated", projectId] });
    qc.invalidateQueries({ queryKey: ["project", projectId] });
  };

  const moveLesson = useMutation({
    mutationFn: (payload: { aula_id: string; dia: string; inicio: string; fim: string; salas?: string[]; pin_after_move?: boolean }) =>
      apiFetch(`/projects/${projectId}/timetable/move`, { method: "POST", body: JSON.stringify(payload) }),
    onSuccess: refresh,
  });

  const swapLessons = useMutation({
    mutationFn: (payload: { aula_a_id: string; aula_b_id: string; pin_both?: boolean }) =>
      apiFetch(`/projects/${projectId}/timetable/swap`, { method: "POST", body: JSON.stringify(payload) }),
    onSuccess: refresh,
  });

  const changeRoom = useMutation({
    mutationFn: (payload: { aula_id: string; salas: string[]; swap_with_aula_id?: string | null; pin_after?: boolean }) =>
      apiFetch(`/projects/${projectId}/timetable/change-room`, { method: "POST", body: JSON.stringify(payload) }),
    onSuccess: refresh,
  });

  const allocateManual = useMutation({
    mutationFn: (payload: {
      aula_id: string;
      disciplina_id: string;
      dia: string;
      inicio: string;
      fim: string;
      salas: string[];
      semestre_alocado: number;
      is_pinned: boolean;
    }) => apiFetch(`/projects/${projectId}/timetable/allocate-manual`, { method: "POST", body: JSON.stringify(payload) }),
    onSuccess: refresh,
  });

  const pinLesson = useMutation({
    mutationFn: ({ aula_id, pinned }: { aula_id: string; pinned: boolean }) =>
      apiFetch(`/projects/${projectId}/timetable/lessons/${encodeURIComponent(aula_id)}/pin`, {
        method: "PATCH",
        body: JSON.stringify({ pinned }),
      }),
    onSuccess: refresh,
  });

  const pinAll = useMutation({
    mutationFn: (pinned: boolean) =>
      apiFetch(`/projects/${projectId}/timetable/pin-all`, { method: "POST", body: JSON.stringify({ pinned }) }),
    onSuccess: refresh,
  });

  const removeLesson = useMutation({
    mutationFn: (aula_id: string) =>
      apiFetch(`/projects/${projectId}/timetable/lessons/${encodeURIComponent(aula_id)}`, { method: "DELETE" }),
    onSuccess: refresh,
  });

  const clearTimetable = useMutation({
    mutationFn: () => apiFetch(`/projects/${projectId}/timetable`, { method: "DELETE" }),
    onSuccess: refresh,
  });

  return { moveLesson, swapLessons, changeRoom, allocateManual, pinLesson, pinAll, removeLesson, clearTimetable };
}

export interface JobStreamEvent {
  type: "progress" | "done";
  status: string;
  ratio?: number;
  message?: string;
  result_version_id?: string | null;
}

export function useJobStream(jobId: string | null, onDone?: () => void) {
  const token = useAuthStore((s) => s.accessToken);
  const [event, setEvent] = useState<JobStreamEvent | null>(null);

  useEffect(() => {
    if (!jobId || !token) return;
    const proto = window.location.protocol === "https:" ? "wss" : "ws";
    const ws = new WebSocket(`${proto}://${window.location.host}/api/v1/jobs/${jobId}/stream?token=${encodeURIComponent(token)}`);
    ws.onmessage = (msg) => {
      try {
        const data = JSON.parse(msg.data) as JobStreamEvent;
        setEvent(data);
        if (data.type === "done") {
          onDone?.();
          ws.close();
        }
      } catch {
        // ignore
      }
    };
    return () => {
      ws.close();
    };
  }, [jobId, token]);

  return event;
}

export function useProfessorProfiles() {
  return useQuery({
    queryKey: ["professor-profiles"],
    queryFn: () => apiFetch<ProfessorProfile[]>("/professor/me/profiles"),
  });
}

export function useProfessorTimetable(professorId: string | null) {
  return useQuery({
    queryKey: ["professor-timetable", professorId],
    queryFn: () => apiFetch<ProfessorTimetableResponse>(`/professor/me/profiles/${professorId}/timetable`),
    enabled: Boolean(professorId),
  });
}

export function useUpdateProfessorPreferences(professorId: string) {
  const qc = useQueryClient();
  return useMutation({
    mutationFn: (payload: {
      preferencia_dias: Record<string, number>;
      turnos_pretendidos: Record<string, string[]>;
      disponibilidade?: Record<string, TimeSlot[]>;
      peso_janela?: number;
    }) =>
      apiFetch<ProfessorProfile>(`/professor/me/profiles/${professorId}/preferences`, {
        method: "PATCH",
        body: JSON.stringify(payload),
      }),
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ["professor-profiles"] });
    },
  });
}
