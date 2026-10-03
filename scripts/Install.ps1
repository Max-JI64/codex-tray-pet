param([switch]$ValidateOnly)
$ErrorActionPreference='Stop'
$exe=Join-Path $PSScriptRoot 'CodexPetLite.exe'
if(-not (Test-Path -LiteralPath $exe)){throw 'CodexPetLite.exe not found. Extract the complete release ZIP.'}
$taskName='Codex Status Pet Lite'
$existing=Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
if($existing -and @($existing.Actions | Where-Object {-not [String]::Equals($_.Execute,$exe,[StringComparison]::OrdinalIgnoreCase)}).Count){throw 'A pet is registered in a different folder. Uninstall that copy first.'}
if($ValidateOnly){Write-Output 'Installation paths validated; no task was changed.';return}
$user=[Security.Principal.WindowsIdentity]::GetCurrent().Name
$action=New-ScheduledTaskAction -Execute $exe -WorkingDirectory $PSScriptRoot
$trigger=New-ScheduledTaskTrigger -AtLogOn -User $user
$principal=New-ScheduledTaskPrincipal -UserId $user -LogonType Interactive -RunLevel Limited
$settings=New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -ExecutionTimeLimit ([TimeSpan]::Zero) -MultipleInstances IgnoreNew
Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description 'Shows a tray pet while Codex desktop is running.' -Force | Out-Null
Start-ScheduledTask -TaskName $taskName
Write-Output 'Codex Tray Pet installed for the current user.'
