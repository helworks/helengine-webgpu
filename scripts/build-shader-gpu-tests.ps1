param(
    [string]$SdkRoot = 'C:\dev\helworks\tools\emsdk',
    [string]$OutputRoot = 'C:\dev\helworks\builds\helengine-webgpu\shared-shaders\shader-tests'
)
$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$fixtures = Join-Path $OutputRoot 'fixtures'
& dotnet build (Join-Path $repository 'tests/fixtures/helengine.webgpu.fixtures.csproj') --disable-build-servers --artifacts-path (Join-Path $OutputRoot 'dotnet')
if ($LASTEXITCODE -ne 0) { throw 'Building shader fixtures failed.' }
& dotnet (Join-Path $OutputRoot 'dotnet/bin/helengine.webgpu.fixtures/debug/helengine.webgpu.fixtures.dll') $fixtures
if ($LASTEXITCODE -ne 0) { throw 'Cooking shader fixtures failed.' }
$env:EM_CONFIG = Join-Path $SdkRoot '.emscripten'
$platform = Join-Path $repository 'src/platform/webgpu'
$arguments = @((Join-Path $repository 'tests/native/shared_shader_gpu_harness.cpp'))
foreach ($unit in @('webgpu_device_context','webgpu_frame','webgpu_shader_library','webgpu_shader_artifact_reader','webgpu_material_artifact_reader','webgpu_material_bindings')) { $arguments += Join-Path $platform ($unit + '.cpp') }
$arguments += @('-std=c++20','--use-port=emdawnwebgpu','-fexceptions','-frtti','-sALLOW_MEMORY_GROWTH=1',('-I'+(Join-Path $repository 'src')),'-IC:/dev/helworks/tools/webgpu-shaders/include','--preload-file',((Join-Path $fixtures 'shaders-content')+'@/content/cooked'),'-o',(Join-Path $OutputRoot 'shared.js'))
& (Join-Path $SdkRoot 'upstream/emscripten/em++.exe') @arguments
if ($LASTEXITCODE -ne 0) { throw 'Compiling the real shared shader library GPU harness failed.' }
[IO.File]::WriteAllText((Join-Path $OutputRoot 'index.html'), '<canvas id="helengine-canvas" width="640" height="360"></canvas><script src="shared.js"></script>')
$shadowArguments = @((Join-Path $repository 'tests/native/shadow_resources_gpu_harness.cpp')) + $arguments[1..($arguments.Count-1)]
$shadowArguments[$shadowArguments.Count-1] = Join-Path $OutputRoot 'shadows.js'
& (Join-Path $SdkRoot 'upstream/emscripten/em++.exe') @shadowArguments
if ($LASTEXITCODE -ne 0) { throw 'Compiling shadow resource GPU checks failed.' }
[IO.File]::WriteAllText((Join-Path $OutputRoot 'shadows.html'), '<canvas id="helengine-canvas" width="640" height="360"></canvas><script src="shadows.js"></script>')
