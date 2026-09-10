import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { fetchLogs } from "@/api/ground";
import { isoDateToYmd } from "@/lib/utils";
import { GS_MODE_NAMES, type GsLogRow } from "@/types/ground";

export function LogsQueryCard() {
  const [serial, setSerial] = useState("");
  const [start, setStart] = useState(""); // "YYYY-MM-DD" from the date picker
  const [end, setEnd] = useState("");
  const [status, setStatus] = useState<{ ok: boolean; message: string } | null>(null);
  const [rows, setRows] = useState<GsLogRow[]>([]);
  const [busy, setBusy] = useState(false);

  async function handleSubmit(e: React.FormEvent) {
    e.preventDefault();
    setBusy(true);
    const result = await fetchLogs(serial, isoDateToYmd(start), isoDateToYmd(end));
    setBusy(false);
    if (result.status === "ok") {
      setStatus({ ok: true, message: "OK" });
      setRows(result.data);
    } else {
      setStatus({ ok: false, message: String(result.data) });
      setRows([]);
    }
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle>Get logs</CardTitle>
      </CardHeader>
      <CardContent className="flex flex-col gap-3">
        <form onSubmit={handleSubmit} className="flex flex-col gap-3">
          <div className="space-y-1.5">
            <Label>Submarine serial</Label>
            <Input required value={serial} onChange={(e) => setSerial(e.target.value)} />
          </div>
          <div className="grid grid-cols-2 gap-2">
            <div className="space-y-1.5">
              <Label>Start date</Label>
              <Input type="date" required value={start} onChange={(e) => setStart(e.target.value)} />
            </div>
            <div className="space-y-1.5">
              <Label>End date</Label>
              <Input type="date" required value={end} onChange={(e) => setEnd(e.target.value)} />
            </div>
          </div>
          <Button type="submit" size="sm" disabled={busy}>
            Query logs
          </Button>
          {status && (
            <p className={`text-xs font-medium ${status.ok ? "text-good" : "text-destructive"}`}>
              {status.ok ? "OK" : `Error: ${status.message}`}
            </p>
          )}
        </form>
        <div className="max-h-64 overflow-y-auto">
          {rows.length === 0 ? (
            <p className="text-sm text-muted-foreground">{status ? "No log records in that range." : ""}</p>
          ) : (
            <div className="flex flex-col gap-1.5">
              {rows.map((r, i) => (
                <div key={i} className="rounded-md bg-muted px-2.5 py-1.5 font-mono text-xs">
                  {r.ymd} {r.hms} {GS_MODE_NAMES[r.mode] ?? "?"} light={r.lightRaw} tempADC={r.tempAdcRaw} batt=
                  {r.batteryRaw} dht={r.dhtTemp}°C/{r.dhtHumidity}%{!r.dhtValid && " (dht invalid)"}
                </div>
              ))}
            </div>
          )}
        </div>
      </CardContent>
    </Card>
  );
}
