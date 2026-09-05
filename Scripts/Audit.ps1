param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8', [switch]$Packaged)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
if ($Packaged) {
    & "$ProjectRoot\Builds\Windows\Afterlight.exe" -dx12 -windowed -ResX=2560 -ResY=1440 -AfterlightAudit -unattended -nosplash -log
} else {
    & "$EngineRoot\Engine\Binaries\Win64\UnrealEditor.exe" "$ProjectRoot\Afterlight.uproject" -game -dx12 -windowed -ResX=2560 -ResY=1440 -AfterlightAudit -unattended -nosplash -log
}
if ($LASTEXITCODE -ne 0) { throw "Runtime audit process failed: $LASTEXITCODE" }
