param(
    [string]$GeneratedCoreRoot = 'C:\dev\helworks\builds\helengine-webgpu\cache\v2\d87d222cd166d9049967e97c50c8e301\b\webgpu\debug\release\generated-core',
    [string]$OutputRoot = 'C:\dev\helworks\builds\helengine-webgpu\renderer-parity\shader-tests',
    [string]$SdkRoot = 'C:\dev\helworks\tools\emsdk'
)
$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$env:EM_CONFIG = Join-Path $SdkRoot '.emscripten'
$arguments = @((Join-Path $repository 'tests/native/light_state_harness.cpp'))
foreach ($unit in @('webgpu_light_state','webgpu_shadow_plan')) { $arguments += Join-Path $repository ('src/platform/webgpu/' + $unit + '.cpp') }
foreach ($unit in @('float2','float3','float4','float4x4')) { $arguments += Join-Path $GeneratedCoreRoot ($unit + '.cpp') }
$arguments += @('-std=c++20','-fexceptions','-frtti',('-I'+(Join-Path $repository 'src')),('-I'+$GeneratedCoreRoot),'-IC:/dev/helworks/tools/webgpu-shaders/include','-sENVIRONMENT=web,node','-sEXIT_RUNTIME=1','-o',(Join-Path $OutputRoot 'lights.js'))
& (Join-Path $SdkRoot 'upstream/emscripten/em++.exe') @arguments
if ($LASTEXITCODE -ne 0) { throw 'Compiling native light/shadow planning checks failed.' }
[IO.File]::WriteAllText((Join-Path $OutputRoot 'lights.html'), '<script src="lights.js"></script>')
& node (Join-Path $OutputRoot 'lights.js')
if ($LASTEXITCODE -ne 0) { throw 'Native light/shadow planning assertions failed.' }
