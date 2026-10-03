param([string]$Version='0.1.1')
$ErrorActionPreference='Stop'
if($Version -notmatch '^\d+\.\d+\.\d+$'){throw 'Invalid version.'}
$projectFolder=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildFolder=Join-Path $projectFolder 'build'
$distFolder=Join-Path $projectFolder 'dist'
$packageFolder=Join-Path $distFolder ('codex-tray-pet-v'+$Version+'-windows-x64')
New-Item -ItemType Directory -Path $packageFolder -Force | Out-Null
foreach($file in @('CodexPetLite.exe','settings.html')){Copy-Item -LiteralPath (Join-Path $buildFolder $file) -Destination $packageFolder}
Copy-Item -LiteralPath (Join-Path $projectFolder 'README.md') -Destination $packageFolder
Copy-Item -LiteralPath (Join-Path $projectFolder 'docs') -Destination $packageFolder -Recurse -Force
Copy-Item -LiteralPath (Join-Path $projectFolder 'assets\pet-creation-prompt.txt') -Destination $packageFolder
foreach($file in @('Install.ps1','Uninstall.ps1','Install.cmd','Uninstall.cmd','Start.cmd')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $file) -Destination $packageFolder}
[IO.File]::WriteAllText((Join-Path $packageFolder 'pet-settings.txt'),"badgeColor=1`nanimation=1`ncustomIcon=0`n",[Text.UTF8Encoding]::new($false))
$zip=Join-Path $distFolder ('codex-tray-pet-v'+$Version+'-windows-x64.zip')
Compress-Archive -LiteralPath $packageFolder -DestinationPath $zip -Force
Get-FileHash -LiteralPath $zip -Algorithm SHA256 | ForEach-Object { $_.Hash.ToLower()+'  '+(Split-Path -Leaf $_.Path) } | Set-Content -Encoding ASCII -LiteralPath (Join-Path $distFolder 'SHA256SUMS.txt')
Write-Output $zip
