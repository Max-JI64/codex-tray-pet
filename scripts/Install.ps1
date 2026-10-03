param([switch]$ValidateOnly)
$ErrorActionPreference='Stop'
$exe=Join-Path $PSScriptRoot 'CodexPetLite.exe'
if(-not (Test-Path -LiteralPath $exe)){throw 'CodexPetLite.exe not found. Extract the complete release ZIP.'}
$taskName='Codex Status Pet Lite'
$existing=Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
if($existing -and @($existing.Actions | Where-Object {-not [String]::Equals($_.Execute,$exe,[StringComparison]::OrdinalIgnoreCase)}).Count){throw 'A pet is registered in a different folder. Uninstall that copy first.'}
$otherCopy=@(Get-Process -Name CodexPetLite -ErrorAction SilentlyContinue | Where-Object {-not [String]::Equals($_.Path,$exe,[StringComparison]::OrdinalIgnoreCase)})
if($otherCopy.Count){throw 'A pet is running from a different folder. Stop that copy before installing.'}
if($ValidateOnly){Write-Output 'Installation paths validated; no task was changed.';return}
$user=[Security.Principal.WindowsIdentity]::GetCurrent().Name
$action=New-ScheduledTaskAction -Execute $exe -WorkingDirectory $PSScriptRoot
$trigger=New-ScheduledTaskTrigger -AtLogOn -User $user
$principal=New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Limited
$settings=New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew
Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description 'Shows a tray pet while Codex desktop is running.' -Force | Out-Null
Start-ScheduledTask -TaskName $taskName
$deadline=(Get-Date).AddSeconds(20)
$running=$null
do {
 Start-Sleep -Milliseconds 250
 $running=Get-Process -Name CodexPetLite -ErrorAction SilentlyContinue | Where-Object {[String]::Equals($_.Path,$exe,[StringComparison]::OrdinalIgnoreCase)} | Select-Object -First 1
} while(-not $running -and (Get-Date) -lt $deadline)
if(-not $running){throw 'The startup task was registered, but the pet did not start. Check Task Scheduler.'}
Start-Sleep -Seconds 2
$running.Refresh()
if($running.HasExited){throw 'The pet exited immediately after startup.'}
Write-Output 'Codex Tray Pet installed for the current user.'
