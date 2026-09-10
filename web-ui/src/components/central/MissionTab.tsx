import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { assignMission, endMission, updateMission } from "@/api/central";
import type { Submarine } from "@/types/central";
import { useCentralStore } from "@/store/centralStore";

export function MissionTab({ sub }: { sub: Submarine }) {
  const [description, setDescription] = useState("");
  const [commanderName, setCommanderName] = useState("");
  const [personnelCount, setPersonnelCount] = useState("");
  const [researchTopic, setResearchTopic] = useState("");
  const [researcherNames, setResearcherNames] = useState("");
  const [status, setStatus] = useState<{ ok: boolean; message: string } | null>(null);
  const [busy, setBusy] = useState(false);
  const refreshNow = useCentralStore((s) => s.refreshNow);

  async function run(action: "assign" | "update" | "end") {
    setBusy(true);
    const result =
      action === "end"
        ? await endMission(sub.serial)
        : await (action === "assign" ? assignMission : updateMission)({
            serial: sub.serial,
            description,
            commanderName,
            personnelCount,
            researchTopic,
            researcherNames,
          });
    setBusy(false);
    setStatus(result.ok ? { ok: true, message: "Saved" } : { ok: false, message: result.error ?? "Unknown error" });
    if (result.ok) {
      // Local form fields intentionally stay as-is (React never wipes them
      // on its own) - the fleet poll picks up the new mission on its next
      // tick and updates the read-only summary above this form.
      refreshNow();
    }
  }

  return (
    <div className="flex flex-col gap-3">
      <div className="space-y-1.5">
        <Label>Description</Label>
        <Input placeholder="Mission description" value={description} onChange={(e) => setDescription(e.target.value)} />
      </div>
      <div className="grid grid-cols-2 gap-2">
        <div className="space-y-1.5">
          <Label>Commander (combat)</Label>
          <Input value={commanderName} onChange={(e) => setCommanderName(e.target.value)} />
        </div>
        <div className="space-y-1.5">
          <Label>Personnel (combat)</Label>
          <Input type="number" value={personnelCount} onChange={(e) => setPersonnelCount(e.target.value)} />
        </div>
      </div>
      <div className="space-y-1.5">
        <Label>Research topic (research)</Label>
        <Input value={researchTopic} onChange={(e) => setResearchTopic(e.target.value)} />
      </div>
      <div className="space-y-1.5">
        <Label>Researcher names (research, comma-separated)</Label>
        <Input value={researcherNames} onChange={(e) => setResearcherNames(e.target.value)} />
      </div>
      <div className="flex flex-wrap gap-2">
        <Button size="sm" disabled={busy} onClick={() => run("assign")}>
          Assign new
        </Button>
        <Button size="sm" variant="secondary" disabled={busy} onClick={() => run("update")}>
          Update existing
        </Button>
        <Button size="sm" variant="destructive" disabled={busy} onClick={() => run("end")}>
          End mission
        </Button>
      </div>
      {status && (
        <p className={`text-xs font-medium ${status.ok ? "text-good" : "text-destructive"}`}>{status.message}</p>
      )}
    </div>
  );
}
