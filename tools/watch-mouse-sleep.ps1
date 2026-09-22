# watch-mouse-sleep.ps1
#
# Watches Razer DeathAdder V2 Pro (VID_1532 PID_007D) presence in the Windows
# device tree and logs transitions (present <-> ABSENT) with timestamps.
#
# Purpose: determine what happens at the USB/HID level when the wireless mouse
# falls asleep, so the OpenRGB plugin can pick the right wake-detection trigger:
#   A) device disappears on sleep  -> plugin reacts to DEVICE_LIST_UPDATED (hotplug)
#   B) device stays enumerated     -> plugin must poll / use another signal
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File .\watch-mouse-sleep.ps1
# Let the mouse idle for 10+ minutes so it sleeps, move a little to wake it,
# wait again, then press Ctrl+C. Read mouse-presence.log afterwards.
#
# Optional params:
#   -LogPath           log file location (default: .\mouse-presence.log)
#   -IntervalSeconds   poll cadence (default: 1)
#   -HeartbeatSeconds  how often to log when nothing changed (default: 60)

param(
    [string]$LogPath          = "$PSScriptRoot\mouse-presence.log",
    [int]   $IntervalSeconds  = 1,
    [int]   $HeartbeatSeconds = 60
)

$VidPidPattern = 'VID_1532&PID_007D'   # Razer DeathAdder V2 Pro

function Get-MouseDeviceInfo {
    $found = @(Get-PnpDevice -PresentOnly | Where-Object {
        $_.Status -eq 'OK' -and $_.InstanceId -match $VidPidPattern
    })
    return $found
}

function Get-PresentIds {
    $ids = @(Get-MouseDeviceInfo | ForEach-Object { $_.InstanceId })
    return ($ids -join ' | ')
}

$start        = Get-Date
$lastState    = $null
$lastHeartbeat= $start

$header = @(
    "# Razer DeathAdder V2 Pro (VID_1532 PID_007D) presence log"
    "# started: $($start.ToString('yyyy-MM-dd HH:mm:ss'))"
    "# 'present' = device enumerated in Windows, 'ABSENT' = not found"
    "# Let the mouse idle so it sleeps; wake it; repeat. Then Ctrl+C."
    ""
) -join "`r`n"

$header | Set-Content -Path $LogPath -Encoding UTF8
Write-Host "Logging to $LogPath  (Ctrl+C to stop)"

try {
    while ($true) {
        $devices = @(Get-MouseDeviceInfo)
        $present = ($devices.Count -gt 0)
        $now     = Get-Date
        $stamp   = $now.ToString('yyyy-MM-dd HH:mm:ss')

        if ($present -ne $lastState -and $null -ne $lastState) {
            $ids = Get-PresentIds
            $line = "$stamp  ${present}  <-- TRANSITION  [ids: $ids]"
            $line | Add-Content -Path $LogPath -Encoding UTF8
            Write-Host $line
            $lastHeartbeat = $now
        }
        elseif ($null -eq $lastState) {
            # first poll: baseline
            $ids = Get-PresentIds
            $line = "$stamp  ${present}  (baseline) [ids: $ids]"
            $line | Add-Content -Path $LogPath -Encoding UTF8
            Write-Host $line
            $lastHeartbeat = $now
        }
        elseif (($now - $lastHeartbeat).TotalSeconds -ge $HeartbeatSeconds) {
            $line = "$stamp  ${present}  (heartbeat)"
            $line | Add-Content -Path $LogPath -Encoding UTF8
            Write-Host $line
            $lastHeartbeat = $now
        }

        $lastState = $present
        Start-Sleep -Seconds $IntervalSeconds
    }
}
finally {
    $end = Get-Date
    "`n# stopped: $($end.ToString('yyyy-MM-dd HH:mm:ss'))" | Add-Content -Path $LogPath -Encoding UTF8
}