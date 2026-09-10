import { useCentralStore } from "@/store/centralStore";
import { usePolling } from "@/hooks/usePolling";
import { StatRow } from "@/components/central/StatRow";
import { AddSubmarineDialog } from "@/components/central/AddSubmarineDialog";
import { SubmarineCard } from "@/components/central/SubmarineCard";

export default function CentralComputerPage() {
  const fleet = useCentralStore((s) => s.fleet);
  const loading = useCentralStore((s) => s.loading);
  const error = useCentralStore((s) => s.error);
  const startPolling = useCentralStore((s) => s.startPolling);
  usePolling(startPolling);

  const submarines = fleet?.submarines ?? [];

  return (
    <div className="flex flex-col gap-4">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-lg font-semibold">Central Computer</h1>
          <p className="flex items-center gap-1.5 text-xs text-muted-foreground">
            <span className="h-1.5 w-1.5 animate-pulse-dot rounded-full bg-good" />
            Live · refreshing every 2s
          </p>
        </div>
      </div>

      <StatRow fleet={fleet} />

      <div className="flex items-center justify-between">
        <h2 className="text-xs font-semibold uppercase tracking-wide text-muted-foreground">Fleet</h2>
        <AddSubmarineDialog />
      </div>

      {error && (
        <div className="rounded-md bg-bad-soft px-3 py-2 text-sm text-bad">Couldn't reach Central Computer: {error}</div>
      )}

      {loading && !fleet && <div className="text-sm text-muted-foreground">Loading fleet…</div>}

      {!loading && submarines.length === 0 && !error && (
        <div className="rounded-lg border border-dashed border-border p-8 text-center text-sm text-muted-foreground">
          🌊 The fleet is empty — add a submarine to get started.
        </div>
      )}

      <div className="grid grid-cols-1 gap-3 md:grid-cols-2 xl:grid-cols-3 2xl:grid-cols-4">
        {submarines.map((sub) => (
          <SubmarineCard key={sub.serial} sub={sub} />
        ))}
      </div>
    </div>
  );
}
