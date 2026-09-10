import { Card } from "@/components/ui/card";
import type { FleetState } from "@/types/central";

export function StatRow({ fleet }: { fleet: FleetState | null }) {
  const subs = fleet?.submarines ?? [];
  const total = subs.length;
  const combat = subs.filter((s) => s.type === "Combat").length;
  const research = subs.filter((s) => s.type === "Research").length;
  const connected = subs.filter((s) => s.connected).length;

  const tiles = [
    { label: "Fleet size", value: total, color: "text-foreground" },
    { label: "Combat", value: combat, color: "text-combat" },
    { label: "Research", value: research, color: "text-research" },
    { label: "Hardware connected", value: connected, color: "text-good" },
  ];

  return (
    <div className="grid grid-cols-2 gap-2 sm:grid-cols-4 sm:gap-3">
      {tiles.map((t) => (
        <Card key={t.label} className="p-3 sm:p-4">
          <div className={`text-2xl font-bold ${t.color}`}>{t.value}</div>
          <div className="text-xs text-muted-foreground">{t.label}</div>
        </Card>
      ))}
    </div>
  );
}
