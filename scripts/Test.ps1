$ErrorActionPreference='Stop'
$projectFolder=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$exe=Join-Path $projectFolder 'build\CodexPetLite.exe'
if(-not (Test-Path -LiteralPath $exe)){throw 'Build first.'}
foreach($check in @('--self-test','--stream-test','--detection-test','--panel-test','--custom-test','--custom-state-test','--motion-test')){
 $p=Start-Process -FilePath $exe -ArgumentList $check -WorkingDirectory (Split-Path -Parent $exe) -WindowStyle Hidden -Wait -PassThru
 if($p.ExitCode -ne 0){throw "$check failed: $($p.ExitCode)"}
 Write-Output "PASS $check"
}
