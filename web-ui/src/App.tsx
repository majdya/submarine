import { NavLink, Route, Routes } from "react-router-dom";
import { Anchor, Radio } from "lucide-react";
import { cn } from "@/lib/utils";
import CentralComputerPage from "@/pages/CentralComputerPage";
import GroundStationPage from "@/pages/GroundStationPage";

const NAV_ITEMS = [
  { to: "/", label: "Central Computer", icon: Anchor },
  { to: "/ground-station", label: "Ground Station", icon: Radio },
];

export default function App() {
  return (
    <div className="flex min-h-dvh flex-col">
      {/* Top bar: brand + nav on larger screens */}
      <header className="sticky top-0 z-40 border-b border-border bg-background/95 backdrop-blur">
        <div className="container flex h-14 items-center justify-between">
          <div className="flex items-center gap-2">
            <div className="flex h-8 w-8 items-center justify-center rounded-md bg-primary/15 text-primary">
              <Anchor className="h-4 w-4" />
            </div>
            <span className="text-sm font-semibold">Submarine Fleet</span>
          </div>
          <nav className="hidden gap-1 sm:flex">
            {NAV_ITEMS.map((item) => (
              <NavLink
                key={item.to}
                to={item.to}
                end={item.to === "/"}
                className={({ isActive }) =>
                  cn(
                    "flex items-center gap-2 rounded-md px-3 py-1.5 text-sm font-medium transition-colors",
                    isActive ? "bg-secondary text-foreground" : "text-muted-foreground hover:text-foreground",
                  )
                }
              >
                <item.icon className="h-4 w-4" />
                {item.label}
              </NavLink>
            ))}
          </nav>
        </div>
      </header>

      <main className="container flex-1 pb-24 pt-4 sm:pb-8">
        <Routes>
          <Route path="/" element={<CentralComputerPage />} />
          <Route path="/ground-station" element={<GroundStationPage />} />
        </Routes>
      </main>

      {/* Bottom tab bar: mobile-first primary nav */}
      <nav className="fixed inset-x-0 bottom-0 z-40 border-t border-border bg-background/95 backdrop-blur sm:hidden">
        <div className="grid grid-cols-2">
          {NAV_ITEMS.map((item) => (
            <NavLink
              key={item.to}
              to={item.to}
              end={item.to === "/"}
              className={({ isActive }) =>
                cn(
                  "flex flex-col items-center gap-1 py-2.5 text-xs font-medium transition-colors",
                  isActive ? "text-primary" : "text-muted-foreground",
                )
              }
            >
              <item.icon className="h-5 w-5" />
              {item.label}
            </NavLink>
          ))}
        </div>
      </nav>
    </div>
  );
}
