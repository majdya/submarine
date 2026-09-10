# Submarine Fleet - Web UI (React)

A React + Vite + Tailwind + shadcn/ui + zustand replacement for the two
hand-written `dashboard.html` files, driving the exact same backend APIs.
Two pages/routes:

- `/` - **Central Computer** (port 8080): the full read/write fleet
  dashboard - add submarines, assign/update/end missions, associate combat
  submarines and send messages between them, set sensor limits.
- `/ground-station` - **Ground Station** (port 8081): the read-only view -
  fleet list, summary report, log/event queries by date range.

Mobile-first: bottom tab bar on small screens, top nav from `sm:` up: forms
use full-width sheet-style dialogs on mobile.

## Why this exists

The original `dashboard.html` files were hand-rolled vanilla JS doing manual
DOM diffing against a 2-second poll. That grew a real, recurring class of
bugs (typing in a form field getting wiped by the next poll's re-render) that
needed increasingly delicate hand-written "don't touch the DOM node the user
is focused in" logic to fix. React's own reconciliation (each `<SubmarineCard>`
keyed by serial, each form's `<input>` backed by real component state) makes
that whole bug class structurally impossible - a poll just updates the
zustand store's data; React only re-renders what actually changed, and never
tears down a focused input just because new data arrived.

## Prerequisites

- Node.js 18+ and npm (already on this machine).
- The C++ backends built and running (see the repo root's `build-and-start.md`):
  - `central_computer.exe` serving its API on `http://localhost:8080`
  - `ground_station.exe` serving its API on `http://localhost:8081`

Nothing here changes those binaries or their APIs - this is a pure frontend
replacement, mounted separately from the CMake build (`add_subdirectory` in
the root `CMakeLists.txt` does not reference `web-ui/`).

## Development

```
cd web-ui
npm install
npm run dev
```

Open the printed URL (usually `http://localhost:5173`). The dev server
proxies `/api/central/*` → `http://localhost:8080/api/*` and
`/api/ground/*` → `http://localhost:8081/api/*` (see `vite.config.ts`), so
make sure both backends are already running first.

## Production build

```
npm run build
```

Outputs static files to `web-ui/dist/`. Since the C++ `HttpServer`s only
know how to serve their own single `web/dashboard.html` file (no static
asset directory support), the simplest way to use a production build today
is to serve `dist/` with any static file server and rely on the same
`/api/central` and `/api/ground` proxy paths (e.g. via a small `vite preview`
run, or any reverse proxy in front of both backends). The old
`central-computer/web/dashboard.html` and `ground_station/web/dashboard.html`
are left untouched as a fallback / for the graded console app to keep
working standalone.

## Project structure

```
src/
  api/            fetch wrappers per backend (central.ts, ground.ts)
  types/          TypeScript types mirroring each backend's exact JSON shapes
  store/          zustand stores (2s polling, replace-on-fetch)
  components/
    ui/           shadcn primitives (button, card, dialog, tabs, ...)
    central/      Central Computer page's building blocks
    ground/       Ground Station page's building blocks
  pages/          the two routed pages
  App.tsx         shell: top nav (desktop) / bottom tab bar (mobile), routes
```
