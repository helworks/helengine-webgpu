# Shared WebGPU shaders: execution evidence

Status: Tasks 1–5 complete; Tasks 6–7 final browser acceptance in progress. No architectural decision is pending. Full DemoDisc acceptance remains open.

## Implementation

- Stable WebGpu target 10, webgpu descriptor and HEL_API_WEBGPU; existing values 0–9 preserved (engine 19ba3506).
- Shared HLSL requests, defines, include callbacks/dependency hashes, shaderc/SPIR-V and pinned Tint structured IR/WGSL translation. Original and final reflection are retained separately; no generated-source rewrites or uniformity suppression. Shared cache remains disabled until its key covers all tools.
- Dawn/Tint 31e25af254ab572c77054edec4946d2244e184dd matches Emscripten 6.0.12/emdawnwebgpu; Silk.NET 2.21.0 and nlohmann/json 3.12.0 are pinned. Tools/lock: C:\dev\helworks\tools\webgpu-shaders.
- All five original variants compile: ForwardStandard, ForwardStandardShadowed, ShadowDepth, Textured, RoundedShape. The standard editor default/Mesh aliases preserve their identities.
- WGSP v1 declared artifacts, deterministic selection paths, preparation before publication, renderer UI dependencies and lazy builder registry integration.
- WGPM v1 preserves shader/PBR/render/shadow intent, named constants and imported texture references. The editor does not yet automatically project arbitrary custom buffers into the optional raw input field.
- Native strict readers, original-to-final matrix/array packing, retained shader/layout/pipeline ownership and migration of both renderers. Production no longer copies/loads the two fixed WGSL files.
- Explicit browser package selection rejects missing/incomplete configured output instead of falling back to an old build.

Schemas: C:\dev\helworks\helengine-webgpu\docs\WebGpuArtifactSchema.md.

## Approved shared source correction

Helena approved SampleLevel(..., 0) for six shadow-depth sampling calls in C:\dev\helworks\helengine\engine\helengine.editor\shaders\builtin\ForwardStandardShader.hlsl. DirectX11 shadow resources have one mip level. Engine commit c02333c9. The four targeted DirectX11/Vulkan shader tests passed: C:\dev\helworks\builds\helengine-webgpu\shared-shaders\shared-shadow-windows-vulkan.log.

## Evidence

All logs below are under C:\dev\helworks\builds\helengine-webgpu\shared-shaders.

- Builder suite: 70/70 passed (task6-suite.log).
- Engine target descriptors: 7/7 passed (descriptor-final.log).
- Native harness: seven real shader pairs, corrupted sizes/versions/UTF-8, duplicate JSON keys, bad member ranges/alignment, stage mismatch, packing and known hash vectors passed (task6-native-final.log).
- Custom HLSL through production cookers, readers and C++ library: b0/t0/s0, nonidentity matrix and tint array yielded exact GPU center RGBA [32,64,96,255] and black background (task6-gpu-browser.log).
- Server configuration: 2/2 passed (task7-server-green.log).
- All 25 real Windows-authored gallery materials preserved metallic/roughness sweeps through WGPM (gallery-probe.log and C:\dev\helworks\builds\helengine-webgpu\shared-shaders\gallery-materials).
- Official export of C:\dev\helprojs\demodisc\project.heproj through C:\dev\helworks\helengine\scripts\build-platform.ps1 and Build Waiter published all four fresh, nonempty files under C:\dev\helworks\builds\helengine-webgpu\shared-shaders\player (export-retry.log). Its manifest declares original Textured/RoundedShape plus ForwardStandard/default WGSP selections.
- First full run on the new package: 29/30 browser tests passed (browser-full.log), NVIDIA Lovelace, fallback false. Geometry/UI pixels, allocation reuse, rotation lighting, input, resize, visibility/audio and invalid/missing shader failures passed. Closure renamed an unquoted public diagnostic property; literal-key correction and official reexport succeeded. Final full run: 30/30 passed (browser-final.log), with the same hardware adapter and all three expected shader identities. The final export is recorded in export-final.log and the output build-state file reports succeeded/exit 0.
- Include changes and intentional compile errors verify changed cooked content, preserved last valid output and no partial prepared publication in cooker tests.

## Failures investigated

Reused restricted MSBuild servers caused access-denied cache writes; dedicated invocations with disabled server/node reuse resolved this. The dynamically loaded plugin lacked its private SPIRV-Cross dependency; a RED regression preceded CopyLocalLockFileAssemblies and the next export succeeded. Embedded-JavaScript nullish assignment caused an Emscripten trigraph warning; ordinary assignment resolved it. The release diagnostic rename above was caught by the new identity test. No failures were ignored.

## Remaining full DemoDisc work

WebGPU scene selection is still Cube-only. The objective remains 37 desktop scenes from 39 total: HD selection, menus/transitions, game modules, physics, full PBR/multiple lights/shadow passes, render targets, software ray tracing and gameplay acceptance. Inventory: C:\dev\helworks\builds\helengine-webgpu\full-demodisc\scene-inventory.json.

The actual gallery has Windows overrides and no WebGPU overrides; a common-only WebGPU probe demonstrated default-value loss. Desktop authored settings must be migrated during full project integration. The preservation evidence uses actual Windows-authored inputs and does not claim current WebGPU inheritance already selects them.

Uniform packing supports one-dimensional arrays, nested structs and matrices. Multidimensional arrays require per-dimension source strides and fail explicitly. The renderer advertises one directional light, without shadow passes; compiling variants does not establish full renderer parity. Vertex-color materials require a mesh color stream and currently fail explicitly. Custom selections follow the existing file-name VS/PS/default program contract and supported resource/stream limits.

## Execution rulings

1. Keep approved checkout paths and create an engine feature branch instead of relocating a dirty checkout. Cost if wrong: explicit root override if discovery changes.
2. Inspect final Tint semantic WGSL after its public structured IR pipeline. Cost if wrong: inspect missing layout facts at an earlier IR point without changing cooked contracts.
3. Continue independent Task 4 format work while Task 3 source feedback was pending. Cost if wrong: rerun fixtures after final metadata changes; decision is now resolved and fixtures passed.
4. Follow the existing custom ShaderPackageBuilder file-name .vs/.ps and VS/PS/default convention. Cost if wrong: extend the shared request with actual definitions before accepting other conventions.
5. Preserve existing .hasset material package paths and extensionless imported texture paths, identified by WGPM magic. Cost if wrong: change shared path policy and scene references together.

Unrelated engine edits were preserved. No screenshots, videos or browser traces were captured.
