param(
    [string]$GeneratedCoreRoot = 'C:\dev\helworks\builds\helengine-webgpu\cache\v2\d87d222cd166d9049967e97c50c8e301\b\webgpu\debug\release\generated-core',
    [string]$OutputRoot = 'C:\dev\helworks\builds\helengine-webgpu\renderer-parity\shader-tests',
    [string]$SdkRoot = 'C:\dev\helworks\tools\emsdk'
)
$ErrorActionPreference='Stop'
$repository=Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'build-shader-gpu-tests.ps1') -OutputRoot $OutputRoot -SdkRoot $SdkRoot
if($LASTEXITCODE -ne 0) {throw 'Preparing shadow shader fixtures failed.'}
$env:EM_CONFIG=Join-Path $SdkRoot '.emscripten'
$arguments=@((Join-Path $repository 'tests/native/atlas_shadows_gpu_harness.cpp'))
foreach($unit in @('webgpu_device_context','webgpu_frame','webgpu_shader_library','webgpu_shader_artifact_reader','webgpu_material_bindings','webgpu_light_state','webgpu_shadow_plan','webgpu_shadow_renderer')) {$arguments+=Join-Path $repository ('src/platform/webgpu/'+$unit+'.cpp')}
foreach($unit in @('float2','float3','float4','float4x4')) {$arguments+=Join-Path $GeneratedCoreRoot ($unit+'.cpp')}
$arguments+=@('-std=c++20','--use-port=emdawnwebgpu','-fexceptions','-frtti','-sALLOW_MEMORY_GROWTH=1',('-I'+(Join-Path $repository 'src')),('-I'+$GeneratedCoreRoot),'-IC:/dev/helworks/tools/webgpu-shaders/include','--preload-file',((Join-Path $OutputRoot 'fixtures/shaders-content')+'@/content/cooked'),'-o',(Join-Path $OutputRoot 'atlas.js'))
& (Join-Path $SdkRoot 'upstream/emscripten/em++.exe') @arguments
if($LASTEXITCODE -ne 0) {throw 'Compiling actual atlas renderer checks failed.'}
[IO.File]::WriteAllText((Join-Path $OutputRoot 'atlas.html'),'<link rel="icon" href="data:,"><canvas id="helengine-canvas" width="640" height="360"></canvas><script src="atlas.js"></script>')
