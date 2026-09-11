import type { AddSubmarineInput, ApiResult, FleetState, MissionInput, SetLimitsInput } from "@/types/central";

const BASE = "/api/central";

async function postForm(path: string, fields: any): Promise<ApiResult> {
  const body = new URLSearchParams();
  for (const [key, value] of Object.entries(fields)) {
    body.append(key, String(value));
  }
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
  return postForm(`/submarines/${encodeURIComponent(input.serial)}/mission`, input);
}

export function updateMission(input: MissionInput) {
  return postForm(`/submarines/${encodeURIComponent(input.serial)}/mission/update`, input);
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
  return postForm(`/submarines/${encodeURIComponent(serial)}/limits`, input);
}
