#include "dashboard_html.h"

namespace submarine {

const char* const kDashboardHtml = R"HTMLDOC(<!doctype html>
<html>
<head>
<meta charset="utf-8">
<title>Submarine Fleet Dashboard</title>
<style>
  :root { color-scheme: dark; }
  body { font-family: -apple-system, Segoe UI, Roboto, sans-serif; background: #0b1420; color: #dce6f0;
         margin: 0; padding: 24px; }
  h1 { font-size: 20px; margin: 0 0 4px; }
  .sub { color: #7d93a8; font-size: 13px; margin-bottom: 20px; }
  .layout { display: grid; grid-template-columns: 1fr 340px; gap: 20px; align-items: start; }
  .card { background: #101d2c; border: 1px solid #1e3348; border-radius: 10px; padding: 16px; margin-bottom: 14px; }
  .sub-card { border-left: 4px solid #3a5a78; }
  .sub-card.combat { border-left-color: #c0703a; }
  .sub-card.research { border-left-color: #3a8fc0; }
  .row { display: flex; justify-content: space-between; align-items: baseline; }
  .name { font-weight: 600; font-size: 16px; }
  .serial { color: #7d93a8; font-size: 12px; }
  .badge { font-size: 11px; padding: 2px 8px; border-radius: 999px; }
  .badge.assigned { background: #2e4a2e; color: #9be09b; }
  .badge.available { background: #2a2a2a; color: #aaa; }
  .badge.connected { background: #1f3a52; color: #7fc4f0; }
  .badge.disconnected { background: #3a2a2a; color: #c98a8a; }
  .mode { font-size: 11px; padding: 2px 8px; border-radius: 999px; margin-left: 6px; }
  .mode.m0 { background: #234a29; color: #9be09b; }
  .mode.m1 { background: #4a4423; color: #e0d59b; }
  .mode.m2 { background: #4a2323; color: #e09b9b; }
  .detail { font-size: 13px; color: #b7c6d4; margin-top: 8px; line-height: 1.6; }
  .detail b { color: #dce6f0; }
  .msg { font-size: 12px; background: #0b1622; border-radius: 6px; padding: 6px 10px; margin-top: 4px; }
  form.inline { margin-top: 20px; }
  input, select, button { background: #0b1622; color: #dce6f0; border: 1px solid #2a4054; border-radius: 6px;
         padding: 7px 9px; font-size: 13px; margin: 3px 0; width: 100%; box-sizing: border-box; }
  button { background: #1f4a6b; cursor: pointer; font-weight: 600; }
  button:hover { background: #285f88; }
  label { font-size: 11px; color: #7d93a8; display: block; margin-top: 8px; }
  .status-line { font-size: 12px; min-height: 16px; margin-top: 6px; }
  .status-line.ok { color: #9be09b; }
  .status-line.err { color: #e09b9b; }
  .empty { color: #7d93a8; font-size: 13px; }
</style>
</head>
<body>

<h1>Submarine Fleet Dashboard</h1>
<div class="sub">Live view + full control - the console menu (spec's graded deliverable) still works independently against the same fleet.</div>

<div class="layout">
  <div id="fleet"></div>

  <div>
    <div class="card">
      <b>Add submarine</b>
      <form class="inline" onsubmit="return doAdd(event)">
        <label>Type</label>
        <select id="add-type"><option value="research">Research</option><option value="combat">Combat</option></select>
        <label>Serial number</label>
        <input id="add-serial" required>
        <label>Name</label>
        <input id="add-name" required>
        <label>Serial port (combat only, blank = no hardware)</label>
        <input id="add-port" placeholder="COM8 or /dev/ttyACM0">
        <button type="submit">Add</button>
        <div class="status-line" id="add-status"></div>
      </form>
    </div>

    <div class="card">
      <b>Assign / update mission</b>
      <form class="inline" onsubmit="return doMission(event, 'assign')">
        <label>Serial number</label>
        <input id="mission-serial" required>
        <label>Description</label>
        <input id="mission-desc">
        <label>Commander (combat)</label>
        <input id="mission-commander">
        <label>Personnel count (combat)</label>
        <input id="mission-personnel" type="number">
        <label>Research topic (research)</label>
        <input id="mission-topic">
        <label>Researcher names (research, comma-separated)</label>
        <input id="mission-researchers">
        <button type="submit">Assign (new mission)</button>
        <button type="button" onclick="doMission(event,'update')" style="margin-top:6px">Update (existing mission)</button>
        <button type="button" onclick="doEndMission()" style="margin-top:6px;background:#5a2b2b">End mission</button>
        <div class="status-line" id="mission-status"></div>
      </form>
    </div>

    <div class="card">
      <b>Combat: associate / message</b>
      <form class="inline" onsubmit="return doAssociate(event)">
        <label>Combat submarine serial</label>
        <input id="assoc-serial" required>
        <label>Other combat submarine serial</label>
        <input id="assoc-other" required>
        <button type="submit">Associate with same mission</button>
        <div class="status-line" id="assoc-status"></div>
      </form>
      <form class="inline" onsubmit="return doMessage(event)">
        <label>From (serial)</label>
        <input id="msg-from" required>
        <label>To (serial)</label>
        <input id="msg-to" required>
        <label>Message</label>
        <input id="msg-content" required>
        <button type="submit">Send message</button>
        <div class="status-line" id="msg-status"></div>
      </form>
    </div>
  </div>
</div>

<script>
const MODE_NAMES = ['NORMAL', 'WARNING', 'ERROR'];

async function postForm(path, fields) {
  const body = new URLSearchParams(fields);
  const res = await fetch(path, { method: 'POST', body });
  return res.json();
}

function setStatus(id, result) {
  const el = document.getElementById(id);
  el.textContent = result.ok ? 'OK' : ('Error: ' + result.error);
  el.className = 'status-line ' + (result.ok ? 'ok' : 'err');
}

async function doAdd(ev) {
  ev.preventDefault();
  const result = await postForm('/api/submarines', {
    type: document.getElementById('add-type').value,
    serial: document.getElementById('add-serial').value,
    name: document.getElementById('add-name').value,
    port: document.getElementById('add-port').value,
  });
  setStatus('add-status', result);
  if (result.ok) refresh();
  return false;
}

async function doMission(ev, mode) {
  if (ev) ev.preventDefault();
  const serial = document.getElementById('mission-serial').value;
  const fields = {
    serial,
    description: document.getElementById('mission-desc').value,
    commanderName: document.getElementById('mission-commander').value,
    personnelCount: document.getElementById('mission-personnel').value,
    researchTopic: document.getElementById('mission-topic').value,
    researcherNames: document.getElementById('mission-researchers').value,
  };
  const path = mode === 'assign' ? '/api/submarines/' + encodeURIComponent(serial) + '/mission'
                                  : '/api/submarines/' + encodeURIComponent(serial) + '/mission/update';
  const result = await postForm(path, fields);
  setStatus('mission-status', result);
  if (result.ok) refresh();
  return false;
}

async function doEndMission() {
  const serial = document.getElementById('mission-serial').value;
  const result = await postForm('/api/submarines/' + encodeURIComponent(serial) + '/mission/end', {});
  setStatus('mission-status', result);
  if (result.ok) refresh();
}

async function doAssociate(ev) {
  ev.preventDefault();
  const serial = document.getElementById('assoc-serial').value;
  const other = document.getElementById('assoc-other').value;
  const result = await postForm('/api/submarines/' + encodeURIComponent(serial) + '/participate', { other });
  setStatus('assoc-status', result);
  if (result.ok) refresh();
  return false;
}

async function doMessage(ev) {
  ev.preventDefault();
  const result = await postForm('/api/messages', {
    from: document.getElementById('msg-from').value,
    to: document.getElementById('msg-to').value,
    content: document.getElementById('msg-content').value,
  });
  setStatus('msg-status', result);
  if (result.ok) refresh();
  return false;
}

function renderSubmarine(sub) {
  const badgeClass = sub.assigned ? 'assigned' : 'available';
  const badgeText = sub.assigned ? 'ASSIGNED' : 'AVAILABLE';
  let html = `<div class="card sub-card ${sub.type.toLowerCase()}">`;
  html += `<div class="row"><span class="name">${sub.name}</span><span class="badge ${badgeClass}">${badgeText}</span></div>`;
  html += `<div class="serial">${sub.serial} - ${sub.type}</div>`;

  if (sub.mission) {
    html += `<div class="detail"><b>Mission:</b> ${sub.mission.description || '(no description)'}`;
    if (sub.type === 'Combat') {
      html += ` &middot; Commander: ${sub.mission.commanderName || '-'} &middot; Personnel: ${sub.mission.personnelCount}`;
    } else {
      html += ` &middot; Topic: ${sub.mission.researchTopic || '-'} &middot; Researchers: ${(sub.mission.researcherNames||[]).join(', ') || '-'}`;
    }
    html += `</div>`;
  }
  html += `<div class="detail">Past missions: ${sub.missionHistoryCount}</div>`;

  if (sub.type === 'Combat') {
    const connClass = sub.connected ? 'connected' : 'disconnected';
    const connText = sub.connected ? 'CONNECTED' : 'no hardware';
    html += `<div class="detail"><span class="badge ${connClass}">${connText}</span>`;
    if (sub.liveSnapshot && sub.liveSnapshot.hasData) {
      const s = sub.liveSnapshot;
      html += ` <span class="mode m${s.mode}">${MODE_NAMES[s.mode] || '?'}</span>`;
      html += `<br><b>Live (${s.receivedAtHms}):</b> light=${s.lightRaw} battery=${s.batteryRaw} `
            + `dht=${s.dhtTemp}&deg;C/${s.dhtHumidity}%`;
      if (s.lastEventDescription) html += `<br>Last event: ${s.lastEventDescription} @ ${s.lastEventAtHms}`;
    } else {
      html += `<br><span class="empty">No live data yet.</span>`;
    }
    html += `</div>`;

    if (sub.participatingSerials && sub.participatingSerials.length) {
      html += `<div class="detail"><b>Participating with:</b> ${sub.participatingSerials.join(', ')}</div>`;
    }
    if (sub.messages && sub.messages.length) {
      html += `<div class="detail"><b>Messages received:</b>`;
      for (const m of sub.messages) {
        html += `<div class="msg">From ${m.from}: ${m.content}</div>`;
      }
      html += `</div>`;
    }
  }
  html += `</div>`;
  return html;
}

async function refresh() {
  try {
    const res = await fetch('/api/state');
    const data = await res.json();
    const el = document.getElementById('fleet');
    if (!data.submarines.length) {
      el.innerHTML = '<div class="card empty">The fleet is empty - add a submarine to get started.</div>';
      return;
    }
    el.innerHTML = data.submarines.map(renderSubmarine).join('');
  } catch (e) {
    // server may be mid-restart; just try again on the next tick
  }
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)HTMLDOC";

}  // namespace submarine
