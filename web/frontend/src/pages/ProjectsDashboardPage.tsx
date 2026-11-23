import { useState } from "react";
import { useMutation, useQueryClient } from "@tanstack/react-query";
import { useNavigate } from "react-router-dom";
import { FolderOpen, LogOut, Moon, Plus, Sun, Trash2, Upload } from "lucide-react";
import { apiFetch } from "../api/client";
import { useProjects } from "../api/hooks";
import { useAuthStore } from "../store/authStore";
import { useUiStore } from "../store/uiStore";
import type { Project } from "../types";

export function ProjectsDashboardPage() {
  const navigate = useNavigate();
  const qc = useQueryClient();
  const { user, logout } = useAuthStore();
  const { darkMode, toggleDarkMode } = useUiStore();
  const { data: projects = [], isLoading } = useProjects();

  const [newName, setNewName] = useState("");
  const [loadExample, setLoadExample] = useState(true);

  const createMutation = useMutation({
    mutationFn: () =>
      apiFetch<Project>("/projects", {
        method: "POST",
        body: JSON.stringify({ name: newName || "Nova Grade 2026", load_default_example: loadExample }),
      }),
    onSuccess: (p) => {
      qc.invalidateQueries({ queryKey: ["projects"] });
      setNewName("");
      navigate(`/projects/${p.id}`);
    },
  });

  const deleteMutation = useMutation({
    mutationFn: (id: string) => apiFetch(`/projects/${id}`, { method: "DELETE" }),
    onSuccess: () => qc.invalidateQueries({ queryKey: ["projects"] }),
  });

  const handleImportJsonNew = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const file = e.target.files?.[0];
    if (!file) return;
    const jsonData = JSON.parse(await file.text());
    const p = await apiFetch<Project>("/projects", {
      method: "POST",
      body: JSON.stringify({ name: file.name.replace(/\.json$/i, ""), load_default_example: false }),
    });
    await apiFetch(`/projects/${p.id}/import-json`, {
      method: "POST",
      body: JSON.stringify({ data: jsonData }),
    });
    qc.invalidateQueries({ queryKey: ["projects"] });
    navigate(`/projects/${p.id}`);
  };

  return (
    <div className="min-h-screen bg-slate-50 dark:bg-slate-950">
      <header className="border-b border-slate-200 bg-white px-6 py-3.5 dark:border-slate-800 dark:bg-slate-900">
        <div className="mx-auto flex max-w-6xl items-center justify-between">
          <div className="flex items-center gap-3">
            <img src="/app_icon.svg" alt="Logo" className="h-8 w-8" />
            <div>
              <h1 className="text-base font-bold">Escalonador Inteligente de Horários</h1>
              <p className="text-xs text-slate-500">
                {user?.institution_name} • {user?.full_name} ({user?.role})
              </p>
            </div>
          </div>
          <div className="flex items-center gap-2">
            <button onClick={toggleDarkMode} className="rounded-lg border border-slate-300 p-2 dark:border-slate-700">
              {darkMode ? <Sun className="h-4 w-4" /> : <Moon className="h-4 w-4" />}
            </button>
            <button
              onClick={() => {
                logout();
                navigate("/login");
              }}
              className="inline-flex items-center gap-1.5 rounded-lg border border-slate-300 px-3 py-1.5 text-xs font-medium dark:border-slate-700"
            >
              <LogOut className="h-4 w-4" /> Sair
            </button>
          </div>
        </div>
      </header>

      <main className="mx-auto max-w-6xl space-y-6 p-6">
        <div className="rounded-xl border border-slate-200 bg-white p-5 dark:border-slate-800 dark:bg-slate-900">
          <h2 className="text-sm font-bold uppercase text-slate-500">Novo Projeto / Cenário Acadêmico</h2>
          <div className="mt-3 flex flex-wrap items-center gap-3">
            <input
              value={newName}
              onChange={(e) => setNewName(e.target.value)}
              placeholder="Ex: Ensino Médio 2026 - 1º Semestre"
              className="flex-1 min-w-[240px] rounded-lg border border-slate-300 px-3 py-2 text-sm dark:border-slate-700 dark:bg-slate-800"
            />
            <label className="flex items-center gap-2 text-xs">
              <input type="checkbox" checked={loadExample} onChange={(e) => setLoadExample(e.target.checked)} />
              Carregar cenário padrão
            </label>
            <button
              onClick={() => createMutation.mutate()}
              className="inline-flex items-center gap-1.5 rounded-lg bg-blue-600 px-4 py-2 text-sm font-semibold text-white hover:bg-blue-700"
            >
              <Plus className="h-4 w-4" /> Criar Projeto
            </button>
            <label className="inline-flex cursor-pointer items-center gap-1.5 rounded-lg border border-slate-300 px-4 py-2 text-sm font-medium hover:bg-slate-50 dark:border-slate-700 dark:hover:bg-slate-800">
              <Upload className="h-4 w-4" /> Importar JSON
              <input type="file" accept=".json" onChange={handleImportJsonNew} className="hidden" />
            </label>
          </div>
        </div>

        <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-3">
          {isLoading ? (
            <p className="text-sm text-slate-500">Carregando projetos...</p>
          ) : projects.length === 0 ? (
            <p className="text-sm text-slate-500">Nenhum projeto criado ainda.</p>
          ) : (
            projects.map((p) => (
              <div key={p.id} className="flex flex-col justify-between rounded-xl border border-slate-200 bg-white p-5 shadow-xs dark:border-slate-800 dark:bg-slate-900">
                <div>
                  <h3 className="text-base font-bold">{p.name}</h3>
                  <p className="mt-1 text-xs text-slate-500">Ano Letivo: {p.academic_year}</p>
                  <div className="mt-3 flex flex-wrap gap-1.5 text-[11px]">
                    <span className="rounded bg-slate-100 px-2 py-0.5 dark:bg-slate-800">{p.counts.professores} profs</span>
                    <span className="rounded bg-slate-100 px-2 py-0.5 dark:bg-slate-800">{p.counts.turmas} turmas</span>
                    <span className="rounded bg-slate-100 px-2 py-0.5 dark:bg-slate-800">{p.counts.salas} salas</span>
                    <span className="rounded bg-slate-100 px-2 py-0.5 dark:bg-slate-800">{p.counts.disciplinas} disc.</span>
                  </div>
                </div>
                <div className="mt-5 flex items-center justify-between border-t border-slate-100 pt-3 dark:border-slate-800">
                  <button
                    onClick={() => navigate(`/projects/${p.id}`)}
                    className="inline-flex items-center gap-1.5 rounded-lg bg-blue-600 px-3 py-1.5 text-xs font-semibold text-white hover:bg-blue-700"
                  >
                    <FolderOpen className="h-3.5 w-3.5" /> Abrir Workspace
                  </button>
                  <button
                    onClick={() => {
                      if (window.confirm(`Excluir projeto "${p.name}"?`)) deleteMutation.mutate(p.id);
                    }}
                    className="rounded p-1.5 text-rose-600 hover:bg-rose-50 dark:hover:bg-rose-950"
                  >
                    <Trash2 className="h-4 w-4" />
                  </button>
                </div>
              </div>
            ))
          )}
        </div>
      </main>
    </div>
  );
}
