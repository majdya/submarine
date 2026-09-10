import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from "@/components/ui/select";
import { ToggleGroup } from "@/components/ui/toggle-group";
import { setLimits } from "@/api/central";
import type { Submarine, SetLimitsInput } from "@/types/central";

const PARAMS: { value: SetLimitsInput["param"]; label: string }[] = [
  { value: "temp", label: "Temperature" },
  { value: "humidity", label: "Humidity" },
  { value: "light", label: "Light" },
  { value: "battery", label: "Battery" },
];

export function LimitsTab({ sub }: { sub: Submarine }) {
  const [param, setParam] = useState<SetLimitsInput["param"]>("temp");
  const [normalMin, setNormalMin] = useState("");
  const [normalMax, setNormalMax] = useState("");
  const [warningMin, setWarningMin] = useState("");
  const [warningMax, setWarningMax] = useState("");
  const [enabled, setEnabled] = useState<SetLimitsInput["enabled"]>("");
  const [status, setStatus] = useState<{ ok: boolean; message: string } | null>(null);
  const [busy, setBusy] = useState(false);

  async function handleApply() {
    setBusy(true);
    const result = await setLimits(sub.serial, { param, normalMin, normalMax, warningMin, warningMax, enabled });
    setBusy(false);
    setStatus(result.ok ? { ok: true, message: "Saved" } : { ok: false, message: result.error ?? "Unknown error" });
  }

  return (
    <div className="flex flex-col gap-3">
      <div className="space-y-1.5">
        <Label>Parameter</Label>
        <Select value={param} onValueChange={(v) => setParam(v as SetLimitsInput["param"])}>
          <SelectTrigger>
            <SelectValue />
          </SelectTrigger>
          <SelectContent>
            {PARAMS.map((p) => (
              <SelectItem key={p.value} value={p.value}>
                {p.label}
              </SelectItem>
            ))}
          </SelectContent>
        </Select>
      </div>
      <div className="grid grid-cols-2 gap-2">
        <div className="space-y-1.5">
          <Label>Normal min</Label>
          <Input type="number" value={normalMin} onChange={(e) => setNormalMin(e.target.value)} />
        </div>
        <div className="space-y-1.5">
          <Label>Normal max (temp only)</Label>
          <Input type="number" value={normalMax} onChange={(e) => setNormalMax(e.target.value)} />
        </div>
      </div>
      <div className="grid grid-cols-2 gap-2">
        <div className="space-y-1.5">
          <Label>Warning min</Label>
          <Input type="number" value={warningMin} onChange={(e) => setWarningMin(e.target.value)} />
        </div>
        <div className="space-y-1.5">
          <Label>Warning max (temp only)</Label>
          <Input type="number" value={warningMax} onChange={(e) => setWarningMax(e.target.value)} />
        </div>
      </div>
      <p className="text-xs text-muted-foreground">Leave all bounds blank to only change the enabled state below.</p>
      <div className="space-y-1.5">
        <Label>Enabled</Label>
        <ToggleGroup
          options={[
            { value: "", label: "Unchanged" },
            { value: "enable", label: "Enable" },
            { value: "disable", label: "Disable" },
          ]}
          value={enabled}
          onChange={setEnabled}
        />
      </div>
      <Button size="sm" disabled={busy} onClick={handleApply} className="mt-1">
        Apply
      </Button>
      {status && (
        <p className={`text-xs font-medium ${status.ok ? "text-good" : "text-destructive"}`}>{status.message}</p>
      )}
    </div>
  );
}
