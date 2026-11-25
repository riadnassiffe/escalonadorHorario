import { useMemo, useState } from "react";
import { useMutation, useQueryClient } from "@tanstack/react-query";
import { Check, Copy, KeyRound, Lock, LockOpen, Pencil, Plus, Trash2, Users, X } from "lucide-react";
import { apiFetch } from "../../api/client";
import { filterAndSortItems } from "../../domain/searchSort";
import { ALL_DAYS, DAY_SHORT_LABELS } from "../../domain/time";
import { useAuthStore } from "../../store/authStore";
import type {
  BatchProvisionResponse,
  DayOfWeek,
  Professor,
  ProfessorCreateResponse,
  ProfessorCredential,
  Project,
  TimeSlot,
} from "../../types";
import { SearchSortTableBar } from "../common/SearchSortTableBar";

interface Props {
  projectId: string;
  project?: Project;
  professores: Professor[];
}

const SHIFTS: Array<{ key: "MANHA" | "TARDE" | "NOITE"; label: string }> = [
  { key: "MANHA", label: "Manhã" },
  { key: "TARDE", label: "Tarde" },
  { key: "NOITE", label: "Noite" },
];

export function ProfessoresTab({ projectId, project, professores }: Props) {
  const qc = useQueryClient();
  const user = useAuthStore((s) => s.user);
  const [search, setSearch] = useState("");
  const [sortBy, setSortBy] = useState("code");
  const [asc, setAsc] = useState(true);
  const [editing, setEditing] = useState<Professor | null>(null);
  const [creating, setCreating] = useState(false);
  const [modalError, setModalError] = useState("");
  const [credentialsModal, setCredentialsModal] = useState<{
    title: string;
    subtitle?: string;
    items: ProfessorCredential[];
  } | null>(null);
  const [showBatchDialog, setShowBatchDialog] = useState(false);

  const isAdmin = user?.role === "ADMIN";
  const canManage = user?.role === "ADMIN" || user?.role === "COORDENADOR";

  const saveMutation = useMutation({
    mutationFn: async (prof: Omit<Professor, "id"> & { isNew: boolean }) => {
      if (prof.isNew) {
        return apiFetch<ProfessorCreateResponse>(`/projects/${projectId}/professores`, {
          method: "POST",
          body: JSON.stringify(prof),
        });
      }
      return apiFetch<Professor>(`/projects/${projectId}/professores/${encodeURIComponent(prof.code)}`, {
        method: "PUT",
        body: JSON.stringify(prof),
      });
    },
    onSuccess: (res, vars) => {
      qc.invalidateQueries({ queryKey: ["professores", projectId] });
      qc.invalidateQueries({ queryKey: ["project", projectId] });
      setEditing(null);
      setCreating(false);
      setModalError("");
      if (vars.isNew && "initial_password" in res) {
        const created = res as ProfessorCreateResponse;
        if (created.initial_password || created.delivery_note) {
          setCredentialsModal({
            title: "Acesso Inicial do Professor Gerado",
            subtitle: "Copie e envie as credenciais abaixo para o professor realizar o 1º acesso.",
            items: [
              {
                code: created.code,
                nome: created.nome,
                email: created.email || "",
                initial_password: created.initial_password,
                account_created: created.account_created,
                password_reset: false,
                email_sent: created.email_sent,
                delivery_note: created.delivery_note,
              },
            ],
          });
        }
      }
    },
    onError: (err) => {
      setModalError(err instanceof Error ? err.message : "Erro ao salvar professor.");
    },
  });

  const deleteMutation = useMutation({
    mutationFn: (code: string) =>
      apiFetch(`/projects/${projectId}/professores/${encodeURIComponent(code)}`, { method: "DELETE" }),
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ["professores", projectId] });
      qc.invalidateQueries({ queryKey: ["project", projectId] });
    },
  });

  const provisionSingleMutation = useMutation({
    mutationFn: (prof: Professor) =>
      apiFetch<ProfessorCredential>(
        `/projects/${projectId}/professores/${encodeURIComponent(prof.code)}/provision-access`,
        {
          method: "POST",
          body: JSON.stringify({ reset_password: true }),
        }
      ),
    onSuccess: (cred) => {
      qc.invalidateQueries({ queryKey: ["professores", projectId] });
      setCredentialsModal({
        title: cred.account_created ? "Acesso Criado" : "Nova Senha Inicial Gerada",
        subtitle: "No próximo login será solicitada a troca obrigatória da senha.",
        items: [cred],
      });
    },
  });

  const provisionBatchMutation = useMutation({
    mutationFn: (onlyMissing: boolean) =>
      apiFetch<BatchProvisionResponse>(`/projects/${projectId}/professores/provision-access-batch`, {
        method: "POST",
        body: JSON.stringify({ only_missing: onlyMissing }),
      }),
    onSuccess: (res) => {
      qc.invalidateQueries({ queryKey: ["professores", projectId] });
      setShowBatchDialog(false);
      setCredentialsModal({
        title: "Provisionamento em Lote Concluído",
        subtitle: `${res.created_count} criada(s), ${res.reset_count} redefinida(s), ${res.skipped_count} ignorada(s).`,
        items: res.credentials,
      });
    },
  });

  const togglePrefAccessMutation = useMutation({
    mutationFn: (allowEdit: boolean) =>
      apiFetch<Project>(`/projects/${projectId}/professor-preferences-access`, {
        method: "PATCH",
        body: JSON.stringify({ allow_edit: allowEdit }),
      }),
    onSuccess: () => {
      qc.invalidateQueries({ queryKey: ["project", projectId] });
      qc.invalidateQueries({ queryKey: ["projects"] });
    },
  });

  const filtered = useMemo(
    () =>
      filterAndSortItems(
        professores,
        search,
        (p) => `${p.code} ${p.nome} ${p.email || ""}`,
        (p) => (sortBy === "nome" ? p.nome : sortBy === "janela" ? p.peso_janela : p.code),
        asc
      ),
    [asc, professores, search, sortBy]
  );

  const openNew = async () => {
    const res = await apiFetch<{ next_id: string }>(`/projects/${projectId}/professores/next-id`);
    setModalError("");
    setEditing({
      id: res.next_id,
      code: res.next_id,
      nome: "",
      email: "",
      preferencia_dias: { SEG: 5, TER: 5, QUA: 5, QUI: 5, SEX: 5 },
      disponibilidade: Object.fromEntries(ALL_DAYS.map((d) => [d, [{ inicio: "08:00", fim: "12:00" }]])),
      turnos_pretendidos: Object.fromEntries(ALL_DAYS.map((d) => [d, ["MANHA", "TARDE", "NOITE"]])),
      carga_maxima_diaria_minutos: 0,
      max_aulas_por_dia: 0,
      peso_janela: 8,
    });
    setCreating(true);
  };

  const allowEdit = project?.allow_professor_preferences_edit ?? true;

  return (
    <div className="space-y-3">
      <div className="flex flex-wrap items-center justify-between gap-3">
        <SearchSortTableBar
          searchQuery={search}
          onSearchChange={setSearch}
          totalCount={professores.length}
          filteredCount={filtered.length}
          entityLabel="professores"
          sortOptions={[
            { value: "code", label: "ID" },
            { value: "nome", label: "Nome" },
            { value: "janela", label: "Aversão Janela" },
          ]}
          sortBy={sortBy}
          onSortByChange={setSortBy}
          ascending={asc}
          onToggleOrder={() => setAsc((s) => !s)}
        />

        <div className="flex flex-wrap items-center gap-2">
          {project && (
            isAdmin ? (
              <button
                type="button"
                disabled={togglePrefAccessMutation.isPending}
                onClick={() => togglePrefAccessMutation.mutate(!allowEdit)}
                title={
                  allowEdit
                    ? "Clique para bloquear a edição de preferências pelos professores"
                    : "Clique para liberar a edição de preferências pelos professores"
                }
                className={`inline-flex items-center gap-1.5 rounded-md border px-3 py-2 text-xs font-semibold transition ${
                  allowEdit
                    ? "border-emerald-300 bg-emerald-50 text-emerald-800 hover:bg-emerald-100 dark:border-emerald-800 dark:bg-emerald-950/50 dark:text-emerald-300"
                    : "border-amber-300 bg-amber-50 text-amber-800 hover:bg-amber-100 dark:border-amber-800 dark:bg-amber-950/50 dark:text-amber-300"
                }`}
              >
                {allowEdit ? (
                  <>
                    <LockOpen className="h-4 w-4" /> Edição Professores: Liberada
                  </>
                ) : (
                  <>
                    <Lock className="h-4 w-4" /> Edição Professores: Bloqueada
                  </>
                )}
              </button>
            ) : (
              <span
                className={`inline-flex items-center gap-1.5 rounded-md px-3 py-2 text-xs font-semibold ${
                  allowEdit
                    ? "bg-emerald-50 text-emerald-700 dark:bg-emerald-950/40 dark:text-emerald-300"
                    : "bg-amber-50 text-amber-700 dark:bg-amber-950/40 dark:text-amber-300"
                }`}
              >
                {allowEdit ? <LockOpen className="h-4 w-4" /> : <Lock className="h-4 w-4" />}
                {allowEdit ? "Edição Professores: Liberada" : "Edição Professores: Bloqueada"}
              </span>
            )
          )}

          {canManage && (
            <>
              <button
                type="button"
                onClick={() => setShowBatchDialog(true)}
                className="inline-flex shrink-0 items-center gap-1.5 rounded-md border border-slate-300 bg-white px-3 py-2 text-sm font-medium hover:bg-slate-50 dark:border-slate-700 dark:bg-slate-900 dark:hover:bg-slate-800"
              >
                <Users className="h-4 w-4" /> Gerar Acessos em Lote
              </button>
              <button
                onClick={openNew}
                className="inline-flex shrink-0 items-center gap-1.5 rounded-md bg-blue-600 px-3.5 py-2 text-sm font-medium text-white hover:bg-blue-700"
              >
                <Plus className="h-4 w-4" /> Adicionar Professor
              </button>
            </>
          )}
        </div>
      </div>

      <div className="overflow-x-auto rounded-lg border border-slate-200 bg-white dark:border-slate-800 dark:bg-slate-900">
        <table className="w-full text-left text-sm">
          <thead className="bg-slate-50 text-xs uppercase text-slate-500 dark:bg-slate-800/60">
            <tr>
              <th className="px-4 py-2.5">ID</th>
              <th className="px-4 py-2.5">Nome</th>
              <th className="px-4 py-2.5">Login / Acesso</th>
              <th className="px-4 py-2.5">Aversão Janela</th>
              <th className="px-4 py-2.5">Carga Máx/Dia</th>
              <th className="px-4 py-2.5">Preferência Dias (0-10)</th>
              <th className="px-4 py-2.5 text-right">Ações</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-slate-200 dark:divide-slate-800">
            {filtered.map((p) => (
              <tr key={p.code} className="hover:bg-slate-50 dark:hover:bg-slate-800/40">
                <td className="px-4 py-2.5 font-mono text-xs font-semibold">{p.code}</td>
                <td className="px-4 py-2.5 font-medium">{p.nome}</td>
                <td className="px-4 py-2.5 text-xs">
                  <div className="flex flex-wrap items-center gap-1.5">
                    {p.email && <span className="font-mono text-slate-600 dark:text-slate-300">{p.email}</span>}
                    {p.has_user_account ? (
                      p.must_change_password ? (
                        <span className="rounded-full bg-amber-100 px-2 py-0.5 text-[10px] font-semibold text-amber-800 dark:bg-amber-950 dark:text-amber-300">
                          1º acesso
                        </span>
                      ) : (
                        <span className="rounded-full bg-emerald-100 px-2 py-0.5 text-[10px] font-semibold text-emerald-800 dark:bg-emerald-950 dark:text-emerald-300">
                          Ativo
                        </span>
                      )
                    ) : (
                      <span className="rounded-full bg-slate-100 px-2 py-0.5 text-[10px] font-semibold text-slate-600 dark:bg-slate-800 dark:text-slate-400">
                        Sem acesso
                      </span>
                    )}
                  </div>
                </td>
                <td className="px-4 py-2.5">{p.peso_janela}/10</td>
                <td className="px-4 py-2.5">
                  {p.carga_maxima_diaria_minutos > 0 ? `${p.carga_maxima_diaria_minutos} min` : "Sem limite"}
                </td>
                <td className="px-4 py-2.5 text-xs text-slate-500">
                  {ALL_DAYS.map((d) => `${DAY_SHORT_LABELS[d]}:${p.preferencia_dias[d] ?? 5}`).join(" • ")}
                </td>
                <td className="px-4 py-2.5 text-right whitespace-nowrap">
                  {canManage && (
                    <>
                      <button
                        type="button"
                        title={p.has_user_account ? "Redefinir senha inicial" : "Gerar acesso e senha inicial"}
                        onClick={() => {
                          if (
                            !p.has_user_account ||
                            window.confirm(`Gerar nova senha inicial temporária para ${p.nome}?`)
                          ) {
                            provisionSingleMutation.mutate(p);
                          }
                        }}
                        className="mr-2 rounded p-1 text-amber-600 hover:bg-amber-50 dark:hover:bg-amber-950"
                      >
                        <KeyRound className="h-4 w-4" />
                      </button>
                      <button
                        onClick={() => {
                          setModalError("");
                          setEditing(structuredClone(p));
                          setCreating(false);
                        }}
                        className="mr-2 rounded p-1 text-blue-600 hover:bg-blue-50 dark:hover:bg-blue-950"
                      >
                        <Pencil className="h-4 w-4" />
                      </button>
                      <button
                        onClick={() => {
                          if (window.confirm(`Remover professor ${p.nome}?`)) deleteMutation.mutate(p.code);
                        }}
                        className="rounded p-1 text-rose-600 hover:bg-rose-50 dark:hover:bg-rose-950"
                      >
                        <Trash2 className="h-4 w-4" />
                      </button>
                    </>
                  )}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {editing && (
        <ProfessorModal
          prof={editing}
          isNew={creating}
          error={modalError}
          saving={saveMutation.isPending}
          onClose={() => {
            setEditing(null);
            setCreating(false);
            setModalError("");
          }}
          onSave={(data) => saveMutation.mutate({ ...data, isNew: creating })}
        />
      )}

      {showBatchDialog && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
          <div className="w-full max-w-md rounded-xl bg-white p-5 shadow-2xl dark:bg-slate-900">
            <div className="flex items-center justify-between border-b border-slate-200 pb-3 dark:border-slate-800">
              <h3 className="text-base font-bold">Gerar Acessos em Lote</h3>
              <button onClick={() => setShowBatchDialog(false)}>
                <X className="h-5 w-5" />
              </button>
            </div>
            <p className="mt-3 text-sm text-slate-600 dark:text-slate-300">
              Escolha como gerar as contas e senhas iniciais temporárias para os professores deste projeto:
            </p>
            <div className="mt-4 space-y-2">
              <button
                type="button"
                disabled={provisionBatchMutation.isPending}
                onClick={() => provisionBatchMutation.mutate(true)}
                className="w-full rounded-lg bg-blue-600 px-4 py-2.5 text-left text-sm font-semibold text-white hover:bg-blue-700 disabled:opacity-50"
              >
                Gerar apenas para professores sem acesso
                <span className="block text-xs font-normal text-blue-100">
                  Mantém senhas existentes intactas.
                </span>
              </button>
              <button
                type="button"
                disabled={provisionBatchMutation.isPending}
                onClick={() => {
                  if (window.confirm("Isso irá redefinir a senha inicial de TODOS os professores do projeto. Continuar?")) {
                    provisionBatchMutation.mutate(false);
                  }
                }}
                className="w-full rounded-lg border border-amber-300 bg-amber-50 px-4 py-2.5 text-left text-sm font-semibold text-amber-900 hover:bg-amber-100 disabled:opacity-50 dark:border-amber-800 dark:bg-amber-950/40 dark:text-amber-200"
              >
                Gerar / redefinir senha de todos os professores
                <span className="block text-xs font-normal opacity-80">
                  Exige troca de senha no próximo acesso de cada professor.
                </span>
              </button>
            </div>
          </div>
        </div>
      )}

      {credentialsModal && (
        <CredentialsModal
          title={credentialsModal.title}
          subtitle={credentialsModal.subtitle}
          items={credentialsModal.items}
          onClose={() => setCredentialsModal(null)}
        />
      )}
    </div>
  );
}

function CredentialsModal({
  title,
  subtitle,
  items,
  onClose,
}: {
  title: string;
  subtitle?: string;
  items: ProfessorCredential[];
  onClose: () => void;
}) {
  const [copiedKey, setCopiedKey] = useState<string | null>(null);

  const copyText = async (key: string, text: string) => {
    await navigator.clipboard.writeText(text);
    setCopiedKey(key);
    setTimeout(() => setCopiedKey(null), 2000);
  };

  const copyAll = () => {
    const text = items
      .map(
        (c) =>
          `${c.code} - ${c.nome}\nLogin: ${c.email}\nSenha inicial: ${c.initial_password || "(conta já existente)"}`
      )
      .join("\n\n");
    copyText("all", text);
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
      <div className="max-h-[90vh] w-full max-w-2xl overflow-y-auto rounded-xl bg-white p-5 shadow-2xl dark:bg-slate-900">
        <div className="flex items-center justify-between border-b border-slate-200 pb-3 dark:border-slate-800">
          <div>
            <h3 className="text-base font-bold">{title}</h3>
            {subtitle && <p className="text-xs text-slate-500">{subtitle}</p>}
          </div>
          <button onClick={onClose}>
            <X className="h-5 w-5" />
          </button>
        </div>

        {items.length === 0 ? (
          <p className="py-6 text-center text-sm text-slate-500">
            Nenhum professor precisou de geração/redefinição de acesso.
          </p>
        ) : (
          <div className="mt-4 space-y-3">
            {items.map((c) => (
              <div
                key={c.code}
                className="rounded-lg border border-slate-200 bg-slate-50 p-3 text-sm dark:border-slate-800 dark:bg-slate-800/50"
              >
                <div className="flex flex-wrap items-center justify-between gap-2">
                  <div>
                    <span className="font-mono text-xs font-bold text-blue-600 dark:text-blue-400">{c.code}</span>{" "}
                    <span className="font-semibold">{c.nome}</span>
                  </div>
                  {c.initial_password && (
                    <button
                      type="button"
                      onClick={() =>
                        copyText(c.code, `Login: ${c.email}\nSenha inicial: ${c.initial_password}`)
                      }
                      className="inline-flex items-center gap-1 rounded border border-slate-300 bg-white px-2.5 py-1 text-xs font-medium hover:bg-slate-100 dark:border-slate-700 dark:bg-slate-900"
                    >
                      {copiedKey === c.code ? <Check className="h-3.5 w-3.5 text-emerald-600" /> : <Copy className="h-3.5 w-3.5" />}
                      {copiedKey === c.code ? "Copiado" : "Copiar"}
                    </button>
                  )}
                </div>
                <div className="mt-2 grid gap-2 text-xs sm:grid-cols-2">
                  <div>
                    <span className="text-slate-500">E-mail / Login: </span>
                    <span className="font-mono font-semibold">{c.email}</span>
                  </div>
                  <div>
                    <span className="text-slate-500">Senha inicial: </span>
                    <span className="rounded bg-white px-1.5 py-0.5 font-mono font-bold text-emerald-700 dark:bg-slate-900 dark:text-emerald-400">
                      {c.initial_password || "Não alterada"}
                    </span>
                  </div>
                </div>
                {c.delivery_note && (
                  <p className="mt-1.5 text-[11px] text-slate-500">{c.delivery_note}</p>
                )}
              </div>
            ))}
          </div>
        )}

        <div className="mt-5 flex justify-end gap-2">
          {items.length > 0 && (
            <button
              type="button"
              onClick={copyAll}
              className="inline-flex items-center gap-1.5 rounded-md border border-slate-300 px-4 py-2 text-sm font-medium hover:bg-slate-50 dark:border-slate-700 dark:hover:bg-slate-800"
            >
              {copiedKey === "all" ? <Check className="h-4 w-4 text-emerald-600" /> : <Copy className="h-4 w-4" />}
              {copiedKey === "all" ? "Tudo copiado" : "Copiar Tudo"}
            </button>
          )}
          <button
            type="button"
            onClick={onClose}
            className="rounded-md bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-700"
          >
            Fechar
          </button>
        </div>
      </div>
    </div>
  );
}

function ProfessorModal({
  prof,
  isNew,
  error,
  saving,
  onClose,
  onSave,
}: {
  prof: Professor;
  isNew: boolean;
  error: string;
  saving: boolean;
  onClose: () => void;
  onSave: (p: Professor) => void;
}) {
  const [form, setForm] = useState<Professor>(prof);

  const toggleTurno = (day: DayOfWeek, shift: string) => {
    const current = form.turnos_pretendidos?.[day] || [];
    const next = current.includes(shift)
      ? current.filter((s) => s !== shift)
      : SHIFTS.map((s) => s.key).filter((s) => s === shift || current.includes(s));
    setForm({
      ...form,
      turnos_pretendidos: { ...form.turnos_pretendidos, [day]: next },
    });
  };

  const applyTurnoPreset = (selected: string[]) => {
    setForm({
      ...form,
      turnos_pretendidos: Object.fromEntries(ALL_DAYS.map((d) => [d, [...selected]])),
    });
  };

  const updateDisp = (day: DayOfWeek, idx: number, field: keyof TimeSlot, val: string) => {
    const list = [...(form.disponibilidade[day] || [])];
    list[idx] = { ...list[idx], [field]: val };
    setForm({ ...form, disponibilidade: { ...form.disponibilidade, [day]: list } });
  };

  const addDisp = (day: DayOfWeek) => {
    const list = [...(form.disponibilidade[day] || []), { inicio: "08:00", fim: "12:00" }];
    setForm({ ...form, disponibilidade: { ...form.disponibilidade, [day]: list } });
  };

  const removeDisp = (day: DayOfWeek, idx: number) => {
    const list = (form.disponibilidade[day] || []).filter((_, i) => i !== idx);
    setForm({ ...form, disponibilidade: { ...form.disponibilidade, [day]: list } });
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/50 p-4">
      <div className="max-h-[90vh] w-full max-w-2xl overflow-y-auto rounded-xl bg-white p-5 shadow-2xl dark:bg-slate-900">
        <div className="flex items-center justify-between border-b border-slate-200 pb-3 dark:border-slate-800">
          <h3 className="text-base font-bold">{isNew ? "Novo Professor" : `Editar ${form.nome}`}</h3>
          <button onClick={onClose}><X className="h-5 w-5" /></button>
        </div>

        <div className="mt-4 space-y-4 text-sm">
          <div className="grid grid-cols-3 gap-3">
            <div>
              <label className="text-xs font-medium text-slate-500">ID</label>
              <input
                disabled={!isNew}
                value={form.code}
                onChange={(e) => setForm({ ...form, code: e.target.value })}
                className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5 disabled:opacity-60 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
            <div className="col-span-2">
              <label className="text-xs font-medium text-slate-500">Nome Completo</label>
              <input
                value={form.nome}
                onChange={(e) => setForm({ ...form, nome: e.target.value })}
                className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
          </div>

          <div>
            <label className="text-xs font-medium text-slate-500">
              E-mail de acesso (opcional — se vazio, gera login automático)
            </label>
            <input
              type="email"
              value={form.email || ""}
              onChange={(e) => setForm({ ...form, email: e.target.value })}
              placeholder="professor@escola.edu.br"
              className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5 dark:border-slate-700 dark:bg-slate-800"
            />
          </div>

          <div className="grid grid-cols-3 gap-3">
            <div>
              <label className="text-xs font-medium text-slate-500">Aversão a Janelas ({form.peso_janela})</label>
              <input
                type="range"
                min={0}
                max={10}
                value={form.peso_janela}
                onChange={(e) => setForm({ ...form, peso_janela: Number(e.target.value) })}
                className="mt-2 w-full"
              />
            </div>
            <div>
              <label className="text-xs font-medium text-slate-500">Carga Máx. Diária (min)</label>
              <input
                type="number"
                min={0}
                value={form.carga_maxima_diaria_minutos}
                onChange={(e) => setForm({ ...form, carga_maxima_diaria_minutos: Number(e.target.value) })}
                className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
            <div>
              <label className="text-xs font-medium text-slate-500">Máx. Aulas/Dia</label>
              <input
                type="number"
                min={0}
                value={form.max_aulas_por_dia}
                onChange={(e) => setForm({ ...form, max_aulas_por_dia: Number(e.target.value) })}
                className="mt-1 w-full rounded border border-slate-300 px-3 py-1.5 dark:border-slate-700 dark:bg-slate-800"
              />
            </div>
          </div>

          <div>
            <div className="mb-1.5 flex flex-wrap items-center justify-between gap-2">
              <span className="text-xs font-semibold uppercase text-slate-500">Turnos Pretendidos por Dia</span>
              <div className="flex gap-1.5 text-[11px]">
                <button
                  type="button"
                  onClick={() => applyTurnoPreset(["MANHA", "TARDE", "NOITE"])}
                  className="rounded border border-slate-300 px-2 py-0.5 hover:bg-slate-50 dark:border-slate-700 dark:hover:bg-slate-800"
                >
                  Todos
                </button>
                <button
                  type="button"
                  onClick={() => applyTurnoPreset(["MANHA"])}
                  className="rounded border border-slate-300 px-2 py-0.5 hover:bg-slate-50 dark:border-slate-700 dark:hover:bg-slate-800"
                >
                  Manhã
                </button>
                <button
                  type="button"
                  onClick={() => applyTurnoPreset(["MANHA", "TARDE"])}
                  className="rounded border border-slate-300 px-2 py-0.5 hover:bg-slate-50 dark:border-slate-700 dark:hover:bg-slate-800"
                >
                  Manhã+Tarde
                </button>
              </div>
            </div>
            <div className="overflow-x-auto rounded border border-slate-200 dark:border-slate-700">
              <table className="w-full text-center text-xs">
                <thead className="bg-slate-50 dark:bg-slate-800/60">
                  <tr>
                    <th className="px-3 py-1.5 text-left">Dia</th>
                    {SHIFTS.map((s) => (
                      <th key={s.key} className="px-3 py-1.5">{s.label}</th>
                    ))}
                  </tr>
                </thead>
                <tbody className="divide-y divide-slate-200 dark:divide-slate-800">
                  {ALL_DAYS.map((d) => (
                    <tr key={d}>
                      <td className="px-3 py-1.5 text-left font-semibold">{DAY_SHORT_LABELS[d]}</td>
                      {SHIFTS.map((s) => (
                        <td key={s.key} className="px-3 py-1.5">
                          <input
                            type="checkbox"
                            checked={(form.turnos_pretendidos?.[d] || []).includes(s.key)}
                            onChange={() => toggleTurno(d, s.key)}
                            className="h-3.5 w-3.5 rounded"
                          />
                        </td>
                      ))}
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>

          <div>
            <div className="mb-1.5 text-xs font-semibold uppercase text-slate-500">Preferência por Dia (0 = Menos, 10 = Muito)</div>
            <div className="grid grid-cols-5 gap-2">
              {ALL_DAYS.map((d) => (
                <div key={d} className="rounded border border-slate-200 p-2 text-center dark:border-slate-700">
                  <div className="text-xs font-semibold">{DAY_SHORT_LABELS[d]} ({form.preferencia_dias[d] ?? 5})</div>
                  <input
                    type="range"
                    min={0}
                    max={10}
                    value={form.preferencia_dias[d] ?? 5}
                    onChange={(e) =>
                      setForm({
                        ...form,
                        preferencia_dias: { ...form.preferencia_dias, [d]: Number(e.target.value) },
                      })
                    }
                    className="mt-1 w-full"
                  />
                </div>
              ))}
            </div>
          </div>

          <div>
            <div className="mb-1.5 text-xs font-semibold uppercase text-slate-500">Disponibilidade por Dia</div>
            <div className="space-y-2">
              {ALL_DAYS.map((d) => (
                <div key={d} className="rounded border border-slate-200 p-2 dark:border-slate-800">
                  <div className="flex items-center justify-between">
                    <span className="text-xs font-bold">{DAY_SHORT_LABELS[d]}</span>
                    <button type="button" onClick={() => addDisp(d)} className="text-xs text-blue-600 hover:underline">
                      + Faixa
                    </button>
                  </div>
                  <div className="mt-1 flex flex-wrap gap-2">
                    {(form.disponibilidade[d] || []).map((slot, idx) => (
                      <div key={idx} className="flex items-center gap-1 rounded bg-slate-100 px-2 py-1 dark:bg-slate-800">
                        <input type="time" value={slot.inicio} onChange={(e) => updateDisp(d, idx, "inicio", e.target.value)} className="bg-transparent text-xs" />
                        <span>-</span>
                        <input type="time" value={slot.fim} onChange={(e) => updateDisp(d, idx, "fim", e.target.value)} className="bg-transparent text-xs" />
                        <button type="button" onClick={() => removeDisp(d, idx)} className="text-rose-500"><X className="h-3.5 w-3.5" /></button>
                      </div>
                    ))}
                  </div>
                </div>
              ))}
            </div>
          </div>
        </div>

        {error && (
          <div className="mt-3 rounded bg-rose-100 p-2 text-xs text-rose-700 dark:bg-rose-950 dark:text-rose-300">
            {error}
          </div>
        )}

        <div className="mt-5 flex justify-end gap-2">
          <button onClick={onClose} className="rounded-md border border-slate-300 px-4 py-2 text-sm dark:border-slate-700">Cancelar</button>
          <button
            disabled={!form.nome.trim() || saving}
            onClick={() => onSave(form)}
            className="rounded-md bg-blue-600 px-4 py-2 text-sm font-medium text-white hover:bg-blue-700 disabled:opacity-50"
          >
            {saving ? "Salvando..." : "Salvar"}
          </button>
        </div>
      </div>
    </div>
  );
}
