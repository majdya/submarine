import { useState } from "react";
import { Button } from "@/components/ui/button";
import {
  Dialog,
  DialogContent,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from "@/components/ui/dialog";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { ToggleGroup } from "@/components/ui/toggle-group";
import { Plus } from "lucide-react";
import { addSubmarine } from "@/api/central";
import type { SubmarineType } from "@/types/central";
import { useCentralStore } from "@/store/centralStore";

export function AddSubmarineDialog() {
  const [open, setOpen] = useState(false);
  const [type, setType] = useState<SubmarineType>("Research");
  const [serial, setSerial] = useState("");
  const [name, setName] = useState("");
  const [port, setPort] = useState("");
  const [status, setStatus] = useState<{ ok: boolean; message: string } | null>(null);
  const [submitting, setSubmitting] = useState(false);
  const refreshNow = useCentralStore((s) => s.refreshNow);

  function reset() {
    setSerial("");
    setName("");
    setPort("");
    setStatus(null);
    // Keep type unchanged - user might want to add multiple of same type
  }

  async function handleSubmit(e: React.FormEvent) {
    e.preventDefault();
    setSubmitting(true);

    // DEBUG: Log what we're sending
    const payload = { type, serial, name, port };
    console.log("📤 Submitting submarine:", payload);

    const result = await addSubmarine(payload);
    console.log("📥 Response:", result);

    setSubmitting(false);
    if (result.ok) {
      setOpen(false);
      reset();
      refreshNow();
    } else {
      setStatus({ ok: false, message: result.error ?? "Unknown error" });
    }
  }

  return (
    <Dialog
      open={open}
      onOpenChange={(next) => {
        setOpen(next);
        if (!next) reset();
      }}
    >
      <DialogTrigger asChild>
        <Button size="sm" className="gap-1.5">
          <Plus className="h-4 w-4" />
          Add submarine
        </Button>
      </DialogTrigger>
      <DialogContent>
        <DialogHeader>
          <DialogTitle>Add submarine</DialogTitle>
        </DialogHeader>
        <form onSubmit={handleSubmit} className="flex flex-col gap-3">
          <div className="space-y-1.5">
            <Label>Type: <strong>{type}</strong></Label>
            <ToggleGroup
              options={[
                { value: "Research", label: "Research" },
                { value: "Combat", label: "Combat" },
              ]}
              value={type}
              onChange={(newType) => {
                console.log("Type changed to:", newType);
                setType(newType);
              }}
            />
          </div>
          <div className="space-y-1.5">
            <Label htmlFor="add-serial">Serial</Label>
            <Input id="add-serial" required placeholder="e.g. R-104" value={serial} onChange={(e) => setSerial(e.target.value)} />
          </div>
          <div className="space-y-1.5">
            <Label htmlFor="add-name">Name</Label>
            <Input id="add-name" required placeholder="e.g. Nautilus" value={name} onChange={(e) => setName(e.target.value)} />
          </div>
          <div className="space-y-1.5">
            <Label htmlFor="add-port">Serial port (optional)</Label>
            <Input id="add-port" placeholder="COM8 or /dev/ttyACM0" value={port} onChange={(e) => setPort(e.target.value)} />
          </div>
          {status && !status.ok && <p className="text-sm text-destructive">Error: {status.message}</p>}
          <Button type="submit" disabled={submitting} className="mt-1">
            {submitting ? "Adding…" : "Add submarine"}
          </Button>
        </form>
      </DialogContent>
    </Dialog>
  );
}
