import { create } from "zustand";
import { fetchSubmarines, fetchSummary } from "@/api/ground";
import type { GsSubmarine, GsSummary } from "@/types/ground";

interface GroundStore {
  submarines: GsSubmarine[];
  summary: GsSummary | null;
  error: string | null;
  loading: boolean;
  startPolling: () => () => void;
}

export const useGroundStore = create<GroundStore>((set) => ({
  submarines: [],
  summary: null,
  error: null,
  loading: true,

  startPolling: () => {
    let stopped = false;
    const tick = async () => {
      if (stopped) return;
      const [subsResult, summaryResult] = await Promise.all([fetchSubmarines(), fetchSummary()]);
      if (stopped) return;
      if (subsResult.status === "ok") {
        set({ submarines: subsResult.data, error: null, loading: false });
      } else {
        set({ error: String(subsResult.data), loading: false });
      }
      if (summaryResult.status === "ok") {
        set({ summary: summaryResult.data });
      }
    };
    tick();
    const id = setInterval(tick, 2000);
    return () => {
      stopped = true;
      clearInterval(id);
    };
  },
}));
