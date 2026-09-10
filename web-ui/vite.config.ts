import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";
import path from "path";

// Two backends, same convention: Central Computer on 8080, Ground Station on
// 8081 (see central-computer/src/main.cpp and ground_station/src/main.cpp).
// The dev server proxies /api/* so the app can just call relative paths in
// both dev and prod (prod: this build's static output is served by whichever
// process you point a static file server at, or opened directly - see
// web-ui/README.md).
export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      "@": path.resolve(__dirname, "./src"),
    },
  },
  server: {
    host: true,
    port: 5173,
    proxy: {
      "/api/central": {
        target: "http://localhost:8080",
        changeOrigin: true,
        rewrite: (p) => p.replace(/^\/api\/central/, "/api"),
      },
      "/api/ground": {
        target: "http://localhost:8081",
        changeOrigin: true,
        rewrite: (p) => p.replace(/^\/api\/ground/, "/api"),
      },
    },
  },
});
