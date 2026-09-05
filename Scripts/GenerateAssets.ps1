param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
& "$EngineRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$ProjectRoot\Afterlight.uproject" -run=pythonscript "-script=$PSScriptRoot\GenerateAssets.py" -unattended -nosplash -nullrhi -NoSound -UTF8Output
if ($LASTEXITCODE -ne 0) { throw "Asset generation failed: $LASTEXITCODE" }
