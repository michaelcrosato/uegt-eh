param([string]$Name = 'AFTERLIGHT-Win64-0.1.0')
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Stage = Join-Path $ProjectRoot 'Saved\StagedBuilds\Windows'
$Archive = Join-Path $ProjectRoot "Builds\$Name.zip"
if ($Name -notmatch '^[A-Za-z0-9._-]+$') { throw 'Use a simple archive name without directory separators.' }
if (!(Test-Path -LiteralPath "$Stage\Afterlight.exe")) { throw 'Run Build.ps1 -Target Package first.' }
if (Test-Path -LiteralPath $Archive) { throw "Archive already exists: $Archive. Choose a new -Name." }
# The clean staging tree has no save data, local logs, audit screenshots or debug symbols.
Compress-Archive -Path "$Stage\*" -DestinationPath $Archive -CompressionLevel Optimal
Get-Item -LiteralPath $Archive | Select-Object FullName,Length
Get-FileHash -LiteralPath $Archive -Algorithm SHA256 | Select-Object Hash
