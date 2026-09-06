param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [ValidateSet('Editor','Game','Package')][string]$Target = 'Editor'
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$ProjectFile = Join-Path $ProjectRoot 'Afterlight.uproject'
if (!(Test-Path -LiteralPath $ProjectFile)) { throw "Project not found: $ProjectFile" }
foreach ($Plugin in @('DLSS','StreamlineDLSSG','StreamlineReflex')) {
    if (!(Test-Path -LiteralPath "$EngineRoot\Engine\Plugins\Marketplace\$Plugin\$Plugin.uplugin")) {
        throw "Install NVIDIA DLSS 4.5 UE 5.8 plugin package first. Missing: $Plugin"
    }
}
if ($Target -eq 'Package') {
    & "$EngineRoot\Engine\Build\BatchFiles\RunUAT.bat" -WaitForUATMutex BuildCookRun "-project=$ProjectFile" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -iostore -archive -nodebuginfo -prereqs "-archivedirectory=$ProjectRoot\Builds" -utf8output -unattended
} else {
    $BuildTarget = if ($Target -eq 'Editor') { 'AfterlightEditor' } else { 'Afterlight' }
    & "$EngineRoot\Engine\Build\BatchFiles\Build.bat" $BuildTarget Win64 Development "-Project=$ProjectFile" -WaitMutex -NoHotReloadFromIDE -MaxParallelActions=6
}
if ($LASTEXITCODE -ne 0) { throw "Unreal build failed: $LASTEXITCODE" }
