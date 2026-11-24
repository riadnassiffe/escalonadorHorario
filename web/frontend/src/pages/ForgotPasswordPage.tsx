import { useState } from "react";
import { Link } from "react-router-dom";
import { ArrowLeft, Check, Copy, Mail } from "lucide-react";
import { apiFetch } from "../api/client";
import type { ForgotPasswordResponse } from "../types";

export function ForgotPasswordPage() {
  const [email, setEmail] = useState("");
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");
  const [result, setResult] = useState<ForgotPasswordResponse | null>(null);
  const [copied, setCopied] = useState(false);

  const submit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError("");
    setLoading(true);
    try {
      const res = await apiFetch<ForgotPasswordResponse>("/auth/forgot-password", {
        method: "POST",
        body: JSON.stringify({ email }),
      });
      setResult(res);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Erro ao solicitar redefinição");
    } finally {
      setLoading(false);
    }
  };

  const copyDevUrl = async () => {
    if (!result?.dev_reset_url) return;
    await navigator.clipboard.writeText(result.dev_reset_url);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <div className="flex min-h-screen items-center justify-center bg-slate-100 p-4 dark:bg-slate-950">
      <div className="w-full max-w-md rounded-2xl border border-slate-200 bg-white p-7 shadow-xl dark:border-slate-800 dark:bg-slate-900">
        <div className="flex items-center gap-3">
          <img src="/app_icon.svg" alt="Logo" className="h-10 w-10" />
          <div>
            <h1 className="text-lg font-bold">Recuperar Senha</h1>
            <p className="text-xs text-slate-500">Informe seu e-mail para redefinir o acesso</p>
          </div>
        </div>

        <form onSubmit={submit} className="mt-5 space-y-3 text-sm">
          <div>
            <label className="text-xs font-medium text-slate-500">E-mail cadastrado</label>
            <input
              required
              type="email"
              value={email}
              onChange={(e) => setEmail(e.target.value)}
              placeholder="professor@escola.edu.br"
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
            <Mail className="h-4 w-4" />
            {loading ? "Enviando..." : "Enviar instruções"}
          </button>
        </form>

        {result && (
          <div className="mt-4 space-y-3">
            <div className="rounded-lg border border-emerald-200 bg-emerald-50 p-3 text-xs text-emerald-800 dark:border-emerald-900 dark:bg-emerald-950/50 dark:text-emerald-200">
              {result.message}
            </div>

            {result.dev_reset_url && (
              <div className="rounded-lg border border-amber-300 bg-amber-50 p-3 text-xs text-amber-900 dark:border-amber-800 dark:bg-amber-950/40 dark:text-amber-200">
                <p className="font-semibold">Modo Desenvolvimento (SMTP não configurado)</p>
                <p className="mt-1 break-all font-mono text-[11px]">{result.dev_reset_url}</p>
                <div className="mt-2 flex flex-wrap gap-2">
                  <Link
                    to={`/reset-password?token=${encodeURIComponent(result.dev_reset_token || "")}`}
                    className="inline-flex items-center rounded bg-amber-600 px-2.5 py-1 font-semibold text-white hover:bg-amber-700"
                  >
                    Abrir link de redefinição
                  </Link>
                  <button
                    type="button"
                    onClick={copyDevUrl}
                    className="inline-flex items-center gap-1 rounded border border-amber-400 px-2.5 py-1 font-medium hover:bg-amber-100 dark:border-amber-700 dark:hover:bg-amber-900/50"
                  >
                    {copied ? <Check className="h-3.5 w-3.5" /> : <Copy className="h-3.5 w-3.5" />}
                    {copied ? "Copiado" : "Copiar link"}
                  </button>
                </div>
              </div>
            )}
          </div>
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
