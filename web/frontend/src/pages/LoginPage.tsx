import { useState } from "react";
import { useNavigate } from "react-router-dom";
import { apiFetch } from "../api/client";
import { useAuthStore } from "../store/authStore";
import type { User } from "../types";

interface TokenResp {
  access_token: string;
  refresh_token: string;
  user: User;
}

export function LoginPage() {
  const navigate = useNavigate();
  const setAuth = useAuthStore((s) => s.setAuth);
  const [mode, setMode] = useState<"login" | "register">("login");
  const [institutionName, setInstitutionName] = useState("");
  const [fullName, setFullName] = useState("");
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);

  const submit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError("");
    setLoading(true);
    try {
      const path = mode === "login" ? "/auth/login" : "/auth/register";
      const body =
        mode === "login"
          ? { email, password }
          : { institution_name: institutionName, full_name: fullName, email, password };
      const res = await apiFetch<TokenResp>(path, { method: "POST", body: JSON.stringify(body) });
      setAuth(res.access_token, res.refresh_token, res.user);
      navigate("/");
    } catch (err) {
      setError(err instanceof Error ? err.message : "Falha na autenticação");
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
            <h1 className="text-lg font-bold">Escalonador de Horários</h1>
            <p className="text-xs text-slate-500">Sistema Web Multi-Institucional</p>
          </div>
        </div>

        <div className="mt-5 grid grid-cols-2 rounded-lg bg-slate-100 p-1 text-sm font-medium dark:bg-slate-800">
          <button
            type="button"
            onClick={() => setMode("login")}
            className={`rounded-md py-1.5 ${mode === "login" ? "bg-white shadow-xs dark:bg-slate-900" : "text-slate-500"}`}
          >
            Entrar
          </button>
          <button
            type="button"
            onClick={() => setMode("register")}
            className={`rounded-md py-1.5 ${mode === "register" ? "bg-white shadow-xs dark:bg-slate-900" : "text-slate-500"}`}
          >
            Criar Instituição
          </button>
        </div>

        <form onSubmit={submit} className="mt-4 space-y-3 text-sm">
          {mode === "register" && (
            <>
              <div>
                <label className="text-xs font-medium text-slate-500">Nome da Instituição</label>
                <input required value={institutionName} onChange={(e) => setInstitutionName(e.target.value)} className="mt-1 w-full rounded-lg border border-slate-300 px-3 py-2 dark:border-slate-700 dark:bg-slate-800" />
              </div>
              <div>
                <label className="text-xs font-medium text-slate-500">Seu Nome Completo</label>
                <input required value={fullName} onChange={(e) => setFullName(e.target.value)} className="mt-1 w-full rounded-lg border border-slate-300 px-3 py-2 dark:border-slate-700 dark:bg-slate-800" />
              </div>
            </>
          )}
          <div>
            <label className="text-xs font-medium text-slate-500">E-mail</label>
            <input required type="email" value={email} onChange={(e) => setEmail(e.target.value)} className="mt-1 w-full rounded-lg border border-slate-300 px-3 py-2 dark:border-slate-700 dark:bg-slate-800" />
          </div>
          <div>
            <label className="text-xs font-medium text-slate-500">Senha</label>
            <input required type="password" minLength={6} value={password} onChange={(e) => setPassword(e.target.value)} className="mt-1 w-full rounded-lg border border-slate-300 px-3 py-2 dark:border-slate-700 dark:bg-slate-800" />
          </div>
          {error && <div className="rounded bg-rose-100 p-2 text-xs text-rose-700 dark:bg-rose-950 dark:text-rose-300">{error}</div>}
          <button
            disabled={loading}
            type="submit"
            className="w-full rounded-lg bg-blue-600 py-2.5 font-semibold text-white hover:bg-blue-700 disabled:opacity-50"
          >
            {loading ? "Aguarde..." : mode === "login" ? "Acessar Sistema" : "Cadastrar e Entrar"}
          </button>
        </form>
      </div>
    </div>
  );
}
