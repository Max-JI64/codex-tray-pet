$ErrorActionPreference='Stop'
$taskName='Codex Status Pet Lite'
$exe=Join-Path $PSScriptRoot 'CodexPetLite.exe'
$existing=Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
if($existing){
 if(@($existing.Actions | Where-Object {-not [String]::Equals($_.Execute,$exe,[StringComparison]::OrdinalIgnoreCase)}).Count){throw 'The task belongs to a different installation.'}
 Unregister-ScheduledTask -TaskName $taskName -Confirm:$false
}
$running=@(Get-Process -Name CodexPetLite -ErrorAction SilentlyContinue | Where-Object {[String]::Equals($_.Path,$exe,[StringComparison]::OrdinalIgnoreCase)})
if($running.Count){
 $p=Start-Process -FilePath $exe -ArgumentList '--stop' -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -Wait -PassThru
 if($p.ExitCode -ne 0){throw 'Pet stop request failed.'}
 foreach($process in $running){if(-not $process.WaitForExit(6000)){throw 'Pet did not stop.'}}
}
Write-Output 'Automatic startup removed. Files and settings were kept.'
