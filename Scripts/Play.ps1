param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8', [switch]$Editor)
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Packaged = Join-Path $ProjectRoot 'Builds\Windows\Afterlight.exe'
if ((Test-Path -LiteralPath $Packaged) -and !$Editor) { & $Packaged -dx12; exit }
& "$EngineRoot\Engine\Binaries\Win64\UnrealEditor.exe" "$ProjectRoot\Afterlight.uproject" -game -dx12 -log
