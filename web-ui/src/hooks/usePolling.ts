import { useEffect } from "react";

export function usePolling(startPolling: () => () => void) {
  useEffect(() => {
    const stop = startPolling();
    return stop;
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
}
