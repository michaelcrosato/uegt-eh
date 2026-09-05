param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8')
$ProjectRoot = Split-Path -Parent $PSScriptRoot
& "$EngineRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$ProjectRoot\Afterlight.uproject" -game -dx12 -windowed -ResX=2560 -ResY=1440 -AfterlightProfile -unattended -nosplash -stdout '-LogCmds=global warning,LogTemp display,LogRHI log' '-log=Profile-Quality.log' | Out-Host
if ($LASTEXITCODE -ne 0) { throw "GPU profile failed: $LASTEXITCODE" }
