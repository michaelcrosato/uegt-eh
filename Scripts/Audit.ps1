param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8', [switch]$Packaged, [switch]$NoRayTracing, [switch]$FrameGenerationOnly, [switch]$OrientationOnly)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Started = Get-Date
if (@(@($NoRayTracing, $FrameGenerationOnly, $OrientationOnly) | Where-Object { $_ }).Count -gt 1) { throw 'Choose only one focused audit mode.' }
$AuditFlag = if ($NoRayTracing) { '-AfterlightNoRTAudit' } else { '-AfterlightAudit' }
$RayFlags = @('-dx12')
if ($NoRayTracing) { $RayFlags += '-noraytracing' }
if ($FrameGenerationOnly) { $RayFlags += '-AfterlightFGOnly' }
if ($OrientationOnly) { $RayFlags += '-AfterlightOrientationAudit' }
$ReportName = if ($NoRayTracing) { 'no-rt-audit.json' } elseif ($FrameGenerationOnly) { 'frame-generation-audit.json' } elseif ($OrientationOnly) { 'orientation-audit.json' } else { 'runtime-audit.json' }
if ($OrientationOnly) { Write-Host 'The orientation audit drives real movement and inputs. Please do not press keys or move the mouse during the test.' }
elseif (!$NoRayTracing) { Write-Host 'The audit requests game-window focus for Frame Generation. Do not switch away during that sample.' }
$Arguments = @('-windowed', '-ResX=2560', '-ResY=1440', $AuditFlag, '-unattended', '-nosplash', '-LogCmds="global warning,LogTemp display,LogCore display"') + $RayFlags
if ($Packaged) {
    $Report = "$ProjectRoot\Builds\Windows\Afterlight\Saved\Evidence\$ReportName"
    $Executable = "$ProjectRoot\Builds\Windows\Afterlight\Binaries\Win64\Afterlight.exe"
} else {
    $Report = "$ProjectRoot\Saved\Evidence\$ReportName"
    $Executable = "$EngineRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
    $Arguments = @(('"' + "$ProjectRoot\Afterlight.uproject" + '"'), '-game') + $Arguments
}
if (!(Test-Path -LiteralPath $Executable)) { throw "Audit executable not found: $Executable" }
# This is an interactive rendering test. Explicitly show its window even when
# PowerShell was started by a background agent; the user still controls focus.
$Process = Start-Process -FilePath $Executable -ArgumentList $Arguments -WorkingDirectory $ProjectRoot -WindowStyle Normal -Wait -PassThru
if ($Process.ExitCode -ne 0) { throw "Runtime audit process failed: $($Process.ExitCode)" }
if (!(Test-Path -LiteralPath $Report) -or (Get-Item -LiteralPath $Report).LastWriteTime -lt $Started) { throw 'No fresh audit report was produced.' }
$Result = Get-Content -LiteralPath $Report -Raw | ConvertFrom-Json
if (!$Result.passed) { throw "Runtime checks failed: $($Result.checks_failed -join ', ')" }
$Result | Select-Object passed,mean_render_fps,p95_render_frame_ms,p99_render_frame_ms,sample_count,fg_observed_render_fps,fg_observed_present_fps,fg_samples_with_generated_frames,fg_samples_with_foreground_focus
