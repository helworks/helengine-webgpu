param(
    [Parameter(Mandatory = $true)][string]$Root,
    [ValidateRange(1,65535)][int]$Port = 8198
)
$ErrorActionPreference = 'Stop'
$playerRoot = (Resolve-Path -LiteralPath $Root).Path
if (-not (Test-Path -LiteralPath $playerRoot -PathType Container)) { throw 'Root must be a player output directory.' }
Write-Host "HelEngine player: http://127.0.0.1:$Port/"
& python -m http.server $Port --bind 127.0.0.1 --directory $playerRoot
exit $LASTEXITCODE
