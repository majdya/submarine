import { useState } from "react";
import { ChevronDown } from "lucide-react";
import { Badge } from "@/components/ui/badge";
import { Card } from "@/components/ui/card";
import { Tabs, TabsContent, TabsList, TabsTrigger } from "@/components/ui/tabs";
import { cn } from "@/lib/utils";
import type { Submarine } from "@/types/central";
import { MissionTab } from "./MissionTab";
import { CombatTab } from "./CombatTab";
import { LimitsTab } from "./LimitsTab";

export function SubmarineCard({ sub }: { sub: Submarine }) {
  const [open, setOpen] = useState(false);
  const isCombat = sub.type === "Combat";
  const snapshot = sub.liveSnapshot;

  return (
    <Card
      className={cn(
        "overflow-hidden border-l-4",
        isCombat ? "border-l-combat" : "border-l-research",
      )}
    >
      <div className="p-4">
        <div className="flex items-start justify-between gap-2">
          <div>
            <div className="font-semibold">{sub.name}</div>
            <div className="mt-0.5 flex items-center gap-2">
              <span className="text-xs text-muted-foreground">{sub.serial}</span>
              <Badge variant={isCombat ? "combat" : "research"}>{sub.type}</Badge>
            </div>
          </div>
          <Badge variant={sub.assigned ? "good" : "secondary"}>{sub.assigned ? "Assigned" : "Available"}</Badge>
        </div>

        <div className="mt-3 grid grid-cols-2 gap-x-4 gap-y-2 text-sm sm:grid-cols-3">
          <div>
            <div className="text-xs text-muted-foreground">Mission</div>
            <div className={sub.mission ? "" : "italic text-muted-foreground"}>
              {sub.mission ? sub.mission.description || "(no description)" : "No active mission"}
            </div>
          </div>
          {sub.mission && isCombat && (
            <div>
              <div className="text-xs text-muted-foreground">Commander</div>
              <div>{sub.mission.commanderName || "—"}</div>
            </div>
          )}
          {sub.mission && isCombat && (
            <div>
              <div className="text-xs text-muted-foreground">Personnel</div>
              <div>{sub.mission.personnelCount}</div>
            </div>
          )}
          {sub.mission && !isCombat && (
            <div>
              <div className="text-xs text-muted-foreground">Topic</div>
              <div>{sub.mission.researchTopic || "—"}</div>
            </div>
          )}
          {sub.mission && !isCombat && (
            <div>
              <div className="text-xs text-muted-foreground">Researchers</div>
              <div>{sub.mission.researcherNames?.join(", ") || "—"}</div>
            </div>
          )}
          <div>
            <div className="text-xs text-muted-foreground">Past missions</div>
            <div>{sub.missionHistoryCount}</div>
          </div>
          <div>
            <div className="text-xs text-muted-foreground">Participating with</div>
            <div className={isCombat && sub.participatingSerials?.length ? "" : "italic text-muted-foreground"}>
              {isCombat ? sub.participatingSerials?.join(", ") || "none" : "n/a (Research)"}
            </div>
          </div>
        </div>

        <div className="mt-3 flex items-center gap-2 border-t border-border pt-3">
          <Badge variant={sub.connected ? "good" : "secondary"}>{sub.connected ? "Connected" : "No hardware"}</Badge>
          {snapshot?.hasData && (
            <span className="text-xs text-muted-foreground">
              mode {["NORMAL", "WARNING", "ERROR"][snapshot.mode] ?? "?"}
            </span>
          )}
        </div>

        {snapshot?.hasData && (
          <div className="mt-2 flex flex-wrap gap-x-4 gap-y-1 text-xs text-muted-foreground">
            <span>
              light <b className="text-foreground">{snapshot.lightRaw}</b>
            </span>
            <span>
              battery <b className="text-foreground">{snapshot.batteryRaw}</b>
            </span>
            <span>
              temp <b className="text-foreground">{snapshot.dhtTemp}°C</b>
            </span>
            <span>
              humidity <b className="text-foreground">{snapshot.dhtHumidity}%</b>
            </span>
            <span>as of {snapshot.receivedAtHms}</span>
            {snapshot.lastEventDescription && (
              <span>
                · last event: {snapshot.lastEventDescription} @ {snapshot.lastEventAtHms}
              </span>
            )}
          </div>
        )}

        {isCombat && (
          <div className="mt-3 border-t border-border pt-3">
            <div className="text-xs text-muted-foreground">Messages received</div>
            {sub.messages?.length ? (
              <div className="mt-1 flex flex-col gap-1.5">
                {sub.messages.map((m, i) => (
                  <div key={i} className="rounded-md bg-muted px-2.5 py-1.5 text-sm">
                    <div className="text-xs font-medium text-muted-foreground">From {m.from}</div>
                    {m.content}
                  </div>
                ))}
              </div>
            ) : (
              <div className="text-sm italic text-muted-foreground">none</div>
            )}
          </div>
        )}

        <button
          type="button"
          onClick={() => setOpen((o) => !o)}
          className="mt-3 flex w-full items-center justify-between border-t border-border pt-3 text-sm font-medium text-primary"
        >
          Actions
          <ChevronDown className={cn("h-4 w-4 transition-transform", open && "rotate-180")} />
        </button>
      </div>

      {open && (
        <div className="border-t border-border bg-muted/30 p-4">
          <Tabs defaultValue="mission">
            <TabsList>
              <TabsTrigger value="mission">Mission</TabsTrigger>
              {isCombat && <TabsTrigger value="combat">Associate &amp; message</TabsTrigger>}
              <TabsTrigger value="limits">Sensor limits</TabsTrigger>
            </TabsList>
            <TabsContent value="mission">
              <MissionTab sub={sub} />
            </TabsContent>
            {isCombat && (
              <TabsContent value="combat">
                <CombatTab sub={sub} />
              </TabsContent>
            )}
            <TabsContent value="limits">
              <LimitsTab sub={sub} />
            </TabsContent>
          </Tabs>
        </div>
      )}
    </Card>
  );
}
