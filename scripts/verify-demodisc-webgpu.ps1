[CmdletBinding()]
param(
    [string]$Project='C:\dev\helprojs\demodisc\project.heproj',
    [string]$OutputRoot='C:\dev\helworks\builds\helengine-webgpu\renderer-parity',
    [string]$CacheRoot='C:\dev\helworks\builds\helengine-webgpu\cache',
    [switch]$SkipBuild
)
$ErrorActionPreference='Stop'
$repository=Split-Path -Parent $PSScriptRoot
$engine=Join-Path (Split-Path -Parent $repository) 'helengine'
$player=Join-Path $OutputRoot 'player'
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
function Invoke-Checked([string]$Log,[scriptblock]$Command) {
    $previousErrorAction=$ErrorActionPreference
    try {
        # Windows PowerShell wraps native stderr as ErrorRecord even for exit zero.
        # Retain that stream, then decide success from the actual process exit code.
        $ErrorActionPreference='Continue'
        $global:LASTEXITCODE=$null
        & $Command *> $Log
        $result=$LASTEXITCODE
    } finally {$ErrorActionPreference=$previousErrorAction}
    Get-Content -LiteralPath $Log -Tail 10
    if($null -eq $result -or $result -ne 0){throw "Validation command failed ($result). Full log: $Log"}
}
Push-Location $repository
try {
    $env:MSBUILDDISABLENODEREUSE='1';$env:DOTNET_CLI_USE_MSBUILD_SERVER='0';$env:UseSharedCompilation='false'
    if(-not $SkipBuild) {
        Invoke-Checked (Join-Path $OutputRoot 'verification-plugin.log') {dotnet build builder/helengine.webgpu.builder.csproj --no-dependencies --disable-build-servers}
        Invoke-Checked (Join-Path $OutputRoot 'verification-export.log') {
            dotnet run --no-build --project (Join-Path $engine 'tools/build-waiter/helengine.buildwaiter.csproj') -- --output $player --require index.html --require helengine_webgpu.js --require helengine_webgpu.wasm --require helengine_webgpu.data -- powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $engine 'scripts/build-platform.ps1') -Project $Project -Platform webgpu -Output $player -BuildProfile release -CacheRoot $CacheRoot
        }
    }
    $state=Get-Content -LiteralPath (Join-Path $player '.helengine-build-state.json') -Raw | ConvertFrom-Json
    if($state.status -ne 'succeeded' -or $state.exitCode -ne 0 -or -not $state.completedUtc){throw 'The official full export did not succeed.'}
    $start=[DateTimeOffset]::Parse($state.startedUtc)
    $artifacts=@(foreach($name in @('index.html','helengine_webgpu.js','helengine_webgpu.wasm','helengine_webgpu.data')) {
        $file=Get-Item -LiteralPath (Join-Path $player $name)
        if($file.Length -le 0 -or $file.LastWriteTimeUtc -lt $start.UtcDateTime){throw "Missing or stale official artifact: $($file.FullName)"}
        [pscustomobject]@{path=$file.FullName;bytes=$file.Length;modifiedUtc=$file.LastWriteTimeUtc;sha256=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash}
    })
    Invoke-Checked (Join-Path $OutputRoot 'verification-content.log') {dotnet run --project tests/content/helengine.webgpu.content.csproj --disable-build-servers -- --project-root (Split-Path -Parent $Project) --report (Join-Path $OutputRoot 'content-audit.json')}
    Invoke-Checked (Join-Path $OutputRoot 'verification-managed.log') {dotnet test builder.tests/helengine.webgpu.builder.tests.csproj --disable-build-servers --artifacts-path (Join-Path $OutputRoot 'dotnet')}
    Invoke-Checked (Join-Path $OutputRoot 'verification-server.log') {node --test tests/browser/server_configuration.test.cjs tests/browser/cooked-shader-corruption.test.cjs}
    Invoke-Checked (Join-Path $OutputRoot 'verification-command-runner.log') {powershell -NoProfile -ExecutionPolicy Bypass -File tests/scripts/verify-command.tests.ps1 -OutputRoot (Join-Path $OutputRoot 'script-tests')}
    Invoke-Checked (Join-Path $OutputRoot 'verification-preload-rebuild.log') {powershell -NoProfile -ExecutionPolicy Bypass -File tests/scripts/preload-rebuild.tests.ps1 -OutputRoot (Join-Path $OutputRoot 'preload-rebuild')}
    $env:HELENGINE_WEBGPU_TEST_PLAYER_ROOT=$player
    $env:HELENGINE_WEBGPU_TEST_SHADER_ROOT=Join-Path $OutputRoot 'shader-tests'
    $env:PLAYWRIGHT_JSON_OUTPUT_NAME=Join-Path $OutputRoot 'browser-report.json'
    Push-Location tests/browser
    try {Invoke-Checked (Join-Path $OutputRoot 'verification-browser.log') {npx playwright test --reporter=line,json --output (Join-Path $OutputRoot 'browser-results')}} finally {Pop-Location}
    $summary=[pscustomobject]@{buildId=$state.buildId;project=$Project;sceneCount=37;modules=@('DemoDisc','Raytracing','TiltPlay','Zombislayer');artifacts=$artifacts;browserReport=(Join-Path $OutputRoot 'browser-report.json');completedUtc=[DateTime]::UtcNow}
    $summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputRoot 'verification-summary.json') -Encoding UTF8
    Write-Output "Complete DemoDisc verification passed: $player"
} finally {Pop-Location}
