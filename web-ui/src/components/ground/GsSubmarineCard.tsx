import { Badge } from "@/components/ui/badge";
import { Card } from "@/components/ui/card";
import { cn } from "@/lib/utils";
import type { GsSubmarine } from "@/types/ground";

export function GsSubmarineCard({ sub }: { sub: GsSubmarine }) {
  const isCombat = sub.type === "Combat";
  return (
    <Card className={cn("overflow-hidden border-l-4 p-4", isCombat ? "border-l-combat" : "border-l-research")}>
      <div className="flex items-start justify-between gap-2">
        <div>
          <div className="font-semibold">{sub.name}</div>
          <div className="mt-0.5 flex items-center gap-2">
            <span className="text-xs text-muted-foreground">{sub.serial}</span>
            <Badge variant={isCombat ? "combat" : "research"}>{sub.type}</Badge>
          </div>
        </div>
        <Badge variant={sub.missionAssigned ? "good" : "secondary"}>
          {sub.missionAssigned ? "Assigned" : "Available"}
        </Badge>
      </div>
      {sub.missionAssigned && (
        <div className="mt-2 text-sm">
          <span className="text-xs text-muted-foreground">Mission: </span>
          {sub.missionDescription || "(no description)"}
        </div>
      )}
      <div className="mt-2">
        <Badge variant={sub.connected ? "good" : "secondary"}>{sub.connected ? "Connected" : "No hardware"}</Badge>
      </div>
    </Card>
  );
}
