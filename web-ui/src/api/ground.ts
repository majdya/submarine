import type { GsEventRow, GsLogRow, GsResult, GsSubmarine, GsSummary } from "@/types/ground";

// In dev, vite.config.ts proxies /api/ground/* -> http://localhost:8081/api/*.
const BASE = "/api/ground";

async function getJson<T>(path: string): Promise<GsResult<T>> {
  try {
    const res = await fetch(BASE + path);
    return (await res.json()) as GsResult<T>;
  } catch (e) {
    return { status: "error", data: String(e) as unknown as T };
  }
}

export function fetchSubmarines() {
  return getJson<GsSubmarine[]>("/submarines");
}

export function fetchSummary() {
  return getJson<GsSummary>("/summary");
}

export function fetchLogs(serial: string, start: string, end: string) {
  const qs = `?serial=${encodeURIComponent(serial)}&start=${encodeURIComponent(start)}&end=${encodeURIComponent(end)}`;
  return getJson<GsLogRow[]>("/logs" + qs);
}

export function fetchEvents(serial: string, start: string, end: string) {
  const qs = `?serial=${encodeURIComponent(serial)}&start=${encodeURIComponent(start)}&end=${encodeURIComponent(end)}`;
  return getJson<GsEventRow[]>("/events" + qs);
}
