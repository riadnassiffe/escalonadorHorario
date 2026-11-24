import { useAuthStore } from "../store/authStore";

const API_BASE = "/api/v1";

async function refreshAccessToken(): Promise<string | null> {
  const { refreshToken, setAuth, setTokens, logout } = useAuthStore.getState();
  if (!refreshToken) return null;
  try {
    const res = await fetch(`${API_BASE}/auth/refresh`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ refresh_token: refreshToken }),
    });
    if (!res.ok) {
      logout();
      return null;
    }
    const data = await res.json();
    if (data.user) {
      setAuth(data.access_token, data.refresh_token, data.user);
    } else {
      setTokens(data.access_token, data.refresh_token);
    }
    return data.access_token;
  } catch {
    logout();
    return null;
  }
}

export async function apiFetch<T>(path: string, init: RequestInit = {}): Promise<T> {
  const state = useAuthStore.getState();
  const headers = new Headers(init.headers || {});
  if (!headers.has("Content-Type") && !(init.body instanceof FormData)) {
    headers.set("Content-Type", "application/json");
  }
  if (state.accessToken) {
    headers.set("Authorization", `Bearer ${state.accessToken}`);
  }

  let res = await fetch(`${API_BASE}${path}`, { ...init, headers });
  if (res.status === 401 && state.refreshToken) {
    const nextToken = await refreshAccessToken();
    if (nextToken) {
      headers.set("Authorization", `Bearer ${nextToken}`);
      res = await fetch(`${API_BASE}${path}`, { ...init, headers });
    }
  }

  if (!res.ok) {
    let msg = `Erro HTTP ${res.status}`;
    try {
      const errBody = await res.json();
      msg = errBody.detail || msg;
    } catch {
      // ignore
    }
    throw new Error(msg);
  }

  if (res.status === 204) {
    return undefined as T;
  }
  return res.json() as Promise<T>;
}

export async function apiDownload(path: string, filename: string): Promise<void> {
  const { accessToken } = useAuthStore.getState();
  const res = await fetch(`${API_BASE}${path}`, {
    headers: accessToken ? { Authorization: `Bearer ${accessToken}` } : {},
  });
  if (!res.ok) throw new Error(`Falha ao baixar arquivo (${res.status})`);
  const blob = await res.blob();
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
  URL.revokeObjectURL(url);
}
