param(
    [string]$SdkRoot = 'C:\dev\helworks\tools\emsdk',
    [string]$OutputRoot = 'C:\dev\helworks\builds\helengine-webgpu\device-tests',
    [string]$GeneratedCoreRoot = ''
)
$ErrorActionPreference = 'Stop'
$platformRoot = Split-Path -Parent $PSScriptRoot
$compiler = Join-Path $SdkRoot 'upstream\emscripten\em++.exe'
if (-not (Test-Path -LiteralPath $compiler -PathType Leaf)) { throw 'Run setup-toolchain.ps1 before compiling the device tests.' }
$env:EM_CONFIG = Join-Path $SdkRoot '.emscripten'
if (-not $GeneratedCoreRoot) {
    $cacheRoot = Join-Path $platformRoot '..\builds\helengine-webgpu\cache'
    $generatedConfiguration = Get-ChildItem -LiteralPath $cacheRoot -Filter helcpp_config.hpp -Recurse -File | Where-Object { Test-Path -LiteralPath (Join-Path $_.DirectoryName 'AudioAsset.cpp') -PathType Leaf } | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if (-not $generatedConfiguration) { throw 'Export DemoDisc first or pass -GeneratedCoreRoot with official generated C++.' }
    $GeneratedCoreRoot = $generatedConfiguration.DirectoryName
}
Write-Host "Using official generated contracts: $GeneratedCoreRoot"
New-Item -ItemType Directory -Path $OutputRoot -Force | Out-Null
$compilerArguments = @(
    (Join-Path $platformRoot 'tests\native\device_harness.cpp'),
    (Join-Path $platformRoot 'src\platform\webgpu\webgpu_device_context.cpp'),
    (Join-Path $platformRoot 'tests\native\audio_input_harness.cpp'),
    (Join-Path $platformRoot 'src\platform\web\web_audio_backend.cpp'),
    (Join-Path $platformRoot 'src\platform\web\web_input_backend.cpp'),
    ('-I' + $GeneratedCoreRoot), '-fexceptions', '-frtti',
    ('-I' + (Join-Path $platformRoot 'src')), '-std=c++20', '--use-port=emdawnwebgpu', '--no-entry',
    '-sALLOW_MEMORY_GROWTH=1', '-sEXPORTED_RUNTIME_METHODS=["UTF8ToString"]', '-o', (Join-Path $OutputRoot 'device.js')
)
foreach ($generatedType in @('Asset','AudioAsset','AudioPlaybackRequest','KeyboardState','MouseState','InputFrameState','InputPointerState','InputTextState','InputGamepadState','int2')) {
    $compilerArguments += Join-Path $GeneratedCoreRoot ($generatedType + '.cpp')
}
& $compiler @compilerArguments
if ($LASTEXITCODE -ne 0) { throw 'Compiling the real WebGPU device harness failed.' }
$page = '<canvas id="helengine-canvas" width="640" height="360"></canvas><script>var Module={onRuntimeInitialized(){window.moduleReady=true}};</script><script src="device.js"></script>'
[System.IO.File]::WriteAllText((Join-Path $OutputRoot 'index.html'), $page)
