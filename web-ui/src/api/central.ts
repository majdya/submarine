import type { AddSubmarineInput, ApiResult, FleetState, MissionInput, SetLimitsInput } from "@/types/central";

// In dev, vite.config.ts proxies /api/central/* -> http://localhost:8080/api/*.
// In a static prod build served by a reverse proxy / same-origin setup, point
// this at whatever path fronts port 8080 - see web-ui/README.md.
const BASE = "/api/central";

async function postForm(path: string, fields: Record<string, string>): Promise<ApiResult> {
  const body = new URLSearchParams(fields);
  try {
    const res = await fetch(BASE + path, { method: "POST", body });
    return (await res.json()) as ApiResult;
  } catch (e) {
    return { ok: false, error: String(e) };
  }
}

export async function fetchFleetState(): Promise<FleetState> {
  const res = await fetch(BASE + "/state");
  return (await res.json()) as FleetState;
}

export function addSubmarine(input: AddSubmarineInput) {
  return postForm("/submarines", input);
}

export function assignMission(input: MissionInput) {
  return postForm(`/submarines/${encodeURIComponent(input.serial)}/mission`, input as unknown as Record<string, string>);
}

export function updateMission(input: MissionInput) {
  return postForm(`/submarines/${encodeURIComponent(input.serial)}/mission/update`, input as unknown as Record<string, string>);
}

export function endMission(serial: string) {
  return postForm(`/submarines/${encodeURIComponent(serial)}/mission/end`, {});
}

export function associate(serial: string, other: string) {
  return postForm(`/submarines/${encodeURIComponent(serial)}/participate`, { other });
}

export function sendMessage(from: string, to: string, content: string) {
  return postForm("/messages", { from, to, content });
}

export function setLimits(serial: string, input: SetLimitsInput) {
  return postForm(`/submarines/${encodeURIComponent(serial)}/limits`, input as unknown as Record<string, string>);
}
