import { useState } from "react";
import { Link, useSearchParams } from "react-router-dom";
import { ArrowLeft, KeyRound } from "lucide-react";
import { apiFetch } from "../api/client";

export function ResetPasswordPage() {
  const [searchParams] = useSearchParams();
  const [token, setToken] = useState(searchParams.get("token") || "");
  const [newPassword, setNewPassword] = useState("");
  const [confirmPassword, setConfirmPassword] = useState("");
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");
  const [done, setDone] = useState(false);

  const submit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError("");
    if (newPassword !== confirmPassword) {
      setError("As senhas não coincidem.");
      return;
    }
    setLoading(true);
    try {
      await apiFetch("/auth/reset-password", {
        method: "POST",
        body: JSON.stringify({ token: token.trim(), new_password: newPassword }),
      });
      setDone(true);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Não foi possível redefinir a senha.");
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
            <h1 className="text-lg font-bold">Redefinir Senha</h1>
            <p className="text-xs text-slate-500">Defina uma nova senha para sua conta</p>
          </div>
        </div>

        {done ? (
          <div className="mt-5 space-y-4 text-sm">
            <div className="rounded-lg border border-emerald-200 bg-emerald-50 p-3 text-emerald-800 dark:border-emerald-900 dark:bg-emerald-950/50 dark:text-emerald-200">
              Senha redefinida com sucesso! Já é possível entrar com sua nova senha.
            </div>
            <Link
              to="/login"
              className="block w-full rounded-lg bg-blue-600 py-2.5 text-center font-semibold text-white hover:bg-blue-700"
            >
              Ir para o Login
            </Link>
          </div>
        ) : (
          <form onSubmit={submit} className="mt-5 space-y-3 text-sm">
            <div>
              <label className="text-xs font-medium text-slate-500">Token de redefinição</label>
              <input
                required
                value={token}
                onChange={(e) => setToken(e.target.value)}
                placeholder="Cole o token recebido"
                className="mt-1 w-full rounded-lg border border-slate-300 px-3 py-2 font-mono text-xs dark:border-slate-700 dark:bg-slate-800"
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
              <KeyRound className="h-4 w-4" />
              {loading ? "Salvando..." : "Salvar nova senha"}
            </button>
          </form>
        )}

        <div className="mt-5 border-t border-slate-200 pt-4 text-center text-xs dark:border-slate-800">
          <Link to="/login" className="inline-flex items-center gap-1 font-medium text-blue-600 hover:underline dark:text-blue-400">
            <ArrowLeft className="h-3.5 w-3.5" /> Voltar para o login
          </Link>
        </div>
      </div>
    </div>
  );
}
