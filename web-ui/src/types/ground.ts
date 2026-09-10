// Ground Station API types - mirrors ground_station/web/dashboard.html's
// endpoints exactly. Different envelope shape than Central Computer:
// { status: "ok" | "error", data: ... } (see gs_dashboard_api.cpp, a thin
// relay of the Central Computer's TcpApi's own {"status":...,"data":...}
// wire format).

export interface GsResult<T> {
  status: "ok" | "error";
  data: T;
}

export interface GsSubmarine {
  serial: string;
  name: string;
  type: "Combat" | "Research";
  missionAssigned: boolean;
  missionDescription?: string;
  connected: boolean;
}

export interface GsSummary {
  totalSubmarines: number;
  combat: number;
  research: number;
  activeMissions: number;
}

export const GS_MODE_NAMES = ["NORMAL", "WARNING", "ERROR"] as const;
export const GS_EVENT_NAMES = ["?", "OBJECT_DETECTED", "OBJECT_CLEARED", "SILENCE_PRESSED", "MODE_CHANGED"] as const;

export interface GsLogRow {
  ymd: string;
  hms: string;
  mode: number;
  lightRaw: number;
  tempAdcRaw: number;
  batteryRaw: number;
  dhtTemp: number;
  dhtHumidity: number;
  dhtValid: boolean;
}

export interface GsEventRow {
  ymd: string;
  hms: string;
  eventType: number;
  eventValue: number;
  mode: number;
}
