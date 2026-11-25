import type { Disciplina, Room, Turma } from "../types";

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
