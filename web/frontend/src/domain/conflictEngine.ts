import type {
  AllocatedLesson,
  DayOfWeek,
  Disciplina,
  OptimizationConfig,
  Professor,
  Room,
  SlotConflictInfo,
  Turma,
} from "../types";
import { computeImplicitBreaks, DAY_LABELS, formatInterval, minToTime, timeToMin } from "./time";

export interface DomainSnapshot {
  professores: Record<string, Professor>;
  salas: Record<string, Room>;
  turmas: Record<string, Turma>;
  disciplinas: Record<string, Disciplina>;
  config: Partial<OptimizationConfig>;
  alocacoes: AllocatedLesson[];
}

function overlaps(s1: number, e1: number, s2: number, e2: number): boolean {
  return Math.max(s1, s2) < Math.min(e1, e2);
}

function semestersConflict(a1: AllocatedLesson, a2: AllocatedLesson): boolean {
  if (a1.regime === "ANUAL" || a2.regime === "ANUAL") return true;
  const s1 = a1.semestre === "2" || a1.semestre_alocado === 2 ? 2 : 1;
  const s2 = a2.semestre === "2" || a2.semestre_alocado === 2 ? 2 : 1;
  return s1 === s2;
}

function formatTurmaNames(snap: DomainSnapshot, lesson: AllocatedLesson): string {
  const names = lesson.turmas.map((id) => snap.turmas[id]?.nome || id);
  return names.length ? names.join(", ") : lesson.disciplina_nome;
}

function getTurmaSlotsForDay(turma: Turma, dia: DayOfWeek) {
  const exc = turma.excecoes_por_dia?.[dia];
  if (exc && Array.isArray(exc.slots)) return exc.slots;
  return turma.jornada_padrao?.slots || [];
}

function isProfAvailable(prof: Professor, dia: DayOfWeek, startMin: number, endMin: number): boolean {
  const intervals = prof.disponibilidade?.[dia] || [];
  return intervals.some((inter) => startMin >= timeToMin(inter.inicio) && endMin <= timeToMin(inter.fim));
}

export function evaluateLessonPlacement(
  snap: DomainSnapshot,
  target: AllocatedLesson,
  ignoreIds: Set<string> = new Set()
): SlotConflictInfo[] {
  const result: SlotConflictInfo[] = [];
  const tDia = target.dia;
  const tStart = target.start_min;
  const tEnd = target.end_min;
  const tStr = formatInterval(tStart, tEnd);
  const diaLong = DAY_LABELS[tDia] || tDia;

  for (const tId of target.turmas) {
    const t = snap.turmas[tId];
    const tNome = t?.nome || tId;
    if (t) {
      if (!t.dias_letivos.includes(tDia)) {
        result.push({
          type: "TURMA_INATIVA",
          short_label: "Turma inativa",
          detail: `Turma inativa: A Turma '${tNome}' não funciona na ${diaLong}.`,
          related_aula_id: "",
        });
      }
      const slots = getTurmaSlotsForDay(t, tDia);
      for (const br of computeImplicitBreaks(slots)) {
        if (overlaps(br.start_min, br.end_min, tStart, tEnd)) {
          result.push({
            type: "INTERVALO_RECREIO",
            short_label: "Intervalo",
            detail: `Intervalo de recreio: Sobrepõe o recreio (${formatInterval(br.start_min, br.end_min)}) da Turma '${tNome}'.`,
            related_aula_id: "",
          });
        }
      }
      if (slots.length === 0) {
        result.push({
          type: "FORA_JORNADA",
          short_label: "Fora da jornada",
          detail: `Fora da jornada: A Turma '${tNome}' não possui slots cadastrados na ${diaLong}.`,
          related_aula_id: "",
        });
      } else {
        const ordered = slots
          .map((s) => [timeToMin(s.inicio), timeToMin(s.fim)] as const)
          .sort((a, b) => a[0] - b[0]);
        const jStart = ordered[0][0];
        const jEnd = ordered[ordered.length - 1][1];
        if (tStart < jStart || tEnd > jEnd) {
          result.push({
            type: "FORA_JORNADA",
            short_label: "Fora da jornada",
            detail: `Fora da jornada: O horário ultrapassa a jornada letiva da Turma '${tNome}' (${minToTime(jStart)} - ${minToTime(jEnd)}).`,
            related_aula_id: "",
          });
        } else if (!ordered.some(([s, e]) => s === tStart && e === tEnd)) {
          result.push({
            type: "SLOT_INCOMPATIVEL",
            short_label: "Slot incompatível",
            detail: `Slot incompatível: O horário (${tStr}) não corresponde a um slot cadastrado na Turma '${tNome}'.`,
            related_aula_id: "",
          });
        }
      }
    }

    for (const other of snap.alocacoes) {
      if (ignoreIds.has(other.aula_id)) continue;
      if (
        other.dia === tDia &&
        semestersConflict(target, other) &&
        overlaps(tStart, tEnd, other.start_min, other.end_min) &&
        other.turmas.includes(tId)
      ) {
        result.push({
          type: "CHOQUE_TURMA",
          short_label: `Turma ocupada (${tNome})`,
          detail: `Choque de Turma: Conflita com a aula '${other.disciplina_nome}' (${other.aula_id}) da Turma '${tNome}' neste mesmo horário.`,
          related_aula_id: other.aula_id,
        });
      }
    }

    if (snap.config.distribuir_disciplinas_na_semana ?? true) {
      const disc = snap.disciplinas[target.disciplina_id];
      const maxLim =
        disc && disc.max_aulas_por_dia_turma > 0
          ? disc.max_aulas_por_dia_turma
          : (snap.config.max_aulas_disciplina_dia_turma ?? 2);
      if (maxLim > 0) {
        let otherCount = 0;
        for (const other of snap.alocacoes) {
          if (ignoreIds.has(other.aula_id)) continue;
          if (
            other.dia === tDia &&
            other.disciplina_id === target.disciplina_id &&
            semestersConflict(target, other) &&
            other.turmas.includes(tId)
          ) {
            otherCount++;
          }
        }
        if (otherCount + 1 > maxLim) {
          result.push({
            type: "LIMITE_DIARIO_DISCIPLINA",
            short_label: `Limite diário (${tNome})`,
            detail: `Limite diário excedido: ${otherCount + 1} aulas da disciplina alocadas hoje para a Turma '${tNome}' (máximo permitido: ${maxLim}).`,
            related_aula_id: "",
          });
        }
      }
    }
  }

  for (const pId of target.professores) {
    const p = snap.professores[pId];
    const pNome = p?.nome || pId;
    if (p && !isProfAvailable(p, tDia, tStart, tEnd)) {
      result.push({
        type: "INDISPONIBILIDADE_PROFESSOR",
        short_label: "Prof. indisponível",
        detail: `Indisponibilidade docente: O Prof. '${pNome}' possui indisponibilidade cadastrada neste horário.`,
        related_aula_id: "",
      });
    }
    for (const other of snap.alocacoes) {
      if (ignoreIds.has(other.aula_id)) continue;
      if (
        other.dia === tDia &&
        semestersConflict(target, other) &&
        overlaps(tStart, tEnd, other.start_min, other.end_min) &&
        other.professores.includes(pId)
      ) {
        const ctx = formatTurmaNames(snap, other);
        result.push({
          type: "CHOQUE_PROFESSOR",
          short_label: `Prof. ocupado (${ctx})`,
          detail: `Choque de Professor: O Prof. '${pNome}' já ministra a aula '${other.disciplina_nome}' (${ctx}, ${other.aula_id}) neste mesmo horário.`,
          related_aula_id: other.aula_id,
        });
      }
    }
  }

  for (const sId of target.salas) {
    if (!sId) continue;
    const s = snap.salas[sId];
    const sNome = s?.nome || sId;
    if (target.tipo === "LABORATORIO" && s && s.tipo.toUpperCase() === "TEORICA") {
      result.push({
        type: "INCOMPATIBILIDADE_SALA",
        short_label: "Sala incompatível (Lab)",
        detail: `Incompatibilidade de sala: A aula requer Laboratório, mas a Sala '${sNome}' é Teórica.`,
        related_aula_id: "",
      });
    }
    for (const other of snap.alocacoes) {
      if (ignoreIds.has(other.aula_id)) continue;
      if (
        other.dia === tDia &&
        semestersConflict(target, other) &&
        overlaps(tStart, tEnd, other.start_min, other.end_min) &&
        other.salas.includes(sId)
      ) {
        const ctx = formatTurmaNames(snap, other);
        result.push({
          type: "CHOQUE_SALA",
          short_label: `Sala ocupada (${ctx})`,
          detail: `Choque de Sala: A Sala '${sNome}' já está ocupada pela aula '${other.disciplina_nome}' (${ctx}, ${other.aula_id}) neste mesmo horário.`,
          related_aula_id: other.aula_id,
        });
      }
    }
  }

  return result;
}

export function evaluateLessonAtSlot(
  snap: DomainSnapshot,
  aulaId: string,
  dia: DayOfWeek,
  startMin: number,
  endMin: number,
  ignoreAulaIds: string[] = [],
  salasOverride?: string[]
): SlotConflictInfo[] {
  const existing = snap.alocacoes.find((a) => a.aula_id === aulaId);
  if (!existing) return [];
  const target: AllocatedLesson = {
    ...existing,
    dia,
    start_min: startMin,
    end_min: endMin,
    inicio: minToTime(startMin),
    fim: minToTime(endMin),
    salas: salasOverride && salasOverride.length > 0 ? salasOverride : existing.salas,
  };
  const ignores = new Set(ignoreAulaIds);
  ignores.add(aulaId);
  return evaluateLessonPlacement(snap, target, ignores);
}

export function evaluateLessonSwap(
  snap: DomainSnapshot,
  aulaAId: string,
  aulaBId: string
): SlotConflictInfo[] {
  if (aulaAId === aulaBId) return [];
  const a = snap.alocacoes.find((x) => x.aula_id === aulaAId);
  const b = snap.alocacoes.find((x) => x.aula_id === aulaBId);
  if (!a || !b) return [];

  const conflicts: SlotConflictInfo[] = [];
  const durA = a.end_min - a.start_min;
  const durB = b.end_min - b.start_min;
  if (durA !== durB) {
    conflicts.push({
      type: "DURACAO_INCOMPATIVEL",
      short_label: "Duração diferente",
      detail: `Duração incompatível: '${a.disciplina_nome}' (${durA} min) vs '${b.disciplina_nome}' (${durB} min).`,
      related_aula_id: aulaBId,
    });
  }

  const ignores = [aulaAId, aulaBId];
  conflicts.push(...evaluateLessonAtSlot(snap, aulaAId, b.dia, b.start_min, b.start_min + durA, ignores));
  const confB = evaluateLessonAtSlot(snap, aulaBId, a.dia, a.start_min, a.start_min + durB, ignores);
  for (const c of confB) {
    conflicts.push({ ...c, detail: `[Na origem] ${b.disciplina_nome}: ${c.detail}` });
  }
  return conflicts;
}
