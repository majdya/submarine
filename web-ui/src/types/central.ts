// Central Computer API types - mirrors central-computer/web/dashboard.html's
// /api/state response and mutation routes exactly (verified against the
// live dashboard.html source, not guessed).

export type SubmarineType = "Combat" | "Research";

export interface Mission {
  description: string;
  commanderName: string;
  personnelCount: number;
  researchTopic: string;
  researcherNames: string[];
}

export interface LiveSnapshot {
  hasData: boolean;
  receivedAtHms: string;
  mode: number;
  lightRaw?: number;
  tempAdcRaw?: number;
  batteryRaw?: number;
  dhtTemp?: number;
  dhtHumidity?: number;
  dhtValid?: boolean;
  lastEventDescription?: string;
  lastEventAtHms?: string;
}

export interface ReceivedMessage {
  from: string;
  content: string;
}

export interface Submarine {
  serial: string;
  name: string;
  type: SubmarineType;
  assigned: boolean;
  missionHistoryCount: number;
  mission: Mission | null;
  connected: boolean;
  liveSnapshot: LiveSnapshot;
  participatingSerials?: string[]; // combat only
  messages?: ReceivedMessage[]; // combat only ("messages received")
}

export interface FleetState {
  submarines: Submarine[];
}

// Every mutation route responds { ok: true } or { ok: false, error: string }.
export interface ApiResult {
  ok: boolean;
  error?: string;
}

export interface AddSubmarineInput {
  type: SubmarineType;
  serial: string;
  name: string;
  port: string;
}

export interface MissionInput {
  serial: string;
  description: string;
  commanderName: string;
  personnelCount: string;
  researchTopic: string;
  researcherNames: string;
}

export interface SetLimitsInput {
  param: "temp" | "humidity" | "light" | "battery";
  normalMin: string;
  normalMax: string;
  warningMin: string;
  warningMax: string;
  enabled: "" | "enable" | "disable";
}
