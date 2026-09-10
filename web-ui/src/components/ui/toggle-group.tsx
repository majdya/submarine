import { cn } from "@/lib/utils";

// A small, dependency-free segmented control - the toggle-button pattern the
// previous dashboard used for "Type" and "Enabled" fields, carried over here
// as a real component instead of hand-wired DOM/CSS classes.
export interface ToggleOption<T extends string> {
  value: T;
  label: string;
}

export function ToggleGroup<T extends string>({
  options,
  value,
  onChange,
  className,
}: {
  options: ToggleOption<T>[];
  value: T;
  onChange: (value: T) => void;
  className?: string;
}) {
  return (
    <div className={cn("inline-flex w-full rounded-md bg-muted p-1", className)}>
      {options.map((opt) => (
        <button
          key={opt.value}
          type="button"
          onClick={() => onChange(opt.value)}
          className={cn(
            "flex-1 rounded-sm px-3 py-1.5 text-xs font-medium transition-colors",
            value === opt.value
              ? "bg-background text-foreground shadow"
              : "text-muted-foreground hover:text-foreground",
          )}
        >
          {opt.label}
        </button>
      ))}
    </div>
  );
}
