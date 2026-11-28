import { hmsToSeconds, secondsToHms } from "../../domain/time";

interface Props {
  valueSeconds: number;
  onChangeSeconds: (seconds: number) => void;
  disabled?: boolean;
  idPrefix?: string;
}

export function DurationHmsInput({
  valueSeconds,
  onChangeSeconds,
  disabled = false,
  idPrefix = "duration-hms",
}: Props) {
  const { hours, minutes, seconds } = secondsToHms(valueSeconds);
  const clampedHours = Math.min(99, hours);

  const updatePart = (part: "hours" | "minutes" | "seconds", rawValue: string) => {
    const parsed = Number(rawValue);
    const safe = Number.isFinite(parsed) ? Math.trunc(parsed) : 0;
    const nextH = part === "hours" ? Math.max(0, Math.min(99, safe)) : clampedHours;
    const nextM = part === "minutes" ? Math.max(0, Math.min(59, safe)) : minutes;
    const nextS = part === "seconds" ? Math.max(0, Math.min(59, safe)) : seconds;
    onChangeSeconds(hmsToSeconds(nextH, nextM, nextS));
  };

  return (
    <div className="mt-1 flex flex-wrap items-center gap-2">
      <div className="flex items-center gap-1">
        <input
          id={`${idPrefix}-hours`}
          aria-label="Horas"
          type="number"
          min={0}
          max={99}
          disabled={disabled}
          value={clampedHours}
          onChange={(e) => updatePart("hours", e.target.value)}
          className="w-16 rounded border border-slate-300 px-2 py-1.5 text-sm disabled:opacity-50 dark:border-slate-700 dark:bg-slate-800"
        />
        <span className="text-xs font-medium text-slate-500">h</span>
      </div>
      <div className="flex items-center gap-1">
        <input
          id={`${idPrefix}-minutes`}
          aria-label="Minutos"
          type="number"
          min={0}
          max={59}
          disabled={disabled}
          value={minutes}
          onChange={(e) => updatePart("minutes", e.target.value)}
          className="w-16 rounded border border-slate-300 px-2 py-1.5 text-sm disabled:opacity-50 dark:border-slate-700 dark:bg-slate-800"
        />
        <span className="text-xs font-medium text-slate-500">min</span>
      </div>
      <div className="flex items-center gap-1">
        <input
          id={`${idPrefix}-seconds`}
          aria-label="Segundos"
          type="number"
          min={0}
          max={59}
          disabled={disabled}
          value={seconds}
          onChange={(e) => updatePart("seconds", e.target.value)}
          className="w-16 rounded border border-slate-300 px-2 py-1.5 text-sm disabled:opacity-50 dark:border-slate-700 dark:bg-slate-800"
        />
        <span className="text-xs font-medium text-slate-500">s</span>
      </div>
    </div>
  );
}
