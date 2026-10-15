param(
    [string]$OutputRoot = 'C:\dev\helworks\builds\helengine-webgpu\shared-shaders\native-checks',
    [string]$FixtureRoot = 'C:\dev\helworks\builds\helengine-webgpu\shared-shaders\shader-tests\fixtures\shaders-content'
)
$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
& cmake -S (Join-Path $repository 'tests/native/shaders') -B $OutputRoot -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'Configuring artifact reader checks failed.' }
& cmake --build $OutputRoot --config Release --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Compiling artifact reader checks failed.' }
& (Join-Path $OutputRoot 'Release/shader_artifact_harness.exe') $FixtureRoot
if ($LASTEXITCODE -ne 0) { throw 'Native artifact reader checks failed.' }
