import type { Disciplina, Room, Turma, UnallocatedLesson } from "../types";

export type AllocationViewPerspective = "TURMA" | "PROFESSOR" | "SALA" | "GERAL";

export interface AllocationViewContext {
  viewType: AllocationViewPerspective;
  targetId: string;
}

export type AllowedRoomsSource =
  | "EXPLICIT_DISCIPLINA"
  | "TURMA_FIXED"
  | "TURMA_PERMITTED"
  | "ALL_THEORETICAL_FALLBACK"
  | "LAB_OR_SPECIAL";

export interface AllowedRoomOption {
  key: string;
  salas: string[];
  label: string;
  isSuggested?: boolean;
}

export interface AllowedRoomsResolution {
  options: AllowedRoomOption[];
  source: AllowedRoomsSource;
  reasonLabel: string;
}

export interface TurmaRoomOption {
  room: Room;
  isFixed: boolean;
  turmaNames: string[];
}

export interface DisciplinaRoomOptions {
  turmaRooms: TurmaRoomOption[];
  labAndSpecialRooms: Room[];
  otherTheoreticalRooms: Room[];
  hasSelectedOtherRoom: boolean;
  turmaDefaultSummary: string;
}

export function flattenSalasAlternativas(salasAlternativas: string[][]): string[] {
  const seen = new Set<string>();
  const result: string[] = [];
  for (const combo of salasAlternativas || []) {
    for (const code of combo || []) {
      const trimmed = code.trim();
      if (trimmed && !seen.has(trimmed)) {
        seen.add(trimmed);
        result.push(trimmed);
      }
    }
  }
  return result;
}

export function toggleRoomInSalasAlternativas(
  salasAlternativas: string[][],
  roomCode: string
): string[][] {
  const isSelected = (salasAlternativas || []).some((combo) => combo.includes(roomCode));
  if (isSelected) {
    return (salasAlternativas || []).filter((combo) => !combo.includes(roomCode));
  }
  return [...(salasAlternativas || []), [roomCode]];
}

export function computeDisciplinaRoomOptions(
  selectedTurmaCodes: string[],
  turmas: Turma[],
  salas: Room[],
  selectedRoomCodes: string[]
): DisciplinaRoomOptions {
  const salasByCode = Object.fromEntries(salas.map((r) => [r.code, r])) as Record<string, Room>;
  const turmasByCode = Object.fromEntries(turmas.map((t) => [t.code, t])) as Record<string, Turma>;
  const theoreticalRooms = salas.filter((r) => r.tipo.toUpperCase() === "TEORICA");

  const turmaRoomMap = new Map<string, TurmaRoomOption>();

  const addTurmaRoom = (room: Room, isFixed: boolean, turmaName: string) => {
    const existing = turmaRoomMap.get(room.code);
    if (existing) {
      existing.isFixed = existing.isFixed || isFixed;
      if (!existing.turmaNames.includes(turmaName)) {
        existing.turmaNames.push(turmaName);
      }
      return;
    }
    turmaRoomMap.set(room.code, {
      room,
      isFixed,
      turmaNames: [turmaName],
    });
  };

  for (const tCode of selectedTurmaCodes) {
    const turma = turmasByCode[tCode];
    if (!turma) continue;

    if (turma.sala_teorica_id && salasByCode[turma.sala_teorica_id]) {
      addTurmaRoom(salasByCode[turma.sala_teorica_id], true, turma.nome);
    } else if (turma.salas_teoricas_permitidas && turma.salas_teoricas_permitidas.length > 0) {
      let addedAny = false;
      for (const sCode of turma.salas_teoricas_permitidas) {
        const room = salasByCode[sCode];
        if (room) {
          addTurmaRoom(room, false, turma.nome);
          addedAny = true;
        }
      }
      if (!addedAny) {
        for (const room of theoreticalRooms) {
          addTurmaRoom(room, false, turma.nome);
        }
      }
    } else {
      for (const room of theoreticalRooms) {
        addTurmaRoom(room, false, turma.nome);
      }
    }
  }

  const turmaRooms = Array.from(turmaRoomMap.values());
  const turmaRoomCodeSet = new Set(turmaRooms.map((item) => item.room.code));

  const labAndSpecialRooms = salas.filter(
    (r) => r.tipo.toUpperCase() !== "TEORICA" && !turmaRoomCodeSet.has(r.code)
  );
  const otherTheoreticalRooms = salas.filter(
    (r) => r.tipo.toUpperCase() === "TEORICA" && !turmaRoomCodeSet.has(r.code)
  );

  const selectedSet = new Set(selectedRoomCodes);
  const hasSelectedOtherRoom = otherTheoreticalRooms.some((r) => selectedSet.has(r.code));

  let turmaDefaultSummary: string;
  if (selectedTurmaCodes.length === 0) {
    turmaDefaultSummary = "Selecione ao menos uma turma para visualizar as salas vinculadas.";
  } else if (turmaRooms.length === 0) {
    turmaDefaultSummary = "Nenhuma sala teórica vinculada às turmas selecionadas.";
  } else {
    turmaDefaultSummary = turmaRooms
      .map((opt) => `${opt.room.nome} (${opt.room.code})`)
      .join(", ");
  }

  return {
    turmaRooms,
    labAndSpecialRooms,
    otherTheoreticalRooms,
    hasSelectedOtherRoom,
    turmaDefaultSummary,
  };
}

export function describeDisciplinaRooms(
  disc: Disciplina,
  turmasByCode: Record<string, Turma>,
  _salasByCode: Record<string, Room>
): { label: string; isExplicit: boolean } {
  const explicitCodes = flattenSalasAlternativas(disc.salas_alternativas);
  if (explicitCodes.length > 0) {
    return { label: explicitCodes.join(", "), isExplicit: true };
  }

  if (disc.tipo.toUpperCase() === "TEORICA") {
    const firstTurmaCode = disc.turmas[0];
    const turma = firstTurmaCode ? turmasByCode[firstTurmaCode] : undefined;
    if (turma?.sala_teorica_id) {
      return { label: `Turma: ${turma.sala_teorica_id}`, isExplicit: false };
    }
    if (turma && turma.salas_teoricas_permitidas.length > 0) {
      return {
        label: `Turma: Automática (${turma.salas_teoricas_permitidas.length})`,
        isExplicit: false,
      };
    }
    return { label: "Sala da turma", isExplicit: false };
  }

  return { label: "Qualquer lab/especial", isExplicit: false };
}

export function filterUnallocatedByViewContext(
  unallocated: UnallocatedLesson[],
  context?: AllocationViewContext
): UnallocatedLesson[] {
  if (!context || !context.targetId) {
    return unallocated;
  }
  if (context.viewType === "TURMA" || context.viewType === "GERAL") {
    return unallocated.filter((u) => u.turmas.includes(context.targetId));
  }
  if (context.viewType === "PROFESSOR") {
    return unallocated.filter((u) => u.professores.includes(context.targetId));
  }
  return unallocated;
}

function formatSingleRoomOptionLabel(room: Room): string {
  return `${room.nome} (${room.code} • ${room.tipo}, cap. ${room.capacidade})`;
}

export function getAllowedRoomsForLesson(
  lesson: Pick<UnallocatedLesson, "disciplina_id" | "tipo" | "turmas">,
  disciplinasByCode: Record<string, Disciplina>,
  turmasByCode: Record<string, Turma>,
  salasByCode: Record<string, Room>,
  designatedTheoreticalRooms?: Record<string, string>
): AllowedRoomsResolution {
  const disc = disciplinasByCode[lesson.disciplina_id];

  // 1. Salas explícitas na disciplina (substitui sala padrão da turma, espelhando Preprocessor.cpp)
  const explicitMap = new Map<string, AllowedRoomOption>();
  for (const combo of disc?.salas_alternativas || []) {
    const seenInCombo = new Set<string>();
    const validCodes: string[] = [];
    for (const rawCode of combo || []) {
      const code = rawCode.trim();
      if (code && salasByCode[code] && !seenInCombo.has(code)) {
        seenInCombo.add(code);
        validCodes.push(code);
      }
    }
    if (validCodes.length === 0) continue;
    const key = validCodes.join("+");
    if (!explicitMap.has(key)) {
      const label =
        validCodes.length === 1
          ? formatSingleRoomOptionLabel(salasByCode[validCodes[0]])
          : validCodes.map((c) => `${salasByCode[c].nome} (${c})`).join(" + ");
      explicitMap.set(key, { key, salas: validCodes, label });
    }
  }
  if (explicitMap.size > 0) {
    return {
      options: Array.from(explicitMap.values()),
      source: "EXPLICIT_DISCIPLINA",
      reasonLabel: "Salas específicas configuradas na disciplina.",
    };
  }

  const tipoUpper = (disc?.tipo || lesson.tipo || "").toUpperCase();
  const allSalas = Object.values(salasByCode);

  // 2. Aula teórica sem seleção explícita: respeitar sala fixa / permitidas das turmas
  if (tipoUpper === "TEORICA") {
    const turmaCodes = disc?.turmas?.length ? disc.turmas : lesson.turmas;
    const allTheoreticalRooms = allSalas.filter((r) => r.tipo.toUpperCase() === "TEORICA");
    const optionMap = new Map<string, AllowedRoomOption>();
    let usedFixed = false;
    let usedPermitted = false;
    let usedFallback = false;

    const addSingleRoom = (room: Room, isSuggested = false) => {
      const existing = optionMap.get(room.code);
      if (existing) {
        if (isSuggested) existing.isSuggested = true;
        return;
      }
      optionMap.set(room.code, {
        key: room.code,
        salas: [room.code],
        label: formatSingleRoomOptionLabel(room),
        isSuggested,
      });
    };

    for (const tCode of turmaCodes) {
      const turma = turmasByCode[tCode];
      if (!turma) continue;

      if (turma.sala_teorica_id && salasByCode[turma.sala_teorica_id]) {
        usedFixed = true;
        addSingleRoom(salasByCode[turma.sala_teorica_id]);
      } else if (turma.salas_teoricas_permitidas && turma.salas_teoricas_permitidas.length > 0) {
        const validPermitted = turma.salas_teoricas_permitidas
          .map((c) => salasByCode[c])
          .filter((r): r is Room => Boolean(r));
        const designated = designatedTheoreticalRooms?.[tCode];
        if (validPermitted.length > 0) {
          usedPermitted = true;
          for (const r of validPermitted) {
            addSingleRoom(r, designated === r.code);
          }
        } else {
          usedFallback = true;
          for (const r of allTheoreticalRooms) {
            addSingleRoom(r, designated === r.code);
          }
        }
      } else {
        usedFallback = true;
        const designated = designatedTheoreticalRooms?.[tCode];
        for (const r of allTheoreticalRooms) {
          addSingleRoom(r, designated === r.code);
        }
      }
    }

    if (optionMap.size === 0) {
      usedFallback = true;
      for (const r of allTheoreticalRooms) {
        addSingleRoom(r);
      }
    }

    let source: AllowedRoomsSource;
    let reasonLabel: string;
    if (usedFixed && !usedPermitted && !usedFallback) {
      source = "TURMA_FIXED";
      reasonLabel = "Sala teórica fixa vinculada à turma.";
    } else if (usedPermitted && !usedFallback) {
      source = "TURMA_PERMITTED";
      reasonLabel = "Salas teóricas permitidas para a turma.";
    } else {
      source = "ALL_THEORETICAL_FALLBACK";
      reasonLabel = "Salas teóricas disponíveis no projeto.";
    }

    return {
      options: Array.from(optionMap.values()),
      source,
      reasonLabel,
    };
  }

  // 3. Aula não teórica (LABORATORIO, ESPECIAL, etc.): salas não teóricas (mesmo tipo primeiro)
  const exactTypeRooms: Room[] = [];
  const otherNonTheoreticalRooms: Room[] = [];
  for (const r of allSalas) {
    const rTipo = r.tipo.toUpperCase();
    if (rTipo === tipoUpper) {
      exactTypeRooms.push(r);
    } else if (rTipo !== "TEORICA") {
      otherNonTheoreticalRooms.push(r);
    }
  }

  return {
    options: [...exactTypeRooms, ...otherNonTheoreticalRooms].map((r) => ({
      key: r.code,
      salas: [r.code],
      label: formatSingleRoomOptionLabel(r),
    })),
    source: "LAB_OR_SPECIAL",
    reasonLabel: "Laboratórios e salas especiais compatíveis com o tipo da aula.",
  };
}
