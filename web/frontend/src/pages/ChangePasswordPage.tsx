import { useState } from "react";
import { useNavigate } from "react-router-dom";
import { ArrowLeft, LogOut, ShieldCheck } from "lucide-react";
import { apiFetch } from "../api/client";
import { useAuthStore } from "../store/authStore";
import type { User } from "../types";

export function ChangePasswordPage() {
  const navigate = useNavigate();
  const { user, setUser, logout } = useAuthStore();
  const [currentPassword, setCurrentPassword] = useState("");
  const [newPassword, setNewPassword] = useState("");
  const [confirmPassword, setConfirmPassword] = useState("");
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");

  const mustChange = Boolean(user?.must_change_password);

  const submit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError("");
    if (newPassword !== confirmPassword) {
      setError("As senhas não coincidem.");
      return;
    }
    setLoading(true);
    try {
      const updated = await apiFetch<User>("/auth/change-password", {
        method: "POST",
        body: JSON.stringify({ current_password: currentPassword, new_password: newPassword }),
      });
      setUser(updated);
      navigate(updated.role === "PROFESSOR" ? "/professor" : "/");
    } catch (err) {
      setError(err instanceof Error ? err.message : "Erro ao alterar senha.");
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="flex min-h-screen items-center justify-center bg-slate-100 p-4 dark:bg-slate-950">
      <div className="w-full max-w-md rounded-2xl border border-slate-200 bg-white p-7 shadow-xl dark:border-slate-800 dark:bg-slate-900">
        <div className="flex items-center gap-3">
          <img src="/app_icon.svg" alt="Logo" className="h-10 w-10" />
          <div>
            <h1 className="text-lg font-bold">Alterar Senha</h1>
            <p className="text-xs text-slate-500">
              {user?.full_name} ({user?.email})
            </p>
          </div>
        </div>

        {mustChange && (
          <div className="mt-4 rounded-lg border border-amber-300 bg-amber-50 p-3 text-xs text-amber-900 dark:border-amber-800 dark:bg-amber-950/40 dark:text-amber-200">
            <strong>Primeiro acesso:</strong> por segurança, defina uma senha pessoal antes de continuar para o sistema.
          </div>
        )}

        <form onSubmit={submit} className="mt-4 space-y-3 text-sm">
          <div>
            <label className="text-xs font-medium text-slate-500">
              {mustChange ? "Senha inicial / atual" : "Senha atual"}
            </label>
            <input
              required
              type="password"
              value={currentPassword}
              onChange={(e) => setCurrentPassword(e.target.value)}
              className="mt-1 w-full rounded-lg border border-slate-300 px-3 py-2 dark:border-slate-700 dark:bg-slate-800"
            />
          </div>
          <div>
            <label className="text-xs font-medium text-slate-500">Nova senha (mín. 6 caracteres)</label>
            <input
              required
              type="password"
              minLength={6}
              value={newPassword}
              onChange={(e) => setNewPassword(e.target.value)}
              className="mt-1 w-full rounded-lg border border-slate-300 px-3 py-2 dark:border-slate-700 dark:bg-slate-800"
            />
          </div>
          <div>
            <label className="text-xs font-medium text-slate-500">Confirmar nova senha</label>
            <input
              required
              type="password"
              minLength={6}
              value={confirmPassword}
              onChange={(e) => setConfirmPassword(e.target.value)}
              className="mt-1 w-full rounded-lg border border-slate-300 px-3 py-2 dark:border-slate-700 dark:bg-slate-800"
            />
          </div>

          {error && (
            <div className="rounded bg-rose-100 p-2 text-xs text-rose-700 dark:bg-rose-950 dark:text-rose-300">
              {error}
            </div>
          )}

          <button
            disabled={loading}
            type="submit"
            className="inline-flex w-full items-center justify-center gap-2 rounded-lg bg-blue-600 py-2.5 font-semibold text-white hover:bg-blue-700 disabled:opacity-50"
          >
            <ShieldCheck className="h-4 w-4" />
            {loading ? "Salvando..." : "Atualizar senha e continuar"}
          </button>
        </form>

        <div className="mt-5 flex items-center justify-between border-t border-slate-200 pt-4 text-xs dark:border-slate-800">
          {!mustChange ? (
            <button
              type="button"
              onClick={() => navigate(user?.role === "PROFESSOR" ? "/professor" : "/")}
              className="inline-flex items-center gap-1 font-medium text-blue-600 hover:underline dark:text-blue-400"
            >
              <ArrowLeft className="h-3.5 w-3.5" /> Voltar
            </button>
          ) : (
            <span />
          )}
          <button
            type="button"
            onClick={() => {
              logout();
              navigate("/login");
            }}
            className="inline-flex items-center gap-1 text-slate-500 hover:text-slate-800 dark:hover:text-slate-200"
          >
            <LogOut className="h-3.5 w-3.5" /> Sair
          </button>
        </div>
      </div>
    </div>
  );
}
