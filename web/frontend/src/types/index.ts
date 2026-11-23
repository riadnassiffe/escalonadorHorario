export type DayOfWeek = "SEG" | "TER" | "QUA" | "QUI" | "SEX";
export type ShiftType = "MANHA" | "TARDE" | "NOITE" | "INTEGRAL";
export type RegimeType = "ANUAL" | "SEMESTRAL";
export type SemestreType = "1" | "2" | "AMBOS";

export interface TimeSlot {
  inicio: string;
  fim: string;
}

export interface JornadaTurma {
  slots: TimeSlot[];
}

export interface User {
  id: string;
  institution_id: string;
  institution_name: string;
  email: string;
  full_name: string;
  role: "ADMIN" | "COORDENADOR" | "VISUALIZADOR";
  is_active: boolean;
  created_at: string;
}

export interface Project {
  id: string;
  institution_id: string;
  name: string;
  description: string;
  academic_year: string;
  active_timetable_version_id: string | null;
  created_at: string;
  updated_at: string;
  counts: {
    professores: number;
    salas: number;
    turmas: number;
    disciplinas: number;
  };
}

export interface Professor {
  id: string;
  code: string;
  nome: string;
  preferencia_dias: Record<string, number>;
  disponibilidade: Record<string, TimeSlot[]>;
  turnos_pretendidos: Record<string, string[]>;
  carga_maxima_diaria_minutos: number;
  max_aulas_por_dia: number;
  peso_janela: number;
}

export interface Room {
  id: string;
  code: string;
  nome: string;
  tipo: string;
  capacidade: number;
  recursos: string[];
}

export interface Turma {
  id: string;
  code: string;
  nome: string;
  turno: string;
  dias_letivos: string[];
  jornada_padrao: JornadaTurma;
  excecoes_por_dia: Record<string, JornadaTurma>;
  sala_teorica_id: string | null;
  salas_teoricas_permitidas: string[];
}

export interface Disciplina {
  id: string;
  code: string;
  nome: string;
  tipo: string;
  duracao_minutos: number;
  aulas_semanais: number;
  regime: string;
  semestre: string;
  professores: string[];
  turmas: string[];
  turnos_permitidos: string[];
  salas_alternativas: string[][];
  exigir_aulas_consecutivas: boolean;
  max_aulas_por_dia_turma: number;
}

export interface OptimizationConfig {
  algoritmo: string;
  escopo_semestre: string;
  sincronizar_anuais_entre_semestres: boolean;
  lambda_janelas: number;
  lambda_dias: number;
  lambda_preferencias: number;
  lambda_preferencia_dias: number;
  lambda_preferencia_turnos: number;
  lambda_lacunas_turma: number;
  modelo_preferencia_dias: string;
  preenchimento_integral_jornada: boolean;
  distribuir_disciplinas_na_semana: boolean;
  max_aulas_disciplina_dia_turma: number;
  tempo_limite_segundos: number;
  sem_limite_tempo: boolean;
  max_memoria_mb: number;
  sem_limite_memoria: boolean;
  num_threads: number;
  log_search_progress: boolean;
  usar_distribuicao_como_entrada: boolean;
}

export type SlotConflictType =
  | "TURMA_INATIVA"
  | "INTERVALO_RECREIO"
  | "FORA_JORNADA"
  | "SLOT_INCOMPATIVEL"
  | "INDISPONIBILIDADE_PROFESSOR"
  | "INCOMPATIBILIDADE_SALA"
  | "LIMITE_DIARIO_DISCIPLINA"
  | "CHOQUE_TURMA"
  | "CHOQUE_PROFESSOR"
  | "CHOQUE_SALA"
  | "DURACAO_INCOMPATIVEL";

export interface SlotConflictInfo {
  type: SlotConflictType;
  short_label: string;
  detail: string;
  related_aula_id: string;
}

export interface AllocatedLesson {
  aula_id: string;
  disciplina_id: string;
  disciplina_nome: string;
  tipo: string;
  dia: DayOfWeek;
  inicio: string;
  fim: string;
  start_min: number;
  end_min: number;
  professores: string[];
  turmas: string[];
  salas: string[];
  is_pinned: boolean;
  regime: string;
  semestre: string;
  semestre_alocado: number;
}

export interface TimetableResponse {
  version_id: string | null;
  version_number: number | null;
  label: string | null;
  alocacoes: AllocatedLesson[];
  salas_teoricas_designadas: Record<string, string>;
  estatisticas: Record<string, any>;
  mensagens_diagnostico: string[];
  conflitos: Record<string, SlotConflictInfo[]>;
}

export interface TimetableVersionSummary {
  id: string;
  version_number: number;
  label: string;
  estatisticas: Record<string, any>;
  created_at: string;
  is_active: boolean;
}

export interface OptimizationJob {
  id: string;
  project_id: string;
  celery_task_id: string | null;
  status: "PENDING" | "RUNNING" | "COMPLETED" | "FAILED" | "INFEASIBLE" | "CANCELLED";
  algorithm: string;
  config_snapshot: Record<string, any>;
  progress_ratio: number;
  progress_message: string;
  logs: string[];
  result_version_id: string | null;
  error_detail: string | null;
  created_at: string;
  started_at: string | null;
  finished_at: string | null;
}

export interface UnallocatedLesson {
  aula_id: string;
  disciplina_id: string;
  disciplina_nome: string;
  tipo: string;
  duracao_minutos: number;
  regime: string;
  semestre: string;
  professores: string[];
  turmas: string[];
}
