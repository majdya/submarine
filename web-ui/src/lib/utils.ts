import { type ClassValue, clsx } from "clsx";
import { twMerge } from "tailwind-merge";

export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}

// Native <input type="date"> gives/takes "YYYY-MM-DD"; the Ground Station
// API (GET_LOGS/GET_EVENTS) expects "YYYYMMDD" - convert at the boundary so
// the rest of the app can just use real date pickers.
export function isoDateToYmd(iso: string): string {
  return iso.replace(/-/g, "");
}

