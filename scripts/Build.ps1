param([Parameter(Mandatory=$true)][string]$TccPath)
$ErrorActionPreference='Stop'
$projectFolder=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$compiler=(Resolve-Path -LiteralPath $TccPath).Path
$buildFolder=Join-Path $projectFolder 'build'
New-Item -ItemType Directory -Path $buildFolder -Force | Out-Null
$exe=Join-Path $buildFolder 'CodexPetLite.exe'
& $compiler '-o' $exe (Join-Path $projectFolder 'src\CodexPetLite.c') (Join-Path $projectFolder 'compiler\kernel32-extra.def') (Join-Path $projectFolder 'compiler\shell32.def') '-Wl,-subsystem=windows' '-luser32' '-lgdi32' '-lkernel32'
if($LASTEXITCODE -ne 0){throw "Compiler failed: $LASTEXITCODE"}
Copy-Item -LiteralPath (Join-Path $projectFolder 'assets\settings.html') -Destination $buildFolder
Write-Output $exe
