#pragma once

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>PassVault Keyboard</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI','Roboto',sans-serif;background:#0a0e17;color:#e0e6ed;min-height:100vh;display:flex;flex-direction:column}
header{padding:16px 24px;background:linear-gradient(135deg,#1a1f35,#0d1221);border-bottom:1px solid rgba(99,179,237,.15);text-align:center}
header h1{font-size:1.4em;font-weight:700;background:linear-gradient(90deg,#63b3ed,#b794f4);-webkit-background-clip:text;-webkit-text-fill-color:transparent}
header p{font-size:.8em;color:#718096;margin:2px 0 6px}
.status-bar{display:flex;align-items:center;gap:8px;justify-content:center}
.dot{width:10px;height:10px;border-radius:50%;background:#f56565;transition:.3s}
.dot.on{background:#48bb78;box-shadow:0 0 8px rgba(72,187,120,.5)}
.stxt{font-size:.78em;color:#a0aec0}
.header-tools{display:flex;align-items:center;justify-content:center;gap:8px;flex-wrap:wrap}
.stats-toggle{min-height:36px;border:1px solid rgba(99,179,237,.22);background:rgba(99,179,237,.07);color:#90cdf4;border-radius:8px;padding:7px 12px;font-size:.72em;font-weight:700;cursor:pointer}
.stats-toggle[aria-expanded="true"]{border-color:rgba(72,187,120,.35);background:rgba(72,187,120,.09);color:#68d391}
.container{max-width:860px;margin:0 auto;width:100%;padding:0 24px 24px;flex:1;display:flex;flex-direction:column}

/* Vault Card */
.vault-shell{width:100%;max-width:860px;margin:16px auto 0;padding:0 24px}
.vault-card{position:relative;overflow:hidden;background:linear-gradient(145deg,rgba(21,28,47,.98),rgba(13,18,33,.98));border:1px solid rgba(99,179,237,.16);border-radius:16px;box-shadow:0 18px 60px rgba(0,0,0,.22)}
.vault-card::before{content:'';position:absolute;inset:0 0 auto;height:2px;background:linear-gradient(90deg,#63b3ed,#b794f4,transparent)}
.vault-head{display:flex;align-items:flex-start;justify-content:space-between;gap:16px;padding:20px 20px 14px}
.vault-title-wrap{display:flex;align-items:center;gap:12px}
.vault-mark{width:42px;height:42px;display:grid;place-items:center;flex:0 0 auto;border-radius:12px;color:#d6bcfa;font-size:1.25em;background:linear-gradient(145deg,rgba(99,179,237,.15),rgba(183,148,244,.16));border:1px solid rgba(183,148,244,.22)}
.vault-eyebrow{color:#718096;font-size:.67em;letter-spacing:.12em;text-transform:uppercase;margin-bottom:3px}
.vault-head h2{font-size:1.18em;line-height:1.2;color:#f7fafc}
.vault-head p{margin-top:5px;color:#8b98aa;font-size:.76em}
.vault-lab-note{display:inline-flex;align-items:center;gap:6px;margin:0 20px 14px;padding:6px 10px;border-radius:7px;color:#a0aec0;font-size:.7em;background:rgba(99,179,237,.06);border:1px solid rgba(99,179,237,.1)}
.vault-lab-note i{width:6px;height:6px;border-radius:50%;background:#48bb78;box-shadow:0 0 8px rgba(72,187,120,.45)}
.vault-toolbar{display:flex;gap:8px;padding:0 20px 14px}
.vault-search,.vault-field{width:100%;min-width:0;border:1px solid rgba(99,179,237,.15);background:rgba(8,13,24,.72);color:#e0e6ed;border-radius:9px;padding:9px 12px;outline:none;font-size:.78em}
.vault-search:focus,.vault-field:focus{border-color:rgba(99,179,237,.45);box-shadow:0 0 0 3px rgba(99,179,237,.07)}
.vault-search::placeholder,.vault-field::placeholder{color:#4a5568}
.vault-add{flex:0 0 auto;border:1px solid rgba(183,148,244,.38);background:rgba(183,148,244,.12);color:#d6bcfa;border-radius:9px;padding:9px 14px;cursor:pointer;font-size:.75em;font-weight:700;white-space:nowrap;transition:.2s}
.vault-add:hover{background:rgba(183,148,244,.22)}
.vault-compose{display:grid;grid-template-columns:1fr 1fr 1fr auto;gap:8px;margin:0 20px 14px;padding:12px;border-radius:11px;background:rgba(8,13,24,.45);border:1px solid rgba(183,148,244,.12)}
.vault-compose.hidden{display:none}
.vault-list{display:grid;gap:8px;padding:0 20px 20px}
.vault-item{display:grid;grid-template-columns:minmax(150px,1.1fr) minmax(140px,1fr) minmax(150px,1fr) auto;align-items:center;gap:12px;padding:11px 12px;border-radius:10px;background:rgba(255,255,255,.025);border:1px solid rgba(255,255,255,.045);transition:.2s}
.vault-item:hover{border-color:rgba(99,179,237,.16);background:rgba(99,179,237,.045)}
.vault-site{display:flex;align-items:center;min-width:0;gap:9px}
.vault-icon{width:31px;height:31px;display:grid;place-items:center;flex:0 0 auto;border-radius:8px;color:#90cdf4;background:rgba(99,179,237,.1);font-size:.78em;font-weight:800}
.vault-primary{color:#e7edf5;font-size:.78em;font-weight:700;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.vault-secondary{color:#718096;font-size:.68em;margin-top:2px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.vault-secret{color:#a0aec0;font-family:'Cascadia Code',Consolas,monospace;font-size:.76em;letter-spacing:.08em;white-space:nowrap}
.vault-secret.is-revealed{white-space:normal;overflow-wrap:anywhere;word-break:break-word;letter-spacing:.02em;line-height:1.45}
.vault-actions{display:flex;flex-wrap:wrap;align-items:center;justify-content:flex-start;gap:5px}
.vault-mini{flex:0 0 auto;width:auto;min-height:28px;border:1px solid rgba(99,179,237,.15);background:transparent;color:#8290a3;border-radius:6px;padding:3px 8px;cursor:pointer;font-size:.63em;line-height:1.2;white-space:nowrap;transition:.2s}
.vault-mini:hover{color:#e0e6ed;background:rgba(99,179,237,.08)}
.vault-empty{display:none;text-align:center;padding:22px;color:#718096;font-size:.76em}
.vault-empty.show{display:block}
.vault-pager{display:flex;align-items:center;justify-content:center;gap:8px;padding:0 20px 16px}
.vault-pager.is-hidden{display:none}
.vault-page-info{min-width:82px;text-align:center;color:#718096;font-size:.65em}
.vault-page-btn{min-height:28px;border:1px solid rgba(99,179,237,.15);background:transparent;color:#8290a3;border-radius:6px;padding:3px 9px;font-size:.64em;cursor:pointer}
.vault-page-btn:disabled{opacity:.32;cursor:not-allowed}
.pin-overlay{position:fixed;inset:0;z-index:50;display:grid;place-items:center;padding:16px;background:rgba(2,6,14,.78);backdrop-filter:blur(4px)}
.pin-overlay.is-hidden{display:none}
.pin-dialog{width:min(360px,100%);border:1px solid rgba(183,148,244,.28);border-radius:13px;padding:16px;background:#111827;box-shadow:0 20px 70px rgba(0,0,0,.55)}
.pin-dialog h3{font-size:.95em;color:#e7edf5;margin-bottom:4px}
.pin-dialog p{font-size:.7em;color:#718096;line-height:1.45;margin-bottom:10px}
.pin-field{width:100%;min-height:44px;border:1px solid rgba(183,148,244,.24);background:#080d18;color:#e7edf5;border-radius:8px;padding:9px 12px;font-size:16px;outline:none;text-align:center;letter-spacing:.18em}
.pin-field:focus{border-color:#b794f4}
.pin-error{min-height:18px;margin-top:6px;color:#fc8181;font-size:.68em}
.pin-actions{display:flex;justify-content:flex-end;gap:7px;margin-top:8px}

.vault-lock{position:relative;display:grid;place-items:center;min-height:180px;padding:24px}
.vault-lock .vault-lock-inner{text-align:center;max-width:320px}
.vault-lock .vault-lock-icon{font-size:2.4em;margin-bottom:10px}
.vault-lock .vault-lock-title{font-size:.95em;color:#e7edf5;font-weight:700;margin-bottom:4px}
.vault-lock .vault-lock-hint{font-size:.7em;color:#718096;margin-bottom:12px}
.vault-lock .vault-lock-input{width:100%;min-height:44px;border:1px solid rgba(183,148,244,.24);background:#080d18;color:#e7edf5;border-radius:8px;padding:9px 12px;font-size:16px;outline:none;text-align:center}
.vault-lock .vault-lock-input:focus{border-color:#b794f4}
.vault-lock .vault-lock-error{min-height:18px;margin-top:6px;color:#fc8181;font-size:.68em}
.vault-lock .vault-lock-btn{margin-top:8px;width:100%;min-height:40px;border:1px solid rgba(183,148,244,.38);background:rgba(183,148,244,.12);color:#d6bcfa;border-radius:9px;padding:9px 14px;cursor:pointer;font-size:.78em;font-weight:700}
.vault-lock .vault-lock-btn:hover{background:rgba(183,148,244,.22)}
.vault-card:not(.vault-unlocked) .vault-head,
.vault-card:not(.vault-unlocked) .vault-lab-note,
.vault-card:not(.vault-unlocked) .vault-toolbar,
.vault-card:not(.vault-unlocked) .vault-compose,
.vault-card:not(.vault-unlocked) .vault-list,
.vault-card:not(.vault-unlocked) .vault-pager{display:none}
.vault-unlocked .vault-lock{display:none}

/* Optional local stats: closed by default and rendered after the vault. */
.stats-panel{width:100%;max-width:860px;margin:12px auto 0;padding:0 24px}
.stats-card,.safe-card{background:#111827;border:1px solid rgba(99,179,237,.12);border-radius:12px;padding:14px}
.panel-head{display:flex;align-items:flex-start;justify-content:space-between;gap:10px;margin-bottom:10px}
.panel-title{font-size:.78em;color:#90cdf4;font-weight:800;letter-spacing:.04em;text-transform:uppercase}
.panel-note{font-size:.66em;color:#718096;margin-top:3px}
.stats-grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:8px}
.stat-box{border:1px solid rgba(255,255,255,.05);background:rgba(255,255,255,.025);border-radius:9px;padding:10px}
.stat-value{font-size:1.15em;color:#e7edf5;font-weight:800;overflow-wrap:anywhere}
.stat-label{font-size:.63em;color:#718096;margin-top:2px}
.activity-tools{display:flex;gap:7px;flex-wrap:wrap;margin-top:10px}
.safe-btn{min-height:30px;border:1px solid rgba(99,179,237,.16);background:rgba(99,179,237,.055);color:#9aabc1;border-radius:7px;padding:4px 9px;font-size:.64em;cursor:pointer;white-space:nowrap}
.safe-btn:hover{color:#e7edf5;background:rgba(99,179,237,.1)}
.safe-btn:disabled{cursor:not-allowed;opacity:.46;border-style:dashed}
.activity-list{max-height:120px;overflow:auto;margin-top:9px;padding:8px 10px;border-radius:8px;background:rgba(8,13,24,.58);color:#718096;font-family:Consolas,monospace;font-size:.66em;line-height:1.6}
.activity-empty{color:#4a5568}
.is-hidden{display:none!important}
.toggle-row{display:flex;align-items:center;justify-content:space-between;padding:6px 0}
.toggle-row .toggle-label{color:#a0aec0;font-size:.75em;font-weight:600}
.toggle-switch{position:relative;width:36px;height:20px;flex:0 0 auto}
.toggle-switch input{opacity:0;width:0;height:0}
.toggle-switch .slider{position:absolute;inset:0;background:#2d3748;border-radius:20px;cursor:pointer;transition:.25s}
.toggle-switch .slider::before{content:'';position:absolute;left:2px;bottom:2px;width:16px;height:16px;background:#718096;border-radius:50%;transition:.25s}
.toggle-switch input:checked+.slider{background:rgba(99,179,237,.35)}
.toggle-switch input:checked+.slider::before{transform:translateX(16px);background:#63b3ed}

/* Safe placeholders preserve the classroom UI structure without payloads. */
.lab-placeholder-panel{max-width:860px;margin:12px auto 0;width:100%;padding:0 24px}
.lab-placeholder-card{background:#111827;border:1px solid rgba(183,148,244,.12);border-radius:12px;padding:14px}
.lab-placeholder-head{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:10px}
.lab-placeholder-title{font-size:.76em;color:#b794f4;font-weight:700;letter-spacing:.04em;text-transform:uppercase}
.lab-placeholder-note{font-size:.66em;color:#718096}
.lab-placeholder-grid{display:flex;flex-wrap:wrap;gap:7px}
.lab-placeholder{min-height:30px;border:1px solid rgba(183,148,244,.3);background:rgba(183,148,244,.07);color:#b794f4;border-radius:6px;padding:4px 9px;font-size:.64em;text-align:center;white-space:nowrap}
.lab-placeholder:nth-child(-n+6){border-color:rgba(245,101,101,.32);color:#fc8181;background:rgba(245,101,101,.055)}
.lab-placeholder:nth-child(6n+2){border-color:rgba(99,179,237,.32);color:#63b3ed;background:rgba(99,179,237,.055)}
.lab-placeholder:nth-child(6n+4){border-color:rgba(72,187,120,.32);color:#68d391;background:rgba(72,187,120,.055)}
.lab-placeholder:disabled{cursor:not-allowed;opacity:.78}
.general-action{cursor:pointer!important;opacity:1!important;border-style:solid!important;color:#68d391!important;border-color:rgba(72,187,120,.38)!important;background:rgba(72,187,120,.08)!important}
.action-add-row{display:flex;align-items:center;gap:7px;margin:8px 0 12px}
.action-add-row .kb-send-input{min-height:32px;padding:4px 9px;font-size:.68em}
.general-action-wrap{display:inline-flex;align-items:stretch}
.general-action-wrap .general-action{border-radius:6px 0 0 6px}
.general-action-remove{border:1px solid rgba(245,101,101,.28);border-left:0;background:rgba(245,101,101,.06);color:#fc8181;border-radius:0 6px 6px 0;padding:3px 7px;font-size:.65em;cursor:pointer}
.general-action.run-action{color:#63b3ed!important;border-color:rgba(99,179,237,.38)!important;background:rgba(99,179,237,.08)!important}
.general-action.paste-action{color:#f6ad55!important;border-color:rgba(246,173,85,.38)!important;background:rgba(246,173,85,.08)!important}
.general-action.macro-action{color:#b794f4!important;border-color:rgba(183,148,244,.38)!important;background:rgba(183,148,244,.08)!important}
.general-action.pwsh-action{color:#4fd1c5!important;border-color:rgba(79,209,197,.38)!important;background:rgba(79,209,197,.08)!important}
.macro-step{background:rgba(8,13,24,.55);border-radius:9px;padding:10px;margin-bottom:6px;border:1px solid rgba(99,179,237,.08)}
.macro-step-head{display:flex;align-items:center;justify-content:space-between;margin-bottom:6px}
.macro-step-num{font-size:.62em;color:#718096}
.macro-step-del{font-size:.62em;color:#fc8181;cursor:pointer;background:none;border:none;padding:2px 4px}
.macro-step-fields{display:flex;flex-direction:column;gap:6px}
.macro-step select,.macro-step input{min-height:36px;border:1px solid rgba(99,179,237,.15);background:rgba(8,13,24,.72);color:#e0e6ed;border-radius:7px;padding:6px 10px;font-size:.78em;width:100%;box-sizing:border-box}
.macro-add-btns{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:6px;margin:8px 0}
.macro-add-btn{min-height:36px;font-size:.68em;padding:6px 8px;background:rgba(99,179,237,.08);border:1px solid rgba(99,179,237,.15);border-radius:7px;color:#63b3ed;cursor:pointer}
.builder-open{margin:9px 0 12px}
.action-builder{width:min(430px,100%);max-height:90vh;overflow:auto}
.builder-section{margin-top:10px}.builder-label{color:#718096;font-size:.65em;font-weight:700;text-transform:uppercase;letter-spacing:.05em;margin-bottom:6px}
.key-suggestions{display:flex;flex-wrap:wrap;gap:6px}.key-chip{min-height:30px;border:1px solid rgba(99,179,237,.2);background:rgba(99,179,237,.05);color:#90cdf4;border-radius:6px;padding:4px 9px;font-size:.66em;cursor:pointer}.key-chip.modifier{color:#d6bcfa;border-color:rgba(183,148,244,.28);background:rgba(183,148,244,.07)}.key-chip.selected{color:#08111d;background:#90cdf4;border-color:#90cdf4;font-weight:800}
.builder-fields{display:grid;gap:8px}.action-editor-help{width:100%;color:#718096;font-size:.64em;line-height:1.4}

/* Web Keyboard */
.kb-panel{max-width:860px;margin:12px auto 0;width:100%;padding:0 24px}
.kb-toggle{display:flex;align-items:center;gap:8px;margin-bottom:8px}
.kb-toggle label{font-size:.78em;color:#718096;cursor:pointer;display:flex;align-items:center;gap:4px}
.kb-area{width:100%;min-height:80px;background:#111827;border:1px solid rgba(99,179,237,.15);border-radius:10px;padding:14px;color:#63b3ed;font-family:'Cascadia Code',Consolas,monospace;font-size:1em;outline:none;caret-color:#63b3ed;resize:none}
.kb-area:focus{border-color:rgba(99,179,237,.4);box-shadow:0 0 12px rgba(99,179,237,.1)}
.kb-area::placeholder{color:#4a5568}
.kb-info{font-size:.7em;color:#4a5568;margin-top:4px;text-align:center}
.kb-hidden{display:none}
.kb-card{background:#111827;border:1px solid rgba(99,179,237,.12);border-radius:12px;padding:14px}
.kb-toggle-row{display:flex;align-items:center;justify-content:space-between;gap:8px;flex-wrap:wrap}
.kb-controls{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}
.kb-shortcuts{display:flex;flex-wrap:wrap;gap:6px;margin-top:8px}
.kb-section-label{margin-top:12px;color:#718096;font-size:.67em;font-weight:700;text-transform:uppercase;letter-spacing:.05em}
.kb-tool-columns{display:grid;grid-template-columns:1fr 1.35fr;gap:18px}
.kb-indicators{display:flex;align-items:center;gap:8px;flex-wrap:wrap;margin:8px 0}
.kb-indicator{border:1px solid rgba(99,179,237,.18);border-radius:7px;padding:5px 9px;color:#607086;background:rgba(99,179,237,.035);font-size:.68em}
.kb-send-row{display:grid;grid-template-columns:1fr auto;gap:8px;margin-top:10px}
.kb-send-input{min-width:0;min-height:42px;border:1px solid rgba(99,179,237,.15);background:rgba(8,13,24,.72);color:#e0e6ed;border-radius:9px;padding:9px 12px;outline:none;font-size:.82em}
.kb-send-input:focus{border-color:rgba(99,179,237,.45)}

footer{text-align:center;padding:8px;font-size:.7em;color:#2d3748}

@media(max-width:600px){
  header{padding:14px 12px}
  .vault-shell{padding:0 10px;margin-top:10px}
  .vault-card{border-radius:13px}
  .vault-head{flex-direction:column;padding:16px 14px 12px}
  .vault-head .vault-add{width:100%;min-height:44px}
  .vault-lab-note{margin:0 14px 12px;line-height:1.45}
  .vault-toolbar{padding:0 14px 12px}
  .vault-search,.vault-field{min-height:44px;font-size:.9em}
  .vault-toolbar,.vault-compose{grid-template-columns:1fr;flex-direction:column}
  .vault-compose{display:grid;margin:0 14px 12px;padding:10px}
  .vault-compose.hidden{display:none}
  .vault-list{padding:0 10px 14px}
  .vault-pager{padding:0 10px 12px}
  .vault-item{grid-template-columns:minmax(0,1fr) auto;gap:7px 10px;padding:10px}
  .vault-site{grid-column:1/-1}
  .vault-user,.vault-secret,.vault-actions{grid-column:1}
  .vault-user{grid-column:1;min-width:0}
  .vault-secret{grid-column:2;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:128px;padding:5px 8px;border-radius:6px;background:rgba(8,13,24,.55)}
  .vault-secret.is-revealed{grid-column:1/-1;max-width:none;white-space:normal;overflow:visible;text-overflow:clip;padding:8px 9px}
  .vault-actions{grid-column:1/-1;display:flex!important;flex-wrap:wrap;gap:5px}
  .vault-mini{flex:0 0 auto!important;width:auto!important;min-height:28px;padding:3px 7px;font-size:.63em;text-align:center}
  .kb-panel,.lab-placeholder-panel,.stats-panel{padding:0 10px}
  .kb-toggle{align-items:flex-start;flex-wrap:wrap}
  .kb-area{min-height:110px;font-size:16px}
  .stats-grid{grid-template-columns:repeat(2,minmax(0,1fr))}
  .safe-btn{min-height:32px;font-size:.66em;padding:4px 8px}
  .kb-tool-columns{grid-template-columns:1fr;gap:4px}
  .kb-send-row{grid-template-columns:1fr}
  .action-add-row{align-items:stretch;flex-wrap:wrap}
  .action-add-row .kb-send-input{flex:1 1 130px;font-size:16px}
  .kb-send-input{font-size:16px}
  .lab-placeholder-head{align-items:flex-start;flex-direction:column}
  .lab-placeholder-grid{display:flex;flex-wrap:wrap}
  .lab-placeholder{min-height:32px;font-size:.66em;padding:4px 8px}
}
</style>
</head>
<body>
<header>
<h1>&#128274; PassVault Keyboard</h1>
<p>Encrypted Vault on ESP32 &mdash; Type usernames or passwords to PC via USB HID</p>
<div class="header-tools">
<div class="status-bar">
<div class="dot" id="dot"></div>
<span class="stxt" id="stxt">Disconnected</span>
</div>
<button class="stats-toggle" id="statsToggle" type="button" aria-expanded="false" onclick="toggleStats()">Stats: Off</button>
</div>
</header>

<section class="vault-shell">
<div class="vault-card" id="vaultCard">
<div class="vault-lock" id="vaultLock">
<div class="vault-lock-inner">
<div class="vault-lock-icon">&#128274;</div>
<div class="vault-lock-title">Vault Locked</div>
<div class="vault-lock-hint">Enter master password to access the vault</div>
<form onsubmit="unlockVault(event)">
<input class="vault-lock-input" id="vaultPassInput" type="password" placeholder="Master password" required autocomplete="off">
<div class="vault-lock-error" id="vaultPassError"></div>
<button class="vault-lock-btn" type="submit">Unlock</button>
</form>
</div>
</div>
<div class="vault-head">
<div class="vault-title-wrap">
<div class="vault-mark">&#128274;</div>
<div>
<div class="vault-eyebrow">AES-256 encrypted on device</div>
<h2>Password Vault</h2>
<p>Entries are encrypted at rest in ESP32 flash (NVS). Username and password can be typed separately.</p>
</div>
</div>
<button class="vault-add" type="button" onclick="toggleVaultForm()">+ New entry</button>
</div>
<div class="vault-lab-note"><i></i> Encrypted at rest in ESP32 NVS &middot; available to this connected lab page</div>
<div class="vault-toolbar">
<input class="vault-search" id="vaultSearch" type="search" placeholder="Search..." autocomplete="off">
</div>
<form class="vault-compose hidden" id="vaultCompose" onsubmit="addVaultEntry(event)">
<input class="vault-field" id="vaultService" placeholder="Service" required autocomplete="off">
<input class="vault-field" id="vaultUser" placeholder="Username" required autocomplete="off">
<input class="vault-field" id="vaultPassword" type="password" placeholder="Password" required autocomplete="new-password">
<button class="vault-add" type="submit">Add</button>
</form>
<div class="vault-list" id="vaultList">
<div class="vault-empty" id="vaultEmpty">No entries yet. Click "+ New entry" to add one.</div>
</div>
<div class="vault-pager is-hidden" id="vaultPager"><button class="vault-page-btn" id="vaultPrev" type="button" onclick="changeVaultPage(-1)">&#8249; Prev</button><span class="vault-page-info" id="vaultPageInfo"></span><button class="vault-page-btn" id="vaultNext" type="button" onclick="changeVaultPage(1)">Next &#8250;</button></div>
<div style="text-align:right;padding:4px 20px 12px"><button class="vault-mini" type="button" onclick="toggleChangePass()">Change master password</button></div>
<form class="vault-compose hidden" id="changePassForm" onsubmit="changeMasterPass(event)" style="margin:0 20px 14px">
<input class="vault-field" id="currentMasterPass" type="password" placeholder="Current password" required autocomplete="off">
<input class="vault-field" id="newMasterPass" type="password" placeholder="New password (4-64 chars)" minlength="4" maxlength="64" required autocomplete="new-password">
<input class="vault-field" id="confirmMasterPass" type="password" placeholder="Confirm new password" minlength="4" maxlength="64" required autocomplete="new-password">
<button class="vault-add" type="submit">Save</button>
</form>
</div>
</section>

<section class="stats-panel is-hidden" id="statsPanel" aria-label="Local activity stats">
<div class="stats-card">
<div class="panel-head"><div><div class="panel-title">Local Stats</div><div class="panel-note">Closed by default · appears after the vault</div></div></div>
<div class="stats-grid">
<div class="stat-box"><div class="stat-value" id="statSent">0</div><div class="stat-label">Keys sent</div></div>
<div class="stat-box"><div class="stat-value" id="statVault">0</div><div class="stat-label">Vault entries</div></div>
<div class="stat-box"><div class="stat-value" id="statSession">00:00</div><div class="stat-label">Session</div></div>
<div class="stat-box"><div class="stat-value" id="statLink">Offline</div><div class="stat-label">Connection</div></div>
</div>
<div class="activity-tools">
<button class="safe-btn" type="button" onclick="releaseKeys()">Release key state</button>
<button class="safe-btn" type="button" onclick="exportActivity()">Export CSV</button>
<button class="safe-btn" type="button" onclick="clearActivity()">Clear activity</button>
</div>
<div class="activity-list" id="activityList"><span class="activity-empty">No local activity yet.</span></div>
</div>
</section>

<div class="kb-panel">
<div class="kb-card">
<div class="kb-toggle-row">
<div class="kb-toggle">
<label><input type="checkbox" id="chkKb" style="accent-color:#b794f4" onchange="toggleKb()" checked> &#9000; Web Keyboard</label>
<label><input type="checkbox" style="accent-color:#a0aec0" disabled> Thai KB</label>
<span style="font-size:.7em;color:#4a5568">(type here to send keys to PC)</span>
</div>
<button class="safe-btn" type="button" onclick="if(ws&amp;&amp;ws.readyState===1)ws.send('LANG:DETECT')" title="Detect PC keyboard language via CapsLock probe">Detect Lang</button>
<button class="safe-btn" type="button" onclick="tapShortcut('switchLang')" title="Send Alt+Shift to switch PC keyboard language">Switch Lang (Alt+Shift)</button>
</div>
<div class="kb-indicators"><span class="kb-indicator">CapsLock: OFF</span><span class="kb-indicator" id="langFlag">🏳️ ?</span></div>
<textarea class="kb-area" id="kbInput" placeholder="Click here and type to send keystrokes to PC..." autocomplete="off" autocorrect="off" autocapitalize="off" spellcheck="false"></textarea>
<div id="kbTools">
<div class="toggle-row"><span class="toggle-label">Keys &amp; Shortcuts</span><label class="toggle-switch"><input type="checkbox" onchange="document.getElementById('kbToolsContent').classList.toggle('is-hidden',!this.checked)"><span class="slider"></span></label></div>
<div class="kb-tool-columns is-hidden" id="kbToolsContent">
<div>
<div class="kb-section-label">Keys</div>
<div class="kb-controls">
<button class="safe-btn" type="button" onclick="tapNamedKey('Tab')">Tab</button><button class="safe-btn" type="button" onclick="tapNamedKey('Escape')">Esc</button><button class="safe-btn" type="button" onclick="tapNamedKey('Enter')">Enter</button><button class="safe-btn" type="button" onclick="tapNamedKey('Backspace')">Backspace</button><button class="safe-btn" type="button" onclick="tapNamedKey('Delete')">Delete</button><button class="safe-btn" type="button" onclick="tapNamedKey('Home')">Home</button><button class="safe-btn" type="button" onclick="tapNamedKey('End')">End</button><button class="safe-btn" type="button" onclick="tapNamedKey('PageUp')">PgUp</button><button class="safe-btn" type="button" onclick="tapNamedKey('PageDown')">PgDn</button><button class="safe-btn" type="button" onclick="tapNamedKey('ArrowUp')">Up</button><button class="safe-btn" type="button" onclick="tapNamedKey('ArrowDown')">Down</button><button class="safe-btn" type="button" onclick="tapNamedKey('ArrowLeft')">Left</button><button class="safe-btn" type="button" onclick="tapNamedKey('ArrowRight')">Right</button>
</div>
</div>
<div>
<div class="kb-section-label">Safe shortcuts</div>
<div class="kb-shortcuts">
<button class="safe-btn" type="button" onclick="tapNamedKey('CapsLock')">CapsLock</button><button class="safe-btn" type="button" onclick="tapShortcut('copy')">Ctrl+C</button><button class="safe-btn" type="button" onclick="tapShortcut('paste')">Ctrl+V</button><button class="safe-btn" type="button" onclick="tapShortcut('undo')">Ctrl+Z</button><button class="safe-btn" type="button" onclick="tapShortcut('selectAll')">Ctrl+A</button><button class="safe-btn" type="button" onclick="tapShortcut('switchApp')">Alt+Tab</button><button class="safe-btn" type="button" onclick="tapNamedKey('PrintScreen')">Print Screen</button><button class="safe-btn" type="button" onclick="tapShortcut('runDialog')">Win+R</button><button class="safe-btn" type="button" onclick="if(confirm('Send Alt+F4?'))tapShortcut('altF4')">Alt+F4</button><button class="safe-btn" type="button" onclick="shutdownPC()">Shutdown</button>
</div>
</div>
</div>
<div class="kb-send-row"><input class="kb-send-input" id="typeInput" type="text" placeholder="Type text to send..." autocomplete="off"><button class="safe-btn" type="button" onclick="sendTypedText()">Send text</button></div>
</div>
<div class="kb-info" id="kbInfo">Keys typed here are sent as USB HID to the PC via ESP32</div>
</div>
</div>

<section class="lab-placeholder-panel" aria-labelledby="labPlaceholderTitle">
<div class="lab-placeholder-card">
<div class="lab-placeholder-head">
<div class="lab-placeholder-title" id="labPlaceholderTitle">Actions</div>

</div>
<div class="kb-section-label">General actions</div>
<div class="lab-placeholder-grid" id="generalActionGrid"></div>
<button class="safe-btn builder-open" type="button" onclick="openActionBuilder()">+ Add custom button</button>
<div class="toggle-row"><span class="toggle-label">Lab Placeholders</span><label class="toggle-switch"><input type="checkbox" onchange="document.getElementById('labPlaceholderContent').classList.toggle('is-hidden',!this.checked)"><span class="slider"></span></label></div>
<div class="lab-placeholder-grid is-hidden" id="labPlaceholderContent">
<button class="lab-placeholder" type="button" onclick="if(confirm('Shutdown in 10s?'))ws.send('LAB:shutdown10s')">&#128309; Screen demo</button>
<button class="lab-placeholder" type="button" onclick="if(confirm('Shutdown in 10min?'))ws.send('LAB:shutdown10m')">&#9200; Scheduled rest</button>
<button class="lab-placeholder" type="button" onclick="if(confirm('SAM dump (lab): dumps SAM/SYSTEM/SECURITY hives to C:\\LabSAM. Approve the UAC prompt when it appears.'))ws.send('LAB:SAMDump')">&#128273; SAM dump (lab)</button>
<button class="lab-placeholder" type="button" onclick="if(confirm('Screenshot '))ws.send('LAB:screenshot')">&#128247; Screenshot (lab)</button>
<button class="lab-placeholder" type="button" onclick="if(confirm('Clipboard capture (T1115): saves clipboard text to C:\\LabOut\\clip.txt and opens it. Lab demo — nothing leaves this PC.'))ws.send('LAB:BUTTON1')">&#128203; Clipboard (T1115)</button>
<button class="lab-placeholder" type="button" onclick="if(confirm('Wi-Fi profile discovery (T1016): lists saved Wi-Fi profiles and keys to C:\\LabOut\\wlan.txt. Lab demo — local file only.'))ws.send('LAB:Bu2')">&#128225; Wi-Fi profiles (T1016)</button>
<button class="lab-placeholder" type="button" onclick="if(confirm('Account & privilege discovery (T1087): whoami /all + net user saved to C:\\LabOut\\who.txt. Lab demo — read-only.'))ws.send('LAB:Bu3')">&#128100; Accounts (T1087)</button>
<button class="lab-placeholder" type="button" onclick="if(confirm('Network connection discovery (T1049): netstat + ipconfig saved to C:\\LabOut\\net.txt. Lab demo — read-only.'))ws.send('LAB:Bu4')">&#128268; Connections (T1049)</button>
<button class="lab-placeholder" type="button" onclick="if(confirm('Scheduled task discovery (T1053): task list saved to C:\\LabOut\\tasks.txt. Lab demo — read-only.'))ws.send('LAB:Bu5')">&#128197; Tasks (T1053)</button>
<button class="lab-placeholder" type="button" onclick="if(confirm('Security posture discovery (T1518): Defender status + firewall rules saved to C:\\LabOut\\def.txt. Lab demo — read-only.'))ws.send('LAB:Bu6')">&#128737; Defender+FW (T1518)</button>
<button class="lab-placeholder" type="button" onclick="if(confirm('PERSISTENCE demo (T1547.001): adds HKCU Run key LabPersist that opens a notepad file at next logon. User-level, no UAC. Remove with: reg delete HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Run /v LabPersist /f — OK to continue?'))ws.send('LAB:persist')">&#128257; Persistence (T1547)</button>
<button class="lab-placeholder" type="button" onclick="if(confirm('ADMIN persistence demo (T1053.005 + T1548.002): creates scheduled task LabPersistTask (highest privileges, runs at logon). The device auto-approves the UAC prompt with Alt+Y — demonstrating hardware HID input reaches the secure desktop. Remove with: schtasks /delete /tn LabPersistTask /f — OK to continue?'))ws.send('LAB:persistAdmin')">&#11014;&#65039; Persist Admin (T1053)</button>
<button class="lab-placeholder" type="button" onclick="if(confirm('Browser harvest (T1555.003): opens an elevated PowerShell console (UAC auto-approved), closes Chrome/Edge/Firefox, then copies Login Data, Cookies, Local State, Firefox logins.json/key4.db and DPAPI master keys to C:\\LabOut\\Browser. Collection only — stays in the VM — OK to continue?'))ws.send('LAB:browser')">&#127760; Browser harvest (T1555)</button>
<button class="lab-placeholder" type="button" onclick="if(confirm('LSASS memory dump (T1003.001): dumps live credentials of every logged-in account (incl. domain + tickets) to C:\\LabSAM\\lsass.dmp via Windows own comsvcs.dll. Elevated (UAC auto-approved). Defender may block it — the block + Sysmon EID 10 is part of the lesson. OK to continue?'))ws.send('LAB:lsass')">&#129504; LSASS dump (T1003.001)</button>
<button class="lab-placeholder" type="button" onclick="if(confirm('Decrypt passwords: downloads the offline decryptor from the course repo and runs it on the VM (needs internet + Python). Prints saved Chrome/Edge/Firefox passwords on screen. Nothing leaves the VM — OK to continue?'))ws.send('LAB:decrypt')">&#128273; Decrypt passwords</button>
<button class="lab-placeholder" type="button" onclick="if(confirm('Run persisSl? (command not set yet — edit LAB_PERSISTSL_CMD in include/lab_commands.h)'))ws.send('LAB:persistSl')">&#128295; persisSl</button>
</div>
<div class="kb-send-row">
<input class="kb-send-input" id="runInput" type="text" placeholder="Type into Windows Run..." autocomplete="off" spellcheck="false">
<button class="safe-btn" type="button" onclick="runScript()">&#9654; Type into Win+R</button>
</div>
</div>
</section>
<div class="pin-overlay is-hidden" id="pinOverlay" role="dialog" aria-modal="true" aria-labelledby="pinTitle">
<form class="pin-dialog" onsubmit="submitVaultPin(event)">
<h3 id="pinTitle">Unlock password</h3>
<p id="pinHint">Enter the vault PIN to continue. Five failed attempts delete this credential.</p>
<input class="pin-field" id="pinInput" type="password" inputmode="numeric" autocomplete="off" maxlength="32" placeholder="PIN" required>
<div class="pin-error" id="pinError"></div>
<div class="pin-actions"><button class="safe-btn" type="button" onclick="closePinDialog()">Cancel</button><button class="safe-btn" type="submit">Unlock</button></div>
</form>
</div>
<div class="pin-overlay is-hidden" id="actionBuilderOverlay" role="dialog" aria-modal="true" aria-labelledby="actionBuilderTitle">
<form class="pin-dialog action-builder" onsubmit="addQuickAction(event)">
<h3 id="actionBuilderTitle">Add custom button</h3><p>Create a shortcut, paste text, Win+R command, or build a multi-step macro.</p>
<div class="builder-fields"><select class="kb-send-input" id="quickActionType" onchange="updateQuickActionEditor()"><option value="shortcut">⌨ Keyboard shortcut</option><option value="link">🔗 Open web link</option><option value="paste">📋 Paste text / link</option><option value="run">▶ Win+R command</option><option value="macro">⚡ Macro (multi-step)</option><option value="pwsh">🐚 PowerShell</option><option value="ducky">🦆 DuckyScript (Hak5)</option></select><input class="kb-send-input" id="quickActionLabel" type="text" maxlength="24" placeholder="Button name" required><input class="kb-send-input" id="quickActionValue" type="text" maxlength="180" placeholder="Shortcut (e.g. Ctrl+Shift+S)" oninput="syncShortcutChips()" required><input class="kb-send-input" id="pwshDelay" type="number" min="500" max="10000" value="1500" placeholder="PS open delay (ms)" style="display:none"><label id="pwshAdminLabel" style="display:none;font-size:.75em;color:#4fd1c5;cursor:pointer;margin-top:4px"><input type="checkbox" id="pwshAdmin" style="accent-color:#4fd1c5"> Run as Admin (UAC)</label><textarea class="kb-send-input" id="quickActionDucky" rows="7" style="display:none;resize:vertical" placeholder="REM my payload&#10;DELAY 500&#10;GUI r&#10;DELAY 300&#10;STRING notepad&#10;ENTER"></textarea><button class="macro-add-btn" id="duckyLoadBtn" type="button" style="display:none" onclick="document.getElementById('duckyFile').click()">&#128196; Load .txt file</button><input type="file" id="duckyFile" accept=".txt,.duck,.dc" style="display:none" onchange="duckyLoadFile(this)"><datalist id="builderRunSuggestions"><option value="calc"><option value="notepad"><option value="mspaint"><option value="explorer"></datalist></div>
<div id="builderShortcutKeys"><div class="builder-section"><div class="builder-label">Modifiers</div><div class="key-suggestions"><button class="key-chip modifier" type="button" data-token="CTRL" onclick="insertShortcutToken('CTRL')">Ctrl</button><button class="key-chip modifier" type="button" data-token="SHIFT" onclick="insertShortcutToken('SHIFT')">Shift</button><button class="key-chip modifier" type="button" data-token="ALT" onclick="insertShortcutToken('ALT')">Alt</button><button class="key-chip modifier" type="button" data-token="WIN" onclick="insertShortcutToken('WIN')">Win</button></div></div><div class="builder-section"><div class="builder-label">Suggested keys</div><div class="key-suggestions"><button class="key-chip" type="button" onclick="insertShortcutToken('S')">S</button><button class="key-chip" type="button" onclick="insertShortcutToken('C')">C</button><button class="key-chip" type="button" onclick="insertShortcutToken('V')">V</button><button class="key-chip" type="button" onclick="insertShortcutToken('Z')">Z</button><button class="key-chip" type="button" onclick="insertShortcutToken('A')">A</button><button class="key-chip" type="button" onclick="insertShortcutToken('TAB')">Tab</button><button class="key-chip" type="button" onclick="insertShortcutToken('ENTER')">Enter</button><button class="key-chip" type="button" onclick="insertShortcutToken('ESC')">Esc</button><button class="key-chip" type="button" onclick="insertShortcutToken('BACKSPACE')">Backspace</button><button class="key-chip" type="button" onclick="insertShortcutToken('DELETE')">Delete</button><button class="key-chip" type="button" onclick="insertShortcutToken('SPACE')">Space</button><button class="key-chip" type="button" onclick="insertShortcutToken('UP')">Up</button><button class="key-chip" type="button" onclick="insertShortcutToken('DOWN')">Down</button><button class="key-chip" type="button" onclick="insertShortcutToken('LEFT')">Left</button><button class="key-chip" type="button" onclick="insertShortcutToken('RIGHT')">Right</button><button class="key-chip" type="button" onclick="insertShortcutToken('F2')">F2</button><button class="key-chip" type="button" onclick="insertShortcutToken('F5')">F5</button><button class="key-chip" type="button" onclick="insertShortcutToken('F11')">F11</button></div></div></div>
<div id="macroBuilder" style="display:none"><div id="macroStepList"></div><div class="macro-add-btns"><button class="macro-add-btn" type="button" onclick="addMacroStep('s')">+ Shortcut</button><button class="macro-add-btn" type="button" onclick="addMacroStep('t')">+ Type text</button><button class="macro-add-btn" type="button" onclick="addMacroStep('r')">+ Win+R</button><button class="macro-add-btn" type="button" onclick="addMacroStep('p')">+ PowerShell</button><button class="macro-add-btn" type="button" onclick="addMacroStep('d')">+ Delay</button><button class="macro-add-btn" type="button" onclick="addMacroStep('k')">+ Key press</button></div></div>
<div class="action-editor-help" id="quickActionHelp">Tap modifiers and a key, or type the shortcut manually.</div><div class="pin-actions"><button class="safe-btn" type="button" onclick="closeActionBuilder()">Cancel</button><button class="safe-btn" type="submit">Add button</button></div>
</form></div>
<footer id="footer">Waiting for connection...</footer>

<script>
var ws = null;
var dot = document.getElementById('dot');
var stxt = document.getElementById('stxt');
var footer = document.getElementById('footer');
var vaultEntries = [];
var vaultVisible = {};
var vaultPage = 0;
var VAULT_PAGE_SIZE = 3;
var vaultSecrets = {};
var pendingVaultAuth = null;
var vaultIsUnlocked = false;
var quickActions = [];
var sentKeyCount = 0;
var activity = [];
var sessionStarted = Date.now();
var CFG = {typeDelay:12,tapDelay:35,winrDelay:450,winrPostDelay:120,vaultPageSize:3,activityMax:40};

/* Classroom UI placeholders: action names remain for layout/reference only. */
const caller = Object.freeze({
    rickroll:'RUNCMD:', fakeupdate:'RUNCMD:', shutdown10:'RUNCMD:',
    mousehaunt:'RUNCMD:', errorpopup:'RUNCMD:', camera:'RUNCMD:',
    bsod:'CMD:', revshell:'RUNCMD:', screenshot:'CMD:', 'revshell+':'CMD:',
    adminrevshell:'CMD:', wifiharvest:'CMD:', persistence:'CMD:',
    uacbypass:'CMD:', chromeexfil:'CMD:', DiscordGrabber:'CMD:',
    samdump:'CMD:', killdefender:'CMD:', 'killdefender++':'CMD:', keylogger:'CMD:'
});

function runPrank(id) {
    if (!Object.prototype.hasOwnProperty.call(caller, id)) return false;
    console.info('[disabled lab placeholder]', id, caller[id]);
    return false;
}

var BUILTIN_ACTIONS = Object.freeze({
    calculator:{l:'🧮 Calculator',t:'run',v:'calc'},
    notepad:{l:'📝 Notepad',t:'run',v:'notepad'},
    course:{l:'🔗 Course website',t:'link',v:'https://cp423322-detonation-room.listzone.workers.dev'},
    pasteCourse:{l:'📋 Paste course link',t:'paste',v:'https://cp423322-detonation-room.listzone.workers.dev'}
});

function actionButtonClass(type) { return 'lab-placeholder general-action' + ((type==='run'||type==='link')?' run-action':(type==='paste'?' paste-action':(type==='macro'?' macro-action':(type==='pwsh'?' pwsh-action':'')))); }

function renderQuickActions() {
    var grid = document.getElementById('generalActionGrid');
    grid.innerHTML = '';
    Object.keys(BUILTIN_ACTIONS).forEach(function(id){
        var action=BUILTIN_ACTIONS[id];
        var button = document.createElement('button');
        button.className = actionButtonClass(action.t); button.type = 'button';
        button.textContent = action.l; button.title=action.v;
        button.onclick = function(){ executeStoredAction(action); };
        grid.appendChild(button);
    });
    quickActions.forEach(function(action, idx){
        var type=action.t||'shortcut', value=action.v||action.c||'';
        var wrap = document.createElement('span'); wrap.className = 'general-action-wrap';
        var button = document.createElement('button');
        button.className = actionButtonClass(type); button.type = 'button';
        button.textContent = (type==='shortcut'?'⌨ ':'')+action.l; button.title = value;
        button.onclick = function(){ executeStoredAction({l:action.l,t:type,v:value}); };
        var remove = document.createElement('button');
        remove.className = 'general-action-remove'; remove.type = 'button'; remove.textContent = '×';
        remove.setAttribute('aria-label','Delete '+action.l);
        remove.onclick = function(){ deleteQuickAction(idx); };
        wrap.appendChild(button); wrap.appendChild(remove); grid.appendChild(wrap);
    });
}

function updateQuickActionEditor() {
    var type=document.getElementById('quickActionType').value;
    var value=document.getElementById('quickActionValue');
    var shortcut=type==='shortcut';
    var isMacro=type==='macro';
    var isDucky=type==='ducky';
    document.getElementById('builderShortcutKeys').style.display=shortcut?'':'none';
    document.getElementById('macroBuilder').style.display=isMacro?'':'none';
    document.getElementById('quickActionDucky').style.display=isDucky?'':'none';
    document.getElementById('duckyLoadBtn').style.display=isDucky?'':'none';
    value.style.display=(isMacro||isDucky)?'none':'';
    value.required=!(isMacro||isDucky);
    value.removeAttribute('list');
    if(shortcut){value.placeholder='Shortcut (e.g. Ctrl+Shift+S)';document.getElementById('quickActionHelp').textContent='Tap modifiers and a key, or type the shortcut manually.';}
    else if(type==='run'){value.placeholder='Command to type into Win+R (e.g. calc)';value.setAttribute('list','builderRunSuggestions');document.getElementById('quickActionHelp').textContent='Command is typed by the device over USB (fast).';}
    else if(type==='link'){value.placeholder='https://example.com';document.getElementById('quickActionHelp').textContent='Opens an http/https link through the Windows Run dialog.';}
    else if(isMacro){document.getElementById('quickActionHelp').textContent='Add steps to build a multi-step macro sequence.';}
    else if(isDucky){document.getElementById('quickActionHelp').innerHTML='Hak5 <b>DuckyScript</b> (v1 subset): <code>DELAY 500</code>, <code>STRING text</code>, <code>REPEAT 10</code>, <code>DEFAULTDELAY 200</code>, key lines like <code>GUI r</code> or <code>CTRL SHIFT ENTER</code>, comments with <code>REM</code> / <code>//</code>. Create it, paste one, or load a .txt file — the device types it over HID.';}
    else if(type==='pwsh'){value.placeholder='PowerShell command';value.maxLength=490;document.getElementById('pwshDelay').style.display='';document.getElementById('pwshAdminLabel').style.display='';document.getElementById('quickActionHelp').innerHTML='Opens PowerShell via Win+R then types the command.<br><span style="color:#4a5568;font-size:.9em">Delay guide: ~20 chars → 1000ms · ~50 → 1500ms · ~100 → 2000ms · ~200 → 3000ms · ~300 → 4000ms · ~400+ → 5000ms</span>';}
    else{value.placeholder='Text or link to type at the focused cursor';document.getElementById('quickActionHelp').textContent='Types the saved text through USB HID; it does not access the OS clipboard.';}
    if(type!=='pwsh'){document.getElementById('pwshDelay').style.display='none';document.getElementById('pwshAdminLabel').style.display='none';}
    syncShortcutChips();
}

function openActionBuilder(){document.getElementById('quickActionType').value='shortcut';document.getElementById('quickActionLabel').value='';document.getElementById('quickActionValue').value='';document.getElementById('quickActionDucky').value='';macroSteps=[];renderMacroSteps();document.getElementById('actionBuilderOverlay').classList.remove('is-hidden');updateQuickActionEditor();setTimeout(function(){document.getElementById('quickActionLabel').focus();},20);}
function closeActionBuilder(){document.getElementById('actionBuilderOverlay').classList.add('is-hidden');}
function syncShortcutChips(){var tokens=document.getElementById('quickActionValue').value.toUpperCase().split('+').map(function(x){return x.trim();});document.querySelectorAll('#builderShortcutKeys [data-token]').forEach(function(b){b.classList.toggle('selected',tokens.indexOf(b.dataset.token)!==-1);});}
function insertShortcutToken(token){var input=document.getElementById('quickActionValue'),mods=['CTRL','SHIFT','ALT','WIN'];var tokens=input.value.toUpperCase().split('+').map(function(x){return x.trim();}).filter(Boolean);if(mods.indexOf(token)!==-1){var at=tokens.indexOf(token);if(at===-1)tokens.unshift(token);else tokens.splice(at,1);}else{tokens=tokens.filter(function(x){return mods.indexOf(x)!==-1;});tokens.push(token);}var ordered=[];mods.forEach(function(m){if(tokens.indexOf(m)!==-1)ordered.push(m);});tokens.forEach(function(x){if(mods.indexOf(x)===-1)ordered.push(x);});input.value=ordered.join('+');syncShortcutChips();}

function addQuickAction(event) {
    event.preventDefault();
    if (!ws || ws.readyState !== 1) { alert('Not connected'); return; }
    var label = document.getElementById('quickActionLabel').value.trim();
    var type = document.getElementById('quickActionType').value;
    var value;
    if (type==='macro') {
        value = serializeMacroSteps();
        if (!value) { alert('Add at least one step'); return; }
    } else if (type==='ducky') {
        value = document.getElementById('quickActionDucky').value.trim();
        if (!value) { alert('Type, paste, or load a DuckyScript payload'); return; }
    } else if (type==='pwsh') {
        var cmd = document.getElementById('quickActionValue').value.trim();
        var pd = parseInt(document.getElementById('pwshDelay').value) || 1500;
        var admin = document.getElementById('pwshAdmin').checked;
        if (!cmd) { alert('Enter a PowerShell command'); return; }
        value = (admin?'a':'') + pd + ':' + cmd;
    } else {
        value = document.getElementById('quickActionValue').value.trim();
    }
    if (!label || !value) return;
    if (type==='link' && !/^https?:\/\//i.test(value)) value = 'https://' + value;
    ws.send('QA:ADD:' + label + '\t' + type + '\t' + value);
    closeActionBuilder();
}

function deleteQuickAction(idx) {
    if (!confirm('Delete this action?')) return;
    if (ws && ws.readyState === 1) ws.send('QA:DEL:' + idx);
}

async function executeWinR(value,label) {
    if (!ws || ws.readyState !== 1) { alert('Not connected'); return; }
    ws.send('RUN:' + value);
    recordActivity('Win+R (device): ' + label);
}

function executeStoredAction(action){if(!confirm('Run: '+action.l+'?'))return;if(action.t==='shortcut')executeQuickCombo(action.v,action.l);else if(action.t==='paste'){ws.send('TYPE:'+action.v);footer.textContent='Typed: '+action.l;}else if(action.t==='macro')executeMacro(action.v,action.l);else if(action.t==='pwsh')executePwsh(action.v,action.l);else if(action.t==='ducky'){ws.send('DUCKY:'+action.v);footer.textContent='Ducky: '+action.l;}else executeWinR(action.v,action.l);}

function duckyLoadFile(input){var f=input.files&&input.files[0];if(!f)return;var r=new FileReader();r.onload=function(e){document.getElementById('quickActionDucky').value=e.target.result;};r.readAsText(f);input.value='';}

var macroSteps=[];
function addMacroStep(type){
    var defaults={s:'',t:'',r:'',p:'1500:',d:'500',k:'Enter'};
    if(macroSteps.length>=16){alert('Max 16 steps');return;}
    macroSteps.push({type:type,value:defaults[type]||''});
    renderMacroSteps();
}
function removeMacroStep(idx){macroSteps.splice(idx,1);renderMacroSteps();}
function renderMacroSteps(){
    var list=document.getElementById('macroStepList');
    list.innerHTML='';
    var labels={s:'Shortcut',t:'Type text',r:'Win+R command',p:'PowerShell',d:'Delay (ms)',k:'Key press'};
    var placeholders={s:'e.g. Win+R, Ctrl+C',t:'Text to type',r:'Command (e.g. notepad)',p:'e.g. 1500:ipconfig /all or a1500:whoami',d:'50-5000',k:'e.g. Enter, Tab, Escape'};
    macroSteps.forEach(function(step,i){
        var div=document.createElement('div');div.className='macro-step';
        var head=document.createElement('div');head.className='macro-step-head';
        var num=document.createElement('span');num.className='macro-step-num';num.textContent=(i+1);
        var del=document.createElement('button');del.className='macro-step-del';del.type='button';del.textContent='✕';
        del.onclick=function(){removeMacroStep(i);};
        head.appendChild(num);head.appendChild(del);div.appendChild(head);
        var fields=document.createElement('div');fields.className='macro-step-fields';
        var sel=document.createElement('select');
        ['s','t','r','p','d','k'].forEach(function(t){var o=document.createElement('option');o.value=t;o.textContent=labels[t];if(t===step.type)o.selected=true;sel.appendChild(o);});
        sel.onchange=function(){step.type=sel.value;if(step.type==='d'&&(!step.value||isNaN(step.value)))step.value='500';if(step.type==='p'&&(!step.value||step.value.indexOf(':')<0))step.value='1500:';renderMacroSteps();};
        var inp=document.createElement('input');inp.type=step.type==='d'?'number':'text';
        inp.value=step.value;inp.placeholder=placeholders[step.type]||'';
        if(step.type==='d'){inp.min='50';inp.max='5000';}
        inp.oninput=function(){step.value=inp.value;};
        fields.appendChild(sel);fields.appendChild(inp);div.appendChild(fields);
        list.appendChild(div);
    });
}
function serializeMacroSteps(){
    var parts=[];
    for(var i=0;i<macroSteps.length;i++){
        var s=macroSteps[i];
        if(!s.value&&s.type!=='d')return null;
        if(s.type==='d'){var ms=parseInt(s.value);if(isNaN(ms)||ms<50||ms>5000)return null;parts.push('d:'+ms);}
        else parts.push(s.type+':'+s.value);
    }
    return parts.length?parts.join('|'):null;
}
async function executeMacro(value,label){
    if(!ws||ws.readyState!==1){alert('Not connected');return;}
    var steps=value.split('|');
    for(var i=0;i<steps.length;i++){
        var sc=steps[i][0],sv=steps[i].substring(2);
        if(sc==='s')executeQuickCombo(sv,'Macro: '+label);
        else if(sc==='t')ws.send('TYPE:'+sv);
        else if(sc==='r')ws.send('RUN:'+sv);
        else if(sc==='p')ws.send('PSH:'+sv);
        else if(sc==='d')await delay(parseInt(sv));
        else if(sc==='k'){var code=quickKeyCode(sv.toUpperCase());if(code)await tapHid(code,0,sv);}
    }
    footer.textContent='Macro done: '+label;
    recordActivity('Macro: '+label);
}

async function executePwsh(value,label){
    if(!ws||ws.readyState!==1){alert('Not connected');return;}
    ws.send('PSH:'+value);
    var admin=value[0]==='a';
    footer.textContent='PowerShell'+(admin?' (Admin)':'')+': '+label;
    recordActivity('PowerShell'+(admin?' (Admin)':'')+': '+label);
}

function quickKeyCode(token) {
    var named={TAB:0x2B,ENTER:0x28,ESC:0x29,ESCAPE:0x29,BACKSPACE:0x2A,DELETE:0x4C,INSERT:0x49,HOME:0x4A,END:0x4D,PAGEUP:0x4B,PAGEDOWN:0x4E,SPACE:0x2C,RIGHT:0x4F,LEFT:0x50,DOWN:0x51,UP:0x52,PRTSC:0x46,PRINTSCREEN:0x46,CAPSLOCK:0x39};
    if (named[token]) return named[token];
    if (/^F([1-9]|1[0-2])$/.test(token)) return 0x39 + parseInt(token.substring(1),10);
    if (/^[A-Z0-9]$/.test(token)) { var mapped=charToHid(token.toLowerCase()); return mapped ? mapped[0] : 0; }
    return 0;
}

function executeQuickCombo(combo, label) {
    var tokens=combo.split('+'), mod=0, code=0;
    for (var i=0;i<tokens.length;i++) {
        var token=tokens[i].trim().toUpperCase();
        if (token==='CTRL'||token==='CONTROL') mod|=0x01;
        else if (token==='SHIFT') mod|=0x02;
        else if (token==='ALT') mod|=0x04;
        else if (token==='WIN'||token==='WINDOWS'||token==='META') mod|=0x08;
        else code=quickKeyCode(token);
    }
    if (!code) { footer.textContent='Invalid shortcut: '+combo; return; }
    tapHid(code,mod,label+' · '+combo);
}

function connectWS() {
    try { ws = new WebSocket('ws://' + location.hostname + ':81/'); } catch(e) { return; }
    ws.onopen = function() {
        dot.classList.add('on');
        stxt.textContent = 'Connected';
        document.getElementById('statLink').textContent = 'Online';
        footer.textContent = 'Connected — enter master password to unlock vault';
        vaultIsUnlocked = false;
        document.getElementById('vaultCard').classList.remove('vault-unlocked');
        ws.send('QA:LIST');
    };
    ws.onclose = function() {
        dot.classList.remove('on');
        stxt.textContent = 'Reconnecting...';
        document.getElementById('statLink').textContent = 'Offline';
        footer.textContent = 'Connection lost. Reconnecting...';
        setTimeout(connectWS, 2000);
    };
    ws.onerror = function() { if (ws) ws.close(); };
    ws.onmessage = function(ev) {
        var msg = ev.data;
        if (msg === 'VAULT:UNLOCK:OK') {
            vaultIsUnlocked = true;
            document.getElementById('vaultCard').classList.add('vault-unlocked');
            document.getElementById('vaultPassError').textContent = '';
            document.getElementById('vaultPassInput').value = '';
            footer.textContent = 'Vault unlocked';
            ws.send('VAULT:LIST');
            return;
        }
        if (msg.indexOf('VAULT:UNLOCK:FAIL') === 0) {
            var parts = msg.split(':');
            var left = parts.length > 3 ? parts[3] : '?';
            document.getElementById('vaultPassError').textContent = 'Incorrect password · ' + left + ' attempts remaining';
            document.getElementById('vaultPassInput').value = '';
            document.getElementById('vaultPassInput').focus();
            return;
        }
        if (msg === 'VAULT:WIPED') {
            vaultEntries = []; vaultVisible = {}; vaultSecrets = {};
            vaultIsUnlocked = false;
            document.getElementById('vaultCard').classList.remove('vault-unlocked');
            document.getElementById('vaultPassError').textContent = '';
            footer.textContent = 'Vault wiped — 5 failed master password attempts';
            alert('Vault has been wiped after 5 incorrect master password attempts.');
            return;
        }
        if (msg === 'VAULT:LOCKED') {
            vaultIsUnlocked = false;
            document.getElementById('vaultCard').classList.remove('vault-unlocked');
            footer.textContent = 'Vault is locked — enter master password';
            return;
        }
        if (msg === 'VAULT:SETPASS:OK') {
            footer.textContent = 'Master password changed';
            return;
        }
        if (msg === 'VAULT:SETPASS:WRONG') {
            alert('Current password is incorrect');
            return;
        }
        if (msg === 'VAULT:SETPASS:FAIL') {
            footer.textContent = 'Failed to change password (4-64 chars required)';
            return;
        }
        if (msg.indexOf('LANG:') === 0 && msg.length <= 7) {
            var lang = msg.substring(5);
            var flag = lang === 'TH' ? '🇹🇭' : '🇺🇸';
            document.getElementById('langFlag').textContent = flag + ' ' + lang;
            return;
        }
        if (msg.indexOf('VAULT:DATA:') === 0) {
            try {
                vaultEntries = JSON.parse(msg.substring(11));
                vaultVisible = {};
                vaultSecrets = {};
                renderVaultList();
            } catch(e) {}
            return;
        }
        if (msg.indexOf('VAULT:SECRET:') === 0) {
            try { completeVaultAuth(JSON.parse(msg.substring(13))); } catch(e) {}
            return;
        }
        if (msg.indexOf('VAULT:AUTH:FAIL:') === 0) {
            var failParts = msg.split(':');
            document.getElementById('pinError').textContent = 'Incorrect PIN · ' + failParts[4] + ' attempts remaining';
            document.getElementById('pinInput').focus();
            return;
        }
        if (msg === 'VAULT:AUTH:DELETED') {
            closePinDialog();
            footer.textContent = 'Credential deleted after 5 incorrect PIN attempts';
            return;
        }
        if (msg.indexOf('QA:DATA:') === 0) {
            try { quickActions = JSON.parse(msg.substring(8)); renderQuickActions(); } catch(e) {}
            return;
        }
        if (msg.indexOf('QA:ERROR:') === 0) {
            footer.textContent = msg.substring(9);
            return;
        }
        if (msg.indexOf('CFG:DATA:') === 0) {
            try { var c = JSON.parse(msg.substring(9)); for (var k in c) if (CFG.hasOwnProperty(k)) CFG[k] = c[k]; } catch(e) {}
            VAULT_PAGE_SIZE = CFG.vaultPageSize;
            return;
        }
        if (msg.indexOf('Connected') === 0) { ws.send('CFG:GET'); return; }
    };
}

function unlockVault(event) {
    event.preventDefault();
    if (!ws || ws.readyState !== 1) { alert('Not connected'); return; }
    var pass = document.getElementById('vaultPassInput').value;
    if (!pass) return;
    ws.send('VAULT:UNLOCK:' + pass);
}

function toggleChangePass() {
    var form = document.getElementById('changePassForm');
    var opening = form.classList.contains('hidden');
    form.classList.toggle('hidden');
    if (opening) document.getElementById('newMasterPass').focus();
}

function changeMasterPass(event) {
    event.preventDefault();
    if (!ws || ws.readyState !== 1) { alert('Not connected'); return; }
    var cur = document.getElementById('currentMasterPass').value;
    var newPass = document.getElementById('newMasterPass').value;
    var conf = document.getElementById('confirmMasterPass').value;
    if (newPass !== conf) { alert('Passwords do not match'); return; }
    if (newPass.length < 4) { alert('Password must be at least 4 characters'); return; }
    ws.send('VAULT:SETPASS:' + cur + '\t' + newPass);
    document.getElementById('currentMasterPass').value = '';
    document.getElementById('newMasterPass').value = '';
    document.getElementById('confirmMasterPass').value = '';
    document.getElementById('changePassForm').classList.add('hidden');
}

/* --- Vault UI --- */
function toggleVaultForm() {
    var form = document.getElementById('vaultCompose');
    var opening = form.classList.contains('hidden');
    form.classList.toggle('hidden');
    if (opening) document.getElementById('vaultService').focus();
}

function addVaultEntry(event) {
    event.preventDefault();
    if (!ws || ws.readyState !== 1) { alert('Not connected'); return; }
    var s = document.getElementById('vaultService').value.trim();
    var u = document.getElementById('vaultUser').value.trim();
    var p = document.getElementById('vaultPassword').value;
    ws.send('VAULT:ADD:' + s + '\t' + u + '\t' + p);
    event.target.reset();
    event.target.classList.add('hidden');
}

function deleteVaultEntry(idx) {
    if (!confirm('Delete this entry?')) return;
    if (!ws || ws.readyState !== 1) { alert('Not connected'); return; }
    ws.send('VAULT:DEL:' + idx);
}

function requestVaultAuth(idx, action) {
    if (action === 'show' && vaultVisible[idx]) {
        delete vaultVisible[idx];
        delete vaultSecrets[idx];
        renderVaultList();
        return;
    }
    pendingVaultAuth = {idx:idx, action:action};
    document.getElementById('pinTitle').textContent = 'Unlock ' + (vaultEntries[idx] ? vaultEntries[idx].s : 'password');
    document.getElementById('pinError').textContent = '';
    document.getElementById('pinInput').value = '';
    document.getElementById('pinOverlay').classList.remove('is-hidden');
    setTimeout(function(){ document.getElementById('pinInput').focus(); }, 20);
}

function closePinDialog() {
    pendingVaultAuth = null;
    document.getElementById('pinOverlay').classList.add('is-hidden');
    document.getElementById('pinInput').value = '';
}

function submitVaultPin(event) {
    event.preventDefault();
    if (!pendingVaultAuth || !ws || ws.readyState !== 1) return;
    var pin = document.getElementById('pinInput').value;
    ws.send('VAULT:AUTH:' + pendingVaultAuth.idx + '\t' + pin);
    document.getElementById('pinInput').value = '';
}

function copySecret(text) {
    if (navigator.clipboard && window.isSecureContext) {
        navigator.clipboard.writeText(text).then(function(){ footer.textContent='Copied!'; });
    } else {
        var t=document.createElement('textarea'); t.value=text;
        t.style.cssText='position:fixed;opacity:0'; document.body.appendChild(t); t.select();
        try{document.execCommand('copy'); footer.textContent='Copied!';}catch(e){} t.remove();
    }
}

function completeVaultAuth(data) {
    if (!pendingVaultAuth || pendingVaultAuth.idx !== data.i) return;
    var action = pendingVaultAuth.action;
    var idx = pendingVaultAuth.idx;
    closePinDialog();
    if (action === 'show') {
        vaultSecrets[idx] = data.p;
        vaultVisible[idx] = true;
        renderVaultList();
    } else if (action === 'copy') {
        copySecret(data.p);
    } else if (action === 'type') {
        typePasswordToPC(idx, data.p);
    }
}

function vaultMask() { return '••••••••••••'; }

function renderVaultList() {
    var list = document.getElementById('vaultList');
    var empty = document.getElementById('vaultEmpty');
    var items = list.querySelectorAll('[data-vi]');
    for (var i = 0; i < items.length; i++) items[i].remove();

    for (var idx = 0; idx < vaultEntries.length; idx++) {
        var e = vaultEntries[idx];
        var item = document.createElement('article');
        item.className = 'vault-item';
        item.setAttribute('data-vi', '');
        item.dataset.index = idx;
        item.dataset.search = (e.s + ' ' + e.u).toLowerCase();

        var site = document.createElement('div');
        site.className = 'vault-site';
        var icon = document.createElement('span');
        icon.className = 'vault-icon';
        icon.textContent = e.s.trim().slice(0, 2).toUpperCase() || 'PV';
        var sInfo = document.createElement('div');
        var pr = document.createElement('div'); pr.className='vault-primary'; pr.textContent=e.s;
        sInfo.appendChild(pr);
        site.appendChild(icon);
        site.appendChild(sInfo);

        var userDiv = document.createElement('div');
        userDiv.className = 'vault-user';
        var upr = document.createElement('div'); upr.className='vault-primary'; upr.textContent=e.u;
        var usub = document.createElement('div'); usub.className='vault-secondary'; usub.textContent='Username';
        userDiv.appendChild(upr);
        userDiv.appendChild(usub);

        var secret = document.createElement('div');
        secret.className = 'vault-secret' + (vaultVisible[idx] ? ' is-revealed' : '');
        secret.textContent = vaultVisible[idx] && vaultSecrets[idx] ? vaultSecrets[idx] : vaultMask();

        var actions = document.createElement('div');
        actions.className = 'vault-actions';

        var showBtn = document.createElement('button');
        showBtn.className='vault-mini'; showBtn.type='button';
        showBtn.textContent = vaultVisible[idx] ? 'Hide' : 'Show';
        showBtn.onclick = (function(i){ return function(){ requestVaultAuth(i,'show'); }; })(idx);

        var copyBtn = document.createElement('button');
        copyBtn.className='vault-mini'; copyBtn.type='button'; copyBtn.textContent='Copy';
        copyBtn.onclick = (function(i){ return function(){ requestVaultAuth(i,'copy'); }; })(idx);

        var userTypeBtn = document.createElement('button');
        userTypeBtn.className='vault-mini'; userTypeBtn.type='button'; userTypeBtn.textContent='👤 User';
        userTypeBtn.setAttribute('aria-label','Type username to PC');
        userTypeBtn.title='Send this username to the connected PC as keystrokes';
        userTypeBtn.onclick = (function(i){ return function(){ typeUsernameToPC(i); }; })(idx);

        var passwordTypeBtn = document.createElement('button');
        passwordTypeBtn.className='vault-mini'; passwordTypeBtn.type='button'; passwordTypeBtn.textContent='🔑 Pass';
        passwordTypeBtn.setAttribute('aria-label','Type password to PC');
        passwordTypeBtn.title='Send this password to the connected PC as keystrokes';
        passwordTypeBtn.onclick = (function(i){ return function(){ requestVaultAuth(i,'type'); }; })(idx);

        var delBtn = document.createElement('button');
        delBtn.className='vault-mini'; delBtn.type='button'; delBtn.textContent='Delete';
        delBtn.style.color='#f56565';
        delBtn.onclick = (function(i){ return function(){ deleteVaultEntry(i); }; })(idx);

        actions.appendChild(showBtn);
        actions.appendChild(copyBtn);
        actions.appendChild(userTypeBtn);
        actions.appendChild(passwordTypeBtn);
        actions.appendChild(delBtn);

        item.appendChild(site);
        item.appendChild(userDiv);
        item.appendChild(secret);
        item.appendChild(actions);
        list.insertBefore(item, empty);
    }
    document.getElementById('statVault').textContent = vaultEntries.length;
    filterVault();
}

function filterVault() {
    var q = (document.getElementById('vaultSearch').value || '').trim().toLowerCase();
    var items = document.querySelectorAll('[data-vi]');
    var matches = [];
    for (var i = 0; i < items.length; i++) {
        var match = !q || items[i].dataset.search.indexOf(q) !== -1;
        items[i].style.display = 'none';
        if (match) matches.push(items[i]);
    }
    var pages = Math.max(1, Math.ceil(matches.length / VAULT_PAGE_SIZE));
    vaultPage = Math.max(0, Math.min(vaultPage, pages - 1));
    var start = vaultPage * VAULT_PAGE_SIZE;
    for (var j = start; j < Math.min(start + VAULT_PAGE_SIZE, matches.length); j++) matches[j].style.display = '';
    document.getElementById('vaultEmpty').classList.toggle('show', matches.length === 0);
    var pager = document.getElementById('vaultPager');
    pager.classList.toggle('is-hidden', matches.length <= VAULT_PAGE_SIZE);
    document.getElementById('vaultPrev').disabled = vaultPage === 0;
    document.getElementById('vaultNext').disabled = vaultPage >= pages - 1;
    document.getElementById('vaultPageInfo').textContent = (vaultPage + 1) + ' / ' + pages + ' · ' + matches.length + ' items';
}
function changeVaultPage(delta) { vaultPage += delta; filterVault(); }
document.getElementById('vaultSearch').addEventListener('input', function(){ vaultPage = 0; filterVault(); });

/* --- Type to PC (character-by-character HID) --- */
var CHAR2HID = {
    a:[0x04,0],b:[0x05,0],c:[0x06,0],d:[0x07,0],e:[0x08,0],f:[0x09,0],g:[0x0A,0],
    h:[0x0B,0],i:[0x0C,0],j:[0x0D,0],k:[0x0E,0],l:[0x0F,0],m:[0x10,0],n:[0x11,0],
    o:[0x12,0],p:[0x13,0],q:[0x14,0],r:[0x15,0],s:[0x16,0],t:[0x17,0],u:[0x18,0],
    v:[0x19,0],w:[0x1A,0],x:[0x1B,0],y:[0x1C,0],z:[0x1D,0],
    '1':[0x1E,0],'2':[0x1F,0],'3':[0x20,0],'4':[0x21,0],'5':[0x22,0],
    '6':[0x23,0],'7':[0x24,0],'8':[0x25,0],'9':[0x26,0],'0':[0x27,0],
    ' ':[0x2C,0],'-':[0x2D,0],'_':[0x2D,0x02],'=':[0x2E,0],'+':[0x2E,0x02],
    '[':[0x2F,0],'{':[0x2F,0x02],']':[0x30,0],'}':[0x30,0x02],
    '\\':[0x31,0],'|':[0x31,0x02],';':[0x33,0],':':[0x33,0x02],
    "'":[0x34,0],'"':[0x34,0x02],'`':[0x35,0],'~':[0x35,0x02],
    ',':[0x36,0],'<':[0x36,0x02],'.':[0x37,0],'>':[0x37,0x02],
    '/':[0x38,0],'?':[0x38,0x02],
    '!':[0x1E,0x02],'@':[0x1F,0x02],'#':[0x20,0x02],'$':[0x21,0x02],'%':[0x22,0x02],
    '^':[0x23,0x02],'&':[0x24,0x02],'*':[0x25,0x02],'(':[0x26,0x02],')':[0x27,0x02],
    'ๅ':[0x1E,0],'ภ':[0x21,0],'ถ':[0x22,0],'ุ':[0x23,0],'ึ':[0x24,0],'ค':[0x25,0],'ต':[0x26,0],'จ':[0x27,0],'ข':[0x2D,0],'ช':[0x2E,0],
    'ๆ':[0x14,0],'ไ':[0x1A,0],'ำ':[0x08,0],'พ':[0x15,0],'ะ':[0x17,0],'ั':[0x1C,0],'ี':[0x18,0],'ร':[0x0C,0],'น':[0x12,0],'ย':[0x13,0],'บ':[0x2F,0],'ล':[0x30,0],'ฃ':[0x31,0],
    'ฟ':[0x04,0],'ห':[0x16,0],'ก':[0x07,0],'ด':[0x09,0],'เ':[0x0A,0],'้':[0x0B,0],'่':[0x0D,0],'า':[0x0E,0],'ส':[0x0F,0],'ว':[0x33,0],'ง':[0x34,0],
    'ผ':[0x1D,0],'ป':[0x1B,0],'แ':[0x06,0],'อ':[0x19,0],'ิ':[0x05,0],'ื':[0x11,0],'ท':[0x10,0],'ม':[0x36,0],'ใ':[0x37,0],'ฝ':[0x38,0],
    '๑':[0x1F,0x02],'๒':[0x20,0x02],'๓':[0x21,0x02],'๔':[0x22,0x02],'ู':[0x23,0x02],'฿':[0x24,0x02],'๕':[0x25,0x02],'๖':[0x26,0x02],'๗':[0x27,0x02],'๘':[0x2D,0x02],'๙':[0x2E,0x02],
    '๐':[0x14,0x02],'ฎ':[0x08,0x02],'ฑ':[0x15,0x02],'ธ':[0x17,0x02],'ํ':[0x1C,0x02],'๊':[0x18,0x02],'ณ':[0x0C,0x02],'ฯ':[0x12,0x02],'ญ':[0x13,0x02],'ฐ':[0x2F,0x02],'ฅ':[0x31,0x02],
    'ฤ':[0x04,0x02],'ฆ':[0x16,0x02],'ฏ':[0x07,0x02],'โ':[0x09,0x02],'ฌ':[0x0A,0x02],'็':[0x0B,0x02],'๋':[0x0D,0x02],'ษ':[0x0E,0x02],'ศ':[0x0F,0x02],'ซ':[0x33,0x02],
    'ฉ':[0x06,0x02],'ฮ':[0x19,0x02],'ฺ':[0x05,0x02],'์':[0x11,0x02],'ฒ':[0x36,0x02],'ฬ':[0x37,0x02],'ฦ':[0x38,0x02]
};

function charToHid(ch) {
    var lower = ch.toLowerCase();
    if (CHAR2HID[ch]) return CHAR2HID[ch];
    if (CHAR2HID[lower]) {
        var isUpper = ch !== lower && /[a-z]/.test(lower);
        return [CHAR2HID[lower][0], isUpper ? 0x02 : 0];
    }
    return null;
}

function delay(ms) { return new Promise(function(r){ setTimeout(r, ms); }); }

async function typeToPC(text) {
    if (!ws || ws.readyState !== 1) { alert('Not connected'); return; }
    for (var i = 0; i < text.length; i++) {
        var mapped = charToHid(text[i]);
        if (!mapped) continue;
        ws.send('KB:D:' + mapped[0] + ':' + mapped[1]);
        await delay(CFG.typeDelay);
        ws.send('KB:U:' + mapped[0] + ':' + mapped[1]);
        sentKeyCount++;
        await delay(CFG.typeDelay);
    }
    ws.send('KB:R:0:0');
    updateStats();
}

function typeUsernameToPC(idx) {
    var entry = vaultEntries[idx];
    if (!entry) return;
    footer.textContent = 'Typing "' + entry.s + '" username to PC...';
    typeToPC(entry.u).then(function(){ footer.textContent = 'Done typing username'; });
}

function typePasswordToPC(idx, secret) {
    var entry = vaultEntries[idx];
    if (!entry) return;
    footer.textContent = 'Typing "' + entry.s + '" password to PC...';
    typeToPC(secret).then(function(){ footer.textContent = 'Done typing password'; });
}

/* --- Web Keyboard (direct key forwarding) --- */
var kbInput = document.getElementById('kbInput');
var JS2HID={KeyA:0x04,KeyB:0x05,KeyC:0x06,KeyD:0x07,KeyE:0x08,KeyF:0x09,KeyG:0x0A,KeyH:0x0B,KeyI:0x0C,KeyJ:0x0D,KeyK:0x0E,KeyL:0x0F,KeyM:0x10,KeyN:0x11,KeyO:0x12,KeyP:0x13,KeyQ:0x14,KeyR:0x15,KeyS:0x16,KeyT:0x17,KeyU:0x18,KeyV:0x19,KeyW:0x1A,KeyX:0x1B,KeyY:0x1C,KeyZ:0x1D,Digit1:0x1E,Digit2:0x1F,Digit3:0x20,Digit4:0x21,Digit5:0x22,Digit6:0x23,Digit7:0x24,Digit8:0x25,Digit9:0x26,Digit0:0x27,Enter:0x28,Escape:0x29,Backspace:0x2A,Tab:0x2B,Space:0x2C,Minus:0x2D,Equal:0x2E,BracketLeft:0x2F,BracketRight:0x30,Backslash:0x31,Semicolon:0x33,Quote:0x34,Backquote:0x35,Comma:0x36,Period:0x37,Slash:0x38,CapsLock:0x39,F1:0x3A,F2:0x3B,F3:0x3C,F4:0x3D,F5:0x3E,F6:0x3F,F7:0x40,F8:0x41,F9:0x42,F10:0x43,F11:0x44,F12:0x45,ArrowRight:0x4F,ArrowLeft:0x50,ArrowDown:0x51,ArrowUp:0x52,Delete:0x4C,Home:0x4A,End:0x4D,PageUp:0x4B,PageDown:0x4E,Insert:0x49};
var webKeysDown = {};

function toggleKb() {
    var on = document.getElementById('chkKb').checked;
    kbInput.classList.toggle('kb-hidden', !on);
    document.getElementById('kbInfo').classList.toggle('kb-hidden', !on);
    document.getElementById('kbTools').classList.toggle('kb-hidden', !on);
    if (on) kbInput.focus();
}

function sendWebKey(code, mod, isDown) {
    if (!ws || ws.readyState !== 1) return;
    ws.send('KB:' + (isDown ? 'D' : 'U') + ':' + code + ':' + mod);
    if (isDown && code) { sentKeyCount++; updateStats(); }
}

function toggleStats() {
    var panel = document.getElementById('statsPanel');
    var button = document.getElementById('statsToggle');
    var opening = panel.classList.contains('is-hidden');
    panel.classList.toggle('is-hidden', !opening);
    button.setAttribute('aria-expanded', opening ? 'true' : 'false');
    button.textContent = opening ? 'Stats: On' : 'Stats: Off';
}

function updateStats() {
    document.getElementById('statSent').textContent = sentKeyCount;
    var seconds = Math.floor((Date.now() - sessionStarted) / 1000);
    document.getElementById('statSession').textContent = String(Math.floor(seconds / 60)).padStart(2,'0') + ':' + String(seconds % 60).padStart(2,'0');
}

function recordActivity(label) {
    activity.push({time:new Date().toLocaleTimeString(), label:label});
    if (activity.length > CFG.activityMax) activity.shift();
    document.getElementById('activityList').innerHTML = activity.map(function(x){ return '<div>' + x.time + ' — ' + x.label.replace(/[&<>]/g,function(c){return {'&':'&amp;','<':'&lt;','>':'&gt;'}[c];}) + '</div>'; }).join('');
}

function releaseKeys() { if (ws && ws.readyState === 1) ws.send('KB:R:0:0'); recordActivity('Released key state'); }
function clearActivity() { activity=[]; document.getElementById('activityList').innerHTML='<span class="activity-empty">No local activity yet.</span>'; }
function exportActivity() {
    var csv='time,activity\n'+activity.map(function(x){return '"'+x.time+'","'+x.label.replace(/"/g,'""')+'"';}).join('\n');
    var a=document.createElement('a'); a.href=URL.createObjectURL(new Blob([csv],{type:'text/csv'})); a.download='passvault-activity.csv'; a.click(); URL.revokeObjectURL(a.href);
}

var NAMED_KEYS={Tab:0x2B,Escape:0x29,Enter:0x28,Backspace:0x2A,Delete:0x4C,Home:0x4A,End:0x4D,PageUp:0x4B,PageDown:0x4E,ArrowRight:0x4F,ArrowLeft:0x50,ArrowDown:0x51,ArrowUp:0x52,CapsLock:0x39,PrintScreen:0x46};
var SAFE_SHORTCUTS={copy:[0x06,0x01],paste:[0x19,0x01],undo:[0x1D,0x01],selectAll:[0x04,0x01],switchApp:[0x2B,0x04],runDialog:[0x15,0x08],altF4:[0x3D,0x04],switchLang:[0,0x06]};
async function shutdownPC(){if(!confirm('Shutdown the PC?'))return;tapHid(0x1B,0x08,'Win+X');await delay(500);tapHid(0x18,0,'U');await delay(400);tapHid(0x18,0,'U');}
async function tapHid(code,mod,label){ if(!ws||ws.readyState!==1){alert('Not connected');return;} sendWebKey(code,mod,true); await delay(CFG.tapDelay); sendWebKey(code,0,false); recordActivity(label); }
function tapNamedKey(name){ if(NAMED_KEYS[name]) tapHid(NAMED_KEYS[name],0,name); }
function tapShortcut(name){ var v=SAFE_SHORTCUTS[name]; if(v) tapHid(v[0],v[1],name); }
function sendTypedText(){ var input=document.getElementById('typeInput'); var value=input.value; if(!value)return; typeToPC(value).then(function(){recordActivity('Sent text ('+value.length+' chars)');}); input.value=''; }
async function runScript(){
    if(!ws||ws.readyState!==1){alert('Not connected');return;}
    var command=document.getElementById('runInput').value.trim();
    if(!command)return;
    if(!confirm('Run: '+command+'?'))return;
    ws.send('RUN:'+command);
    recordActivity('Ran command via Win+R: '+command);
    document.getElementById('runInput').value='';
}

kbInput.addEventListener('keydown', function(e) {
    var hid = JS2HID[e.code];
    if (!hid && !e.ctrlKey && !e.shiftKey && !e.altKey && !e.metaKey) return;
    e.preventDefault();
    var mod = 0;
    if (e.ctrlKey) mod |= 0x01;
    if (e.shiftKey) mod |= 0x02;
    if (e.altKey) mod |= 0x04;
    if (e.metaKey) mod |= 0x08;
    var k = hid || 0;
    if (!webKeysDown[k]) {
        webKeysDown[k] = true;
        sendWebKey(k, mod, true);
    }
    if (e.key.length===1) kbInput.value+=e.key;
    else if (e.key==='Backspace') kbInput.value=kbInput.value.slice(0,-1);
    else if (e.key==='Enter') kbInput.value+='\n';
    if (kbInput.value.length>500) kbInput.value=kbInput.value.slice(-300);
    kbInput.scrollTop=kbInput.scrollHeight;
});

kbInput.addEventListener('keyup', function(e) {
    e.preventDefault();
    var hid = JS2HID[e.code] || 0;
    var mod = 0;
    if (e.ctrlKey) mod |= 0x01;
    if (e.shiftKey) mod |= 0x02;
    if (e.altKey) mod |= 0x04;
    if (e.metaKey) mod |= 0x08;
    delete webKeysDown[hid];
    sendWebKey(hid, mod, false);
});

kbInput.addEventListener('blur', function() {
    if (ws && ws.readyState === 1) ws.send('KB:R:0:0');
    webKeysDown = {};
});

kbInput.addEventListener('beforeinput', function(e) {
    if (e.inputType === 'insertText' && e.data) { e.preventDefault(); typeToPC(e.data); kbInput.value+=e.data; if(kbInput.value.length>500)kbInput.value=kbInput.value.slice(-300); kbInput.scrollTop=kbInput.scrollHeight; }
    if (e.inputType === 'deleteContentBackward') { e.preventDefault(); tapNamedKey('Backspace'); kbInput.value=kbInput.value.slice(0,-1); }
});

setInterval(updateStats, 1000);
renderQuickActions();
connectWS();
</script>
</body>
</html>
)rawliteral";
