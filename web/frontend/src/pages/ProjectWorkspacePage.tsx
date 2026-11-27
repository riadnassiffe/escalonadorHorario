import { useMemo, useState } from "react";
import { useMutation, useQueryClient } from "@tanstack/react-query";
import { useNavigate, useParams } from "react-router-dom";
import {
  ArrowLeft,
  Download,
  FileCode2,
  FileSpreadsheet,
  Moon,
  Square,
  Sun,
  Upload,
  Zap,
} from "lucide-react";
import { apiDownload, apiFetch } from "../api/client";
import {
  useConfig,
  useDisciplinas,
  useInvalidateWorkspace,
  useJobStream,
  useProfessores,
  useProject,
  useProjectJobs,
  useSalas,
  useTimetable,
  useTimetableVersions,
  useTurmas,
} from "../api/hooks";
import type { DomainSnapshot } from "../domain/conflictEngine";
import { useUiStore } from "../store/uiStore";
import type { OptimizationJob } from "../types";
import { KpiBanner } from "../components/layout/KpiBanner";
import { TimetableView } from "../components/timetable/TimetableView";
import { ProfessoresTab } from "../components/entities/ProfessoresTab";
import { TurmasTab } from "../components/entities/TurmasTab";
import { SalasTab } from "../components/entities/SalasTab";
import { DisciplinasTab } from "../components/entities/DisciplinasTab";
import { ConfigTab } from "../components/optimization/ConfigTab";
import { OptimizeModal } from "../components/optimization/OptimizeModal";
import { DiagnosticsLogsTab } from "../components/logs/DiagnosticsLogsTab";

export function ProjectWorkspacePage() {
  const { projectId = "" } = useParams();
  const navigate = useNavigate();
  const qc = useQueryClient();
  const { activeTab, setActiveTab, darkMode, toggleDarkMode } = useUiStore();
  const invalidateAll = useInvalidateWorkspace(projectId);

  const [selectedVersionId, setSelectedVersionId] = useState<string | null>(null);
  const [showOptimizeModal, setShowOptimizeModal] = useState(false);

  const { data: project } = useProject(projectId);
  const { data: professores = [] } = useProfessores(projectId);
  const { data: salas = [] } = useSalas(projectId);
  const { data: turmas = [] } = useTurmas(projectId);
  const { data: disciplinas = [] } = useDisciplinas(projectId);
  const { data: config } = useConfig(projectId);
  const { data: timetable } = useTimetable(projectId, selectedVersionId);
  const { data: versions = [] } = useTimetableVersions(projectId);
  const { data: jobs = [] } = useProjectJobs(projectId);

  const activeJob = jobs.find((j) => j.status === "PENDING" || j.status === "RUNNING") || null;
  const streamEvent = useJobStream(activeJob?.id || null, () => {
    setSelectedVersionId(null);
    invalidateAll();
  });

  const snapshot: DomainSnapshot = useMemo(
    () => ({
      professores: Object.fromEntries(professores.map((p) => [p.code, p])),
      salas: Object.fromEntries(salas.map((r) => [r.code, r])),
      turmas: Object.fromEntries(turmas.map((t) => [t.code, t])),
      disciplinas: Object.fromEntries(disciplinas.map((d) => [d.code, d])),
      config: config || {},
      alocacoes: timetable?.alocacoes || [],
    }),
    [config, disciplinas, professores, salas, timetable?.alocacoes, turmas]
  );

  const optimizeMutation = useMutation({
    mutationFn: (payload: Record<string, any>) =>
      apiFetch<OptimizationJob>(`/projects/${projectId}/optimize`, {
        method: "POST",
        body: JSON.stringify(payload),
      }),
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ["jobs", projectId] });
      setShowOptimizeModal(false);
    },
  });

  const cancelJobMutation = useMutation({
    mutationFn: (jobId: string) => apiFetch(`/jobs/${jobId}/cancel`, { method: "POST" }),
    onSuccess: () => qc.invalidateQueries({ queryKey: ["jobs", projectId] }),
  });

  const activateVersionMutation = useMutation({
    mutationFn: (vid: string) =>
      apiFetch(`/projects/${projectId}/timetable/versions/${vid}/activate`, { method: "POST" }),
    onSuccess: () => {
      setSelectedVersionId(null);
      invalidateAll();
    },
  });

  const handleImportJson = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    const data = JSON.parse(await file.text());
    await apiFetch(`/projects/${projectId}/import-json`, { method: "POST", body: JSON.stringify({ data }) });
    setSelectedVersionId(null);
    invalidateAll();
  };

  const handleExportJson = async () => {
    const data = await apiFetch(`/projects/${projectId}/export-json`);
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `${project?.name || "projeto"}.json`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const tabs: Array<{ id: typeof activeTab; label: string }> = [
    { id: "grade", label: "📅 Grade Horária" },
    { id: "professores", label: `👨‍🏫 Professores (${professores.length})` },
    { id: "turmas", label: `🎓 Turmas (${turmas.length})` },
    { id: "salas", label: `🏢 Salas (${salas.length})` },
    { id: "disciplinas", label: `📚 Disciplinas (${disciplinas.length})` },
    { id: "config", label: "⚙️ Parâmetros" },
    { id: "logs", label: "🧾 Logs & Diagnóstico" },
  ];

  const liveRatio = streamEvent?.ratio ?? activeJob?.progress_ratio ?? 0;
  const liveMessage = streamEvent?.message ?? activeJob?.progress_message ?? "";

  return (
    <div className="min-h-screen bg-slate-50 dark:bg-slate-950">
      {/* Top Bar */}
      <header className="sticky top-0 z-30 border-b border-slate-200 bg-white/95 px-4 py-2.5 backdrop-blur dark:border-slate-800 dark:bg-slate-900/95">
        <div className="mx-auto flex max-w-[1600px] flex-wrap items-center justify-between gap-2">
          <div className="flex items-center gap-2.5">
            <button onClick={() => navigate("/")} className="rounded-lg border border-slate-300 p-1.5 hover:bg-slate-100 dark:border-slate-700 dark:hover:bg-slate-800">
              <ArrowLeft className="h-4 w-4" />
            </button>
            <img src="/app_icon.svg" alt="Logo" className="h-7 w-7" />
            <div>
              <h1 className="text-sm font-bold leading-tight">{project?.name || "Carregando..."}</h1>
              <span className="text-[11px] text-slate-500">Ano Letivo {project?.academic_year}</span>
            </div>
          </div>

          <div className="flex flex-wrap items-center gap-1.5 text-xs">
            <button
              onClick={() => setShowOptimizeModal(true)}
              className="inline-flex items-center gap-1.5 rounded-lg bg-blue-600 px-3.5 py-1.5 font-bold text-white shadow-xs hover:bg-blue-700"
            >
              <Zap className="h-4 w-4" /> OTIMIZAR GRADE
            </button>

            {versions.length > 0 && (
              <select
                value={selectedVersionId || project?.active_timetable_version_id || ""}
                onChange={(e) => activateVersionMutation.mutate(e.target.value)}
                className="rounded-lg border border-slate-300 bg-white px-2.5 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              >
                {versions.map((v) => (
                  <option key={v.id} value={v.id}>
                    v{v.version_number} — {v.label} {v.is_active ? "(Ativa)" : ""}
                  </option>
                ))}
              </select>
            )}

            <label className="inline-flex cursor-pointer items-center gap-1 rounded-lg border border-slate-300 px-2.5 py-1.5 hover:bg-slate-100 dark:border-slate-700 dark:hover:bg-slate-800">
              <Upload className="h-3.5 w-3.5" /> JSON
              <input type="file" accept=".json" onChange={handleImportJson} className="hidden" />
            </label>
            <button onClick={handleExportJson} className="inline-flex items-center gap-1 rounded-lg border border-slate-300 px-2.5 py-1.5 hover:bg-slate-100 dark:border-slate-700 dark:hover:bg-slate-800">
              <Download className="h-3.5 w-3.5" /> JSON
            </button>
            <button
              onClick={() => apiDownload(`/projects/${projectId}/export/html`, `${project?.name || "grade"}.html`)}
              className="inline-flex items-center gap-1 rounded-lg border border-slate-300 px-2.5 py-1.5 hover:bg-slate-100 dark:border-slate-700 dark:hover:bg-slate-800"
            >
              <FileCode2 className="h-3.5 w-3.5" /> HTML
            </button>
            <button
              onClick={() => apiDownload(`/projects/${projectId}/export/csv`, `${project?.name || "grade"}.csv`)}
              className="inline-flex items-center gap-1 rounded-lg border border-slate-300 px-2.5 py-1.5 hover:bg-slate-100 dark:border-slate-700 dark:hover:bg-slate-800"
            >
              <FileSpreadsheet className="h-3.5 w-3.5" /> CSV
            </button>
            <button onClick={toggleDarkMode} className="rounded-lg border border-slate-300 p-1.5 dark:border-slate-700">
              {darkMode ? <Sun className="h-4 w-4" /> : <Moon className="h-4 w-4" />}
            </button>
          </div>
        </div>
      </header>

      <div className="mx-auto max-w-[1600px] space-y-4 p-4">
        {activeJob && (
          <div className="flex items-center justify-between gap-4 rounded-xl border border-blue-200 bg-blue-50 px-4 py-3 dark:border-blue-900 dark:bg-blue-950/40">
            <div className="flex-1">
              <div className="flex items-center justify-between text-xs font-semibold text-blue-900 dark:text-blue-200">
                <span>Otimizando ({activeJob.algorithm}): {liveMessage}</span>
                <span>{Math.round(liveRatio * 100)}%</span>
              </div>
              <div className="mt-1.5 h-2 w-full overflow-hidden rounded-full bg-blue-200 dark:bg-blue-900">
                <div className="h-full bg-blue-600 transition-all" style={{ width: `${Math.round(liveRatio * 100)}%` }} />
              </div>
            </div>
            <button
              onClick={() => cancelJobMutation.mutate(activeJob.id)}
              className="inline-flex items-center gap-1 rounded-lg bg-rose-600 px-3 py-1.5 text-xs font-semibold text-white hover:bg-rose-700"
            >
              <Square className="h-3.5 w-3.5" /> Cancelar
            </button>
          </div>
        )}

        <KpiBanner timetable={timetable} />

        {/* Navigation Tabs */}
        <div className="flex flex-wrap gap-1 border-b border-slate-200 dark:border-slate-800">
          {tabs.map((t) => (
            <button
              key={t.id}
              onClick={() => setActiveTab(t.id)}
              className={`rounded-t-lg px-4 py-2 text-sm font-medium transition ${
                activeTab === t.id
                  ? "border-b-2 border-blue-600 bg-white text-blue-600 dark:bg-slate-900 dark:text-blue-400"
                  : "text-slate-600 hover:text-slate-900 dark:text-slate-400 dark:hover:text-slate-200"
              }`}
            >
              {t.label}
            </button>
          ))}
        </div>

        {/* Active Tab Content */}
        <div>
          {activeTab === "grade" && <TimetableView projectId={projectId} snapshot={snapshot} timetable={timetable} />}
          {activeTab === "professores" && (
            <ProfessoresTab projectId={projectId} project={project} professores={professores} />
          )}
          {activeTab === "turmas" && <TurmasTab projectId={projectId} turmas={turmas} salas={salas} disciplinas={disciplinas} professores={professores} />}
          {activeTab === "salas" && <SalasTab projectId={projectId} salas={salas} />}
          {activeTab === "disciplinas" && (
            <DisciplinasTab projectId={projectId} disciplinas={disciplinas} professores={professores} turmas={turmas} salas={salas} />
          )}
          {activeTab === "config" && <ConfigTab projectId={projectId} config={config} />}
          {activeTab === "logs" && <DiagnosticsLogsTab jobs={jobs} timetable={timetable} />}
        </div>
      </div>

      {showOptimizeModal && (
        <OptimizeModal
          config={config}
          onClose={() => setShowOptimizeModal(false)}
          onLaunch={(payload) => optimizeMutation.mutate(payload)}
        />
      )}
    </div>
  );
}
