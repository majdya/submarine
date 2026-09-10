import { useGroundStore } from "@/store/groundStore";
import { usePolling } from "@/hooks/usePolling";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Badge } from "@/components/ui/badge";
import { GsSubmarineCard } from "@/components/ground/GsSubmarineCard";
import { LogsQueryCard } from "@/components/ground/LogsQueryCard";
import { EventsQueryCard } from "@/components/ground/EventsQueryCard";

export default function GroundStationPage() {
  const submarines = useGroundStore((s) => s.submarines);
  const summary = useGroundStore((s) => s.summary);
  const loading = useGroundStore((s) => s.loading);
  const error = useGroundStore((s) => s.error);
  const startPolling = useGroundStore((s) => s.startPolling);
  usePolling(startPolling);

  return (
    <div className="flex flex-col gap-4">
      <div>
        <div className="flex items-center gap-2">
          <h1 className="text-lg font-semibold">Ground Station</h1>
          <Badge variant="secondary">Read-only</Badge>
        </div>
        <p className="text-xs text-muted-foreground">
          Browser-based view of exactly what the console CLI can already see (spec §4: log data and event
          data for a date range, from the submarine's Central Computer).
        </p>
      </div>

      {error && <div className="rounded-md bg-bad-soft px-3 py-2 text-sm text-bad">Couldn't reach Ground Station: {error}</div>}

      <Card>
        <CardHeader>
          <CardTitle>Fleet (from LIST_SUBMARINES)</CardTitle>
        </CardHeader>
        <CardContent className="flex flex-col gap-3">
          {loading && submarines.length === 0 && <p className="text-sm text-muted-foreground">Loading…</p>}
          {!loading && submarines.length === 0 && !error && (
            <p className="text-sm text-muted-foreground">No submarines in the fleet.</p>
          )}
          {submarines.map((sub) => (
            <GsSubmarineCard key={sub.serial} sub={sub} />
          ))}
        </CardContent>
      </Card>

      <Card>
        <CardHeader>
          <CardTitle>Summary report</CardTitle>
        </CardHeader>
        <CardContent>
          {summary ? (
            <p className="text-sm">
              Total: <b>{summary.totalSubmarines}</b> · Combat: <b>{summary.combat}</b> · Research:{" "}
              <b>{summary.research}</b> · Active missions: <b>{summary.activeMissions}</b>
            </p>
          ) : (
            <p className="text-sm text-muted-foreground">Loading…</p>
          )}
        </CardContent>
      </Card>

      <LogsQueryCard />
      <EventsQueryCard />
    </div>
  );
}
