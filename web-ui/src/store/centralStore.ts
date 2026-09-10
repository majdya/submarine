import { create } from "zustand";
import { fetchFleetState } from "@/api/central";
import type { FleetState, Submarine } from "@/types/central";

interface CentralStore {
  fleet: FleetState | null;
  error: string | null;
  loading: boolean;
  startPolling: () => () => void; // returns a stop function
  refreshNow: () => Promise<void>;
}

// Why this is simpler than the old vanilla-JS dashboard: polling here only
// ever REPLACES the store's data. React then re-renders each <SubmarineCard>
// by key (submarine.serial) and diffs the DOM itself - it never tears down
// and rebuilds a card's actual <input> elements just because a poll landed,
// so a focused input never loses its value or cursor position. There is no
// "pinned card" / "don't touch the DOM node the user is typing in" logic to
// hand-write at all; that entire bug class doesn't exist here.
export const useCentralStore = create<CentralStore>((set) => ({
  fleet: null,
  error: null,
  loading: true,

  refreshNow: async () => {
    try {
      const fleet = await fetchFleetState();
      set({ fleet, error: null, loading: false });
    } catch (e) {
      set({ error: String(e), loading: false });
    }
  },

  startPolling: () => {
    let stopped = false;
    const tick = async () => {
      if (stopped) return;
      try {
        const fleet = await fetchFleetState();
        if (!stopped) set({ fleet, error: null, loading: false });
      } catch (e) {
        if (!stopped) set({ error: String(e), loading: false });
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

export function findSubmarine(fleet: FleetState | null, serial: string): Submarine | undefined {
  return fleet?.submarines.find((s) => s.serial === serial);
}
