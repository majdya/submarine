import { useState } from "react";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { associate, sendMessage } from "@/api/central";
import type { Submarine } from "@/types/central";
import { useCentralStore } from "@/store/centralStore";

export function CombatTab({ sub }: { sub: Submarine }) {
  const [otherSerial, setOtherSerial] = useState("");
  const [assocStatus, setAssocStatus] = useState<{ ok: boolean; message: string } | null>(null);
  const [assocBusy, setAssocBusy] = useState(false);

  const [msgTo, setMsgTo] = useState("");
  const [msgContent, setMsgContent] = useState("");
  const [msgStatus, setMsgStatus] = useState<{ ok: boolean; message: string } | null>(null);
  const [msgBusy, setMsgBusy] = useState(false);

  const refreshNow = useCentralStore((s) => s.refreshNow);

  async function handleAssociate() {
    setAssocBusy(true);
    const result = await associate(sub.serial, otherSerial);
    setAssocBusy(false);
    setAssocStatus(result.ok ? { ok: true, message: "Saved" } : { ok: false, message: result.error ?? "Unknown error" });
    if (result.ok) refreshNow();
  }

  async function handleSend() {
    setMsgBusy(true);
    const result = await sendMessage(sub.serial, msgTo, msgContent);
    setMsgBusy(false);
    setMsgStatus(result.ok ? { ok: true, message: "Sent" } : { ok: false, message: result.error ?? "Unknown error" });
    if (result.ok) {
      setMsgContent("");
      refreshNow();
    }
  }

  return (
    <div className="flex flex-col gap-5">
      <div className="flex flex-col gap-2">
        <Label>Associate with other combat submarine (serial)</Label>
        <div className="flex gap-2">
          <Input placeholder="e.g. C-2" value={otherSerial} onChange={(e) => setOtherSerial(e.target.value)} />
          <Button size="sm" disabled={assocBusy || !otherSerial} onClick={handleAssociate}>
            Associate
          </Button>
        </div>
        {assocStatus && (
          <p className={`rounded-md px-2 py-1.5 text-xs font-medium ${assocStatus.ok ? "bg-good-soft text-good" : "bg-bad-soft text-bad"}`}>
            {assocStatus.message}
          </p>
        )}
        <p className="text-xs text-muted-foreground">
          Both submarines must already have an active mission before they can be associated.
        </p>
      </div>

      <div className="flex flex-col gap-2 border-t border-border pt-4">
        <Label>Send message to (serial)</Label>
        <Input placeholder="Recipient serial" value={msgTo} onChange={(e) => setMsgTo(e.target.value)} />
        <Label>Message</Label>
        <Input placeholder="Message text" value={msgContent} onChange={(e) => setMsgContent(e.target.value)} />
        <Button size="sm" disabled={msgBusy || !msgTo || !msgContent} onClick={handleSend}>
          Send message
        </Button>
        {/* Errors here (e.g. "both must be combat submarines associated with
            the same mission") are shown as a visible colored banner, not a
            thin status line easy to miss underneath a button. */}
        {msgStatus && (
          <p className={`rounded-md px-2 py-1.5 text-xs font-medium ${msgStatus.ok ? "bg-good-soft text-good" : "bg-bad-soft text-bad"}`}>
            {msgStatus.message}
          </p>
        )}
      </div>
    </div>
  );
}
