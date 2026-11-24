import { useEffect } from "react";
import { BrowserRouter, Navigate, Route, Routes } from "react-router-dom";
import { QueryClient, QueryClientProvider } from "@tanstack/react-query";
import { useAuthStore } from "./store/authStore";
import { useUiStore } from "./store/uiStore";
import { LoginPage } from "./pages/LoginPage";
import { ForgotPasswordPage } from "./pages/ForgotPasswordPage";
import { ResetPasswordPage } from "./pages/ResetPasswordPage";
import { ChangePasswordPage } from "./pages/ChangePasswordPage";
import { ProfessorPortalPage } from "./pages/ProfessorPortalPage";
import { ProjectsDashboardPage } from "./pages/ProjectsDashboardPage";
import { ProjectWorkspacePage } from "./pages/ProjectWorkspacePage";

const queryClient = new QueryClient({
  defaultOptions: {
    queries: { retry: 1, staleTime: 5000 },
  },
});

function ProtectedRoute({
  children,
  allowPendingPasswordChange = false,
  nonProfessorOnly = false,
}: {
  children: React.ReactNode;
  allowPendingPasswordChange?: boolean;
  nonProfessorOnly?: boolean;
}) {
  const token = useAuthStore((s) => s.accessToken);
  const user = useAuthStore((s) => s.user);
  if (!token) return <Navigate to="/login" replace />;
  if (user?.must_change_password && !allowPendingPasswordChange) {
    return <Navigate to="/change-password" replace />;
  }
  if (nonProfessorOnly && user?.role === "PROFESSOR") {
    return <Navigate to="/professor" replace />;
  }
  return <>{children}</>;
}

export function App() {
  const darkMode = useUiStore((s) => s.darkMode);

  useEffect(() => {
    document.documentElement.classList.toggle("dark", darkMode);
  }, [darkMode]);

  return (
    <QueryClientProvider client={queryClient}>
      <BrowserRouter>
        <Routes>
          <Route path="/login" element={<LoginPage />} />
          <Route path="/forgot-password" element={<ForgotPasswordPage />} />
          <Route path="/reset-password" element={<ResetPasswordPage />} />
          <Route
            path="/change-password"
            element={
              <ProtectedRoute allowPendingPasswordChange>
                <ChangePasswordPage />
              </ProtectedRoute>
            }
          />
          <Route
            path="/professor"
            element={
              <ProtectedRoute>
                <ProfessorPortalPage />
              </ProtectedRoute>
            }
          />
          <Route
            path="/"
            element={
              <ProtectedRoute nonProfessorOnly>
                <ProjectsDashboardPage />
              </ProtectedRoute>
            }
          />
          <Route
            path="/projects/:projectId"
            element={
              <ProtectedRoute nonProfessorOnly>
                <ProjectWorkspacePage />
              </ProtectedRoute>
            }
          />
          <Route path="*" element={<Navigate to="/" replace />} />
        </Routes>
      </BrowserRouter>
    </QueryClientProvider>
  );
}

export default App;
