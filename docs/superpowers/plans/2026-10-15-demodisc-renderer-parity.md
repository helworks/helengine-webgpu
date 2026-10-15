# DemoDisc Renderer Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Preserve the previously selected native execution method: the primary agent implements; one fresh independent reviewer checks the finished branch.

**Goal:** Export the complete 37-scene desktop DemoDisc through the official HelEngine builder and run its original gameplay, physics, materials, lighting and software ray tracing in the browser.

**Architecture:** Extend the existing C# → generated C++ → WASM player and shared HLSL → shaderc/SPIR-V → pinned Tint → WGSP pipeline. Use native depth textures and comparison samplers for the directional/spot atlas and point cubemaps, preserving the engine's light-selection, projection, radial-depth and material contracts. Integrate project content through authored WebGPU overrides and the official manifest; keep the project-selected physics backend and execution model.

**Tech Stack:** Emscripten 6.0.12; emdawnwebgpu; Dawn/Tint 31e25af254ab572c77054edec4946d2244e184dd; Silk.NET shaderc/SPIRV-Cross 2.21.0; C++20; existing .NET builder and xUnit; existing Playwright hardware-WebGPU harness.

**Spec:** C:\dev\helworks\helengine-webgpu\docs\superpowers\specs\2026-10-15-demodisc-renderer-parity-proposal.md (native depth architecture approved 2026-10-15).

## Execution status — 2026-10-15

The original steps below describe the planned workflow. This table records the implemented outcome and the source-contract adaptations discovered during execution.

| Tasks | Outcome | Evidence |
| --- | --- | --- |
| 1–5: shader contracts, depth resources, all light families and shadows | Implemented | Backend commits 8375c9c, 59cb8f5, 6121d8e, c5ce007, 971ad5e; numeric atlas/cube/receiver fixtures |
| 6: geometry and camera targets | Implemented for the existing ModelAsset contract | Commit 8f0b908; target chaining, uniform growth, shared transparency and mesh fixtures. Original ModelAsset has no color stream and only TriangleList/LineList; required missing color streams remain explicit errors. None of the 77 DemoDisc materials requests them. |
| 7: desktop content | Implemented | 37 unique scene IDs, 77 materials, 25 exact gallery parameter sets, HD membership and original menu actions; audit proposes zero changes |
| 8: original runtime modules and selected physics | Implemented | Official ninth export succeeds with four modules and HelPhysics; original ray tracer, kinematic paths, disposal and Tilt scene-registration fixes have source regressions |
| 9: complete acceptance and handoff | Complete | Official final export 1c9e0b54-3c23-4430-a158-f73c2d1d0ba0 succeeded. All89 browser cases pass, including all37 exact manifest scene IDs;93 managed,5 Node,4 runner and5 real preload rebuild checks pass. Four artifact hashes verified. Independent review findings closed. Original-content limits are documented in the execution report. |

Final evidence and original-content limits are recorded in C:\dev\helworks\helengine-webgpu\docs\superpowers\reports\2026-10-15-demodisc-renderer-parity-execution.md. Only C:\dev\helworks\helengine-webgpu is integrated into local main, as requested; engine/project branches remain separate.

## Global Constraints

- Include 37 of 39 authored scenes. Exclude only `DemoDiscMainMenuHandheld.helen` and `games/tilt/tilt_trial_ds.helen`. Windows' existing 23-scene selection is not the inventory.
- Preserve modules DemoDisc, TiltPlay, Zombislayer and Raytracing; editor-only code is not a runtime module.
- Preserve selected physics backend, authored solve schedule and single-threaded browser execution. A required backend replacement or execution-model change needs Helena's architectural feedback.
- Keep WGSP v1/WGPM v1 envelopes, canonical shader selection paths, `.hasset` material paths and extensionless `cooked/imported/<assetId>` texture paths.
- Extend metadata only with actual final Tint facts. Never rewrite generated WGSL or generated C++; fix source/compiler producers.
- Keep bindings bN → N, tN → 100+N, sN → 200+N, group 0. Unsupported resource classes/spaces remain explicit failures.
- Shadow resources: Depth32Float; atlas 2048×2048; point cube 512×512×6, matching the existing managed desktop planner dimensions. Four forward slots and at most four shadowed direct lights; retain the configured lower budget.
- Preserve receiver bias 0.0015 for atlas and 0.01 for normalized radial point depth. Use nearest comparison sampling initially to preserve the existing single-comparison visibility; do not introduce an unrequested PCF kernel.
- Do not silently replace authored material values, textures, lights, required shaders, code modules or runtime services with defaults.
- Screenshots/video/traces remain off. Validate GPU output with numeric readbacks and runtime state.
- Outputs/logs belong below C:\dev\helworks\builds\helengine-webgpu\renderer-parity. Do not direct agent-owned output to TEMP.
- Preserve unrelated engine and Windows worktree changes. Stage only owned files. C# follows C:\dev\helworks\helengine\AGENTS.md.
- Execution approved by Helena; proceed through all nine tasks without renewed routine approval gates.

## Review Focus

1. A selected point light changes slot after another light is removed: the cube, slot metadata and sampler must still describe the same light (Tasks 3/5).
2. Camera B samples a render target written by camera A, then a scene unloads: attachments must be ordered correctly and remain alive through GPU submission (Task 6).
3. A shadow-casting object lies outside the main camera view but shadows a visible receiver: camera culling must not remove it from the light pass (Tasks 3/4).
4. A desktop override exists only in platform-specific material data: the WebGPU cook must preserve effective authored values and asset references, including all 25 gallery materials (Task 7).
5. Repeated menu/game transitions, focus loss and ray-tracer texture uploads: original gameplay/services must remain registered once, input must release, and GPU/CPU allocations must retire (Tasks 8/9).

## Scope and Deliverables

The work is grouped into independently verifiable subsystems in one ordered plan: shader/resource contracts (1–2), renderer (3–6), project/runtime integration (7–8), complete player acceptance (9). Completing an intermediate subsystem does not complete the 37-scene objective.

Paths in task file lists are relative to these explicit roots:

- **W:** C:\dev\helworks\helengine-webgpu
- **E:** C:\dev\helworks\helengine
- **D:** C:\dev\helprojs\demodisc
- **B:** C:\dev\helworks\builds\helengine-webgpu\renderer-parity

## Reference Map

- E\engine\helengine.directx11\DirectX11Renderer3D.cs: frame extraction, light budgeting, shadow pass order, caster eligibility and six point faces. Use this full implementation for spot/point parity; the native Windows bridge currently covers a narrower directional path.
- E\engine\helengine.directx11\rendering\DirectX11LightSelectionService.cs and DirectX11ForwardLightShaderDataBuilder.cs: descending submission importance, ambient accumulation and four packed direct-light slots.
- E\engine\helengine.core\managers\rendering\RenderFrameLightClassifier.cs: shadow bonus 1000 plus intensity×100 rounded away from zero.
- E\engine\helengine.directx11\rendering\DirectX11ShadowResourcePlanner.cs, DirectX11ShadowMapAtlas.cs and DirectX11ShadowShaderDataBuilder.cs: budgets, square-grid atlas, projections, metadata and cube face directions/up vectors.
- E\engine\helengine.editor\shaders\builtin\EditorShadowDepth.hlsl: projected depth pass. EditorPointShadowDepth.hlsl: normalized radial depth pass. ForwardStandardShader.hlsl: existing receiver equations.
- D\assets\codebase\menu.authoring\DemoDiscSceneCatalog.cs: actual menu actions. Only three physics examples are currently listed; package the other authored physics scenes too and provide desktop menu entries for them.
- E\engine\helengine.editor\managers\project\EditorPhysics3DRuntimeFeatureRequirementCollector.cs: scene-derived requirements; raw inventory physicsFlags=0 is not evidence that no physics backend is needed.
- [WGSL comparison sampling](https://gpuweb.github.io/gpuweb/wgsl/#texturesamplecomparelevel) and [fragment depth](https://gpuweb.github.io/gpuweb/wgsl/#builtin-values): verify behavior against the pinned compiler/device, not merely the current spec text.

## Validation Commands

Run commands from W unless stated otherwise. Redirect full logs into B, show bounded tails and retain nonzero exit codes. Direct dotnet invocations use `--disable-build-servers`; official export also sets MSBUILDDISABLENODEREUSE=1, DOTNET_CLI_USE_MSBUILD_SERVER=0, UseSharedCompilation=false.

**Managed (M):** `dotnet test builder.tests/helengine.webgpu.builder.tests.csproj --disable-build-servers --artifacts-path C:/dev/helworks/builds/helengine-webgpu/renderer-parity/dotnet --filter <task test class>`.

**GPU fixtures (G):** `powershell -NoProfile -File scripts/build-shader-gpu-tests.ps1 -OutputRoot C:/dev/helworks/builds/helengine-webgpu/renderer-parity/shader-tests`. Update the browser fixture routing to an explicit absolute fixture root in Task 2, preserving the existing default when unset.

**Browser (P):** from W\tests\browser, set `HELENGINE_WEBGPU_TEST_PLAYER_ROOT=C:\dev\helworks\builds\helengine-webgpu\renderer-parity\player` and `HELENGINE_WEBGPU_TEST_SHADER_ROOT=C:\dev\helworks\builds\helengine-webgpu\renderer-parity\shader-tests`, then `npx playwright test <task spec> --reporter=line`. No screenshots/video/traces.

**Official export (X):** build the backend's default plugin DLL first with `dotnet build builder/helengine.webgpu.builder.csproj --no-dependencies --disable-build-servers`; then:

```powershell
dotnet run --no-build --project C:/dev/helworks/helengine/tools/build-waiter/helengine.buildwaiter.csproj -- --output C:/dev/helworks/builds/helengine-webgpu/renderer-parity/player --require index.html --require helengine_webgpu.js --require helengine_webgpu.wasm --require helengine_webgpu.data -- powershell -NoProfile -ExecutionPolicy Bypass -File C:/dev/helworks/helengine/scripts/build-platform.ps1 -Project C:/dev/helprojs/demodisc/project.heproj -Platform webgpu -Output C:/dev/helworks/builds/helengine-webgpu/renderer-parity/player -BuildProfile release -CacheRoot C:/dev/helworks/builds/helengine-webgpu/cache
```

Expected X: Build Waiter succeeded/exit 0, four fresh nonempty files and a manifest matching the requested scenes/modules. A failed build must not be accepted using stale output. M/G/P commands and new tests below become authorized validation when this written plan is approved.

---

### Task 1: Compile and validate depth/comparison shader contracts

**Files:** Modify W\tools\shader-tool\main.cpp; W\builder\shaders\WebGpuShaderBackend.cs, WebGpuShaderProgramPair.cs, WebGpuShaderArtifactSerializer.cs; W\src\platform\webgpu\webgpu_shader_artifact_reader.cpp; E\engine\helengine.editor\shaders\builtin\ForwardStandardShader.hlsl, EditorPointShadowDepth.hlsl. Create W\builder.tests\WebGpuShadowShaderTests.cs. Extend W\tests\native\shader_artifact_harness.cpp and W\docs\WebGpuArtifactSchema.md.

**Interfaces:** Preserve `WebGpuShaderBinding` constructor/properties and `WebGpuShaderProgramPair.Link(WebGpuShaderStage, WebGpuShaderStage)`. Accept `Class="depth-texture"` for dimensions 2D/cube and `Class="sampler"` for regular/comparison sampler kinds reflected from pinned Tint. Map depth resources to existing shared Texture2D/TextureCube resource families; retain depth/comparison facts in backend metadata. Map all other classes explicitly to a supported case or reject.

- [ ] Write `WebGpuShadowShaderTests`: `Depth2D_and_cube_keep_comparison_reflection`, `Comparison_binding_conflict_is_rejected`, `Point_pass_preserves_radial_fragment_depth`, `Non_webgpu_shadow_source_keeps_existing_outputs`. Assert source registers t1/t2–5/s1/s2, final depth resource class/dimension, comparison sampler kind, and radial `SV_Depth` lowering. Native malformed-kind cases must fail before device allocation.
- [ ] Run M filtered to WebGpuShadowShaderTests; record the expected unsupported-class/regular-sampler failures. Do not accept compiler installation failures as RED evidence.
- [ ] Extend Tint resource classification for comparison samplers, validators and shared projection. Read enum values from the pinned Dawn headers; name those values once rather than guessing numeric values. Guard HLSL depth declarations and `SampleCmpLevelZero` receiver calls with `HEL_API_WEBGPU`. Change point helper to take the reference depth and return visibility; preserve receiver strength/bias equations. Under the same define, point PS returns normalized distance through `SV_Depth`; other platforms retain their original color output. No changes to original shader registers or material names.
- [ ] Run M, native artifact harness and the existing Windows DX11/Vulkan standard shader tests. Rebuild the pinned adapter and update its lock hashes using the existing setup script. Expect all selected tests green; generated WGSL must declare depth textures, sampler_comparison and fragment depth where required.
- [ ] Commit owned engine shader changes separately from backend changes: `feat: compile native WebGPU shadow resources`.

### Task 2: Bind depth resources and cache attachment-specific pipelines

**Files:** Modify W\src\platform\webgpu\webgpu_shader_library.hpp/.cpp; W\tests\fixtures\WebGpuShaderFixtureGenerator.cs; W\tests\native\shared_shader_gpu_harness.cpp; W\tests\browser\server.js, player-root.js. Create W\tests\browser\shadow_resources.spec.js and shader-root.js; extend server_configuration.test.cjs.

**Interfaces:** Add `struct WebGpuPipelineTarget` in W\src\platform\webgpu\webgpu_pipeline_target.hpp with `WGPUTextureFormat ColorFormat`, `DepthFormat`; `bool HasColor`; `int32_t DepthBias`; `float DepthBiasSlopeScale`. Add `Pipeline(WebGpuShaderProgram&, bool ui, const ArtifactJson&, const WebGpuPipelineTarget&)`; keep existing three-argument call delegating to the current surface/depth target. Preserve `CreateBinding(WebGpuShaderProgram&, const std::map<std::string,WGPUTextureView>&)`.

- [ ] Add `shadow_resources.spec.js`: depth2D and all six depth-cube faces produce expected 0/1 visibility on either side of known 0.25/0.75 depths; original float texture fixture still reads exact RGBA [32,64,96,255]. Same shader/state on color, atlas and cube target descriptors must get distinct compatible pipelines. Invalid explicit shader fixture roots fail instead of silently selecting another fixture.
- [ ] Run G/P and confirm RED from missing depth layouts/target overload, using a fixture that compiled in Task 1.
- [ ] Build depth layouts with WGPUTextureSampleType_Depth and comparison sampler layouts; use nearest LessEqual samplers. Retain regular texture sampling. Add depth-only initialized 1×1 2D/cube resources cleared to 1 for inactive engine shadow slots only. Create depth-only pipelines with zero color targets and matching fragment outputs. Cache keys include formats, color presence, depth bias/slope and complete authored state; reject material depth textures missing their explicit resource.
- [ ] Add strict explicit fixture-root routing, run G/P plus original shared_shaders.spec.js and Node server tests. Assert no uncaptured WebGPU validation errors; depth resources are never reinterpreted as float color views.
- [ ] Commit: `feat: bind WebGPU comparison samplers and depth pipelines`.

### Task 3: Select and pack all authored light families

**Files:** Create W\src\platform\webgpu\webgpu_light_state.hpp/.cpp and webgpu_shadow_plan.hpp/.cpp. Modify webgpu_render_manager_3d.hpp/.cpp. Create W\tests\browser\forward_lights.spec.js; extend GPU fixture/harness from Task 2.

**Interfaces:** `WebGpuLightState::Prepare(Core* core, ICamera* camera, int32_t maximumVisibleLights)` selects visible enabled lights using the shared classifier importance contract and desktop budget order; `ArtifactJson WebGpuLightState::ForwardValues() const` returns named float4 values for the existing buffer. `WebGpuShadowPlan::Prepare(ICamera*, const WebGpuLightState&, int32_t maximumShadowedLights)` supplies selected original LightComponent pointers, atlas tile rectangles, point cube indices and untransposed light view-projection matrices. Light/plan state borrows engine objects for one camera only.

- [ ] Write fixture assertions: ambient sums authored color×intensity; directional/point/spot match their type/range/cone fields; disabled and wrong-layer lights disappear; more than four direct lights follows the desktop selection budget; zero lights gives zero direct/ambient radiance for lit materials. Unlit material remains independent of lights. A removed light cannot leave prior slot values behind.
- [ ] Run P forward_lights.spec.js; expect current one-directional/hardcoded-ambient failures.
- [ ] Implement native equivalents of the mapped desktop algorithms without bringing DirectX assemblies into generated Core. Use existing LightDirectionUtility and engine matrix routines. Preserve ambient handling and budget semantics as tested by the desktop builders; do not assume ambient occupies a direct shader slot. Pack four named slots through the existing final-layout packer. Build grid atlas and point mappings with consistent selected light identity. Camera culling affects forward submissions; shadow planning retains eligible off-camera casters.
- [ ] Run P plus native/managed reference vectors for mixed ordering, spot cone cosine, directional camera focus, six face matrices, empty shadow budget and point reassignment. Expect bounded four-slot output and matching source equations, without ad hoc ambient 0.2.
- [ ] Commit: `feat: preserve authored WebGPU forward lights and shadow planning`.

### Task 4: Render directional and spot atlas shadows

**Files:** Create W\src\platform\webgpu\webgpu_shadow_renderer.hpp/.cpp. Modify webgpu_render_manager_3d.hpp/.cpp, webgpu_runtime_material.hpp and W\builder\WebGpuPlatformAssetBuilder.cs. Extend W\builder.tests\WebGpuBuildWorkspaceTests.cs; create W\tests\browser\atlas_shadows.spec.js.

**Interfaces:** `WebGpuShadowRenderer(WebGpuFrame&)`; `void Render(ICamera*, const WebGpuShadowPlan&)`; `std::map<std::string,WGPUTextureView> TextureBindings() const`; `ArtifactJson ShadowValues() const`. It owns atlas/cube resources, retains cooked depth programs, and borrows current frame encoder. Add renderer dependency `("EditorShadowDepth", "EditorShadowDepth.vs", "EditorShadowDepth.ps", "default")`, entries VS/PS, matching the existing file-backed built-in convention. Assert the official editor resolves that source snapshot before publishing the dependency; use E\engine\helengine.editor\managers\project\EditorPlatformAssetCookService.cs if its renderer-source resolver needs extending. Never synthesize a source snapshot inside the WebGPU runtime.

- [ ] Add atlas fixture cases: directional and spot receiver brightness decreases with a caster, strength 0 restores brightness, castsShadows=false removes the shadow, receivesShadows=false leaves receiver lit; two atlas tiles cannot overwrite one another. Off-camera caster still shadows an on-camera receiver. Include changed camera orientation, viewport and light movement.
- [ ] Run P atlas_shadows.spec.js; record RED from absence of shadow passes, not timeout alone.
- [ ] Allocate retained Depth32Float atlas, clear once per camera shadow execution, set tile viewport/scissor, render eligible opaque triangle submeshes using EditorShadowDepth WGSP, then end pass before forward sampling. Respect hierarchy/layers/root material shadow flags and geometry transforms; do not reuse main-camera visibility to reject light casters. Fill ShadowBuffer metadata/matrices with the same plan used for rasterization. Start with zero raster depth bias plus existing receiver bias; change only with measured desktop parity evidence, never copy DX11 integer bias blindly.
- [ ] Run M renderer dependency tests and G/P atlas fixtures. Assert zero color attachments in depth pass, expected tile occupancy/receiver numeric changes and no read/write attachment conflict. Advertise directional/spot shadow support only after these pass.
- [ ] Commit: `feat: render WebGPU directional and spot shadow atlas`.

### Task 5: Render radial point shadows across six faces

**Files:** Modify W\src\platform\webgpu\webgpu_shadow_renderer.hpp/.cpp and light/plan integration; W\builder\WebGpuPlatformAssetBuilder.cs. Create W\tests\browser\point_shadows.spec.js; extend the builder dependency test and GPU fixtures.

**Interfaces:** Reuse Task 4 interfaces; point entries in `WebGpuShadowPlan` map one selected light to a cube index and six views. Declare `("EditorPointShadowDepth", "EditorPointShadowDepth.vs", "EditorPointShadowDepth.ps", "default")`, entries VS/PS, through the existing renderer dependency provider and official source resolver from Task 4.

- [ ] Write fixture tests with near/far blockers on +X, -X, +Y, -Y, +Z, -Z; normalized radial depth must be distance/range, not perspective Z. Verify two point lights, mixed atlas+point lights, zero strength, far-range boundary, changed range and removal/reassignment of an earlier light.
- [ ] Run P point_shadows.spec.js; expect missing point passes/resources.
- [ ] Allocate retained six-layer Depth32Float cube storage and individual 2D face render views. Render original radial depth into fragment depth with the Task 1 shader; clear each face to 1. Use mapped desktop face direction/up vectors and 90° projection, original radial range and bias. Bind cube sampling views to the corresponding original pointShadowTextureN names. Reset inactive slots and release replaced resources only after their submission is safe.
- [ ] Run G/P atlas and point fixtures, including six face-edge directions and light-index reassignment. Assert all expected blockers reduce visibility, unblocked rays remain lit, and renderer capability maximum shadowed-light count matches the implemented four-light budget.
- [ ] Commit: `feat: render radial WebGPU point shadow cubemaps`.

### Task 6: Complete authored geometry and camera target behavior

**Files:** Modify W\src\platform\webgpu\webgpu_render_manager_3d.hpp/.cpp, webgpu_render_manager_2d.hpp/.cpp, webgpu_runtime_model.hpp and webgpu_shader_library.hpp/.cpp. Create webgpu_render_target.hpp/.cpp and webgpu_camera_target.hpp/.cpp. Create W\tests\browser\renderer_parity.spec.js; extend native GPU harness.

**Interfaces:** `WebGpuRenderTarget : public RenderTarget` owns color/depth textures and sample view. Override `RenderTarget* WebGpuRenderManager3D::CreateRenderTarget(int32_t width, int32_t height)`, the existing engine creation contract. `WebGpuCameraTarget ResolveCameraTarget(WebGpuFrame&, ICamera*)` returns borrowed views, dimensions and `WebGpuPipelineTarget`, resolving canvas or actual runtime target. Reject nonpositive sizes; do not invent a parallel content reference format. Extend pipeline mesh stream handling to the actual authored semantic/stride layout, preserving missing required stream errors.

- [ ] Write fixture tests for vertex colors, indexed submeshes, negative/mirrored transforms and authored cull state; transparent back-to-front blending with a rotated camera; split viewport, clear/load settings, 3D/UI interleaving and target sampling by a later camera. Dispose the producing scene after submission and verify the consuming scene does not read freed resources.
- [ ] Run P renderer_parity.spec.js; confirm explicit unsupported color-stream/offscreen errors or numeric mismatches.
- [ ] Preserve mesh color streams at upload and select vertex layouts from actual mesh attributes plus final shader semantics. Respect engine queue order/layer/hierarchy/material states. Resolve per-camera dimensions/formats for projection, attachments, viewport and UI scissor; share sampleable target views between 2D/3D without casting a RenderTarget to WebGpuRuntimeTexture. Keep surface format behavior unchanged for the current Cube path. Implement resource ownership through runtime wrapper destruction and frame submission lifetimes.
- [ ] Run G/P parity fixtures and existing mesh_rendering.spec.js/shared_shaders.spec.js. Assert no per-draw pipeline/texture creation, no attachment feedback within a pass, correct clear retention, and exact transparency ordering from shared CameraDepthOrder behavior.
- [ ] Commit: `feat: preserve WebGPU geometry and camera render targets`.

### Task 7: Author WebGPU desktop content and all 37 build scenes

**Files:** Modify D\settings\platform-groups.json, build_config.json; D\assets\codebase\DemoDiscEditorTools\rendering\PbrMaterialGalleryMaterialFactory.cs; D\assets\codebase\menu.authoring\DemoDiscSceneCatalog.cs. Create D\assets\materials\**\*.webgpu.hasset only for missing required WebGPU overrides. Create W\scripts\prepare-demodisc-webgpu.ps1, W\tests\content\DemoDiscWebGpuContentVerifier.cs, Program.cs, helengine.webgpu.content.csproj and W\builder.tests\DemoDiscWebGpuInventoryTests.cs.

**Interfaces:** Preparation script parameters `[string]$ProjectRoot`, `[switch]$Apply`; default is a report/diff. Use the official MaterialAssetSettingsService/MaterialAssetReferenceProjectionService and project settings APIs through the isolated content utility, whose csproj references E\engine\helengine.editor\helengine.editor.csproj and W\builder\helengine.webgpu.builder.csproj. `DemoDiscWebGpuContentVerifier.Verify(string projectRoot, string reportPath)` produces B\content-audit.json with scene path/id, effective material fields/references, module dependencies and expanded runtime feature requirements; it fails on missing required values. Builder tests check inventory/config JSON without adding editor dependencies to the production builder or GPU fixture project. Run the content utility with `dotnet run --project tests/content/helengine.webgpu.content.csproj --disable-build-servers -- --project-root C:/dev/helprojs/demodisc --report C:/dev/helworks/builds/helengine-webgpu/renderer-parity/content-audit.json`; preparation adds `--apply` only when requested.

- [ ] Write `Inventory_has_exact_37_desktop_scenes`, `Gallery_preserves_25_authored_parameter_sets`, `Preparation_is_idempotent_and_preserves_existing_overrides`, `All_desktop_physics_scenes_have_menu_actions`. Assert scene list equals all authored scenes minus the two named exclusions; M0R0…M4R4 preserve exact Windows effective fields/references; existing WebGPU authored overrides win; second Apply produces no diff.
- [ ] Run M inventory tests and verifier dry run; expect Cube-only selection, missing HD membership/material values and missing physics menu actions.
- [ ] Add webgpu to HD; populate WebGPU scenes with original stable references/content IDs and splash startup. Add absent WebGPU material values from effective Windows settings through the matching standard schema; never bulk-copy foreign platform schema fields. Update gallery authoring so regeneration retains WebGPU values. Add the nine missing desktop physics entries without changing handheld selection. Apply only reviewed deterministic project changes; project writes outside workspace use the required filesystem escalation.
- [ ] Run verifier and smallest existing DemoDisc catalog/material tests; confirm 37 = 3 boot/menu/loading + 7 games + 12 physics + 14 rendering + 1 benchmark. Compare other platform settings and existing overrides before/after; expect no changes. Capture expanded blueprint dependencies instead of relying on raw physicsFlags.
- [ ] Commit only owned project/tool changes in their respective repositories: `feat: select complete desktop DemoDisc content for WebGPU`.

### Task 8: Integrate manifest-selected physics and original gameplay modules

**Files:** Modify W\builder\WebGpuBuildWorkspace.cs and W\CMakeLists.txt only if official handoff misses required units; W\src\platform\web\web_application.hpp/.cpp, web_input_backend.hpp/.cpp and web_audio_backend.hpp/.cpp as defects require. Create W\builder.tests\WebGpuRuntimeManifestTests.cs and W\tests\browser\demodisc_runtime.spec.js. Changes to generated-code producers must be in E's existing producer file identified by the failing official export, never generated output.

**Interfaces:** Preserve `RegisterGeneratedRuntimeModules(Core*)`, official runtime scene/startup/code-module manifests and selected runtime requirement resolution. Generated registration is the owner of module/runtime registration; do not add a second unconditional Bepu or HelPhysics registration. Extend existing `window['helengineDiagnostics']` with quoted observation-only fields for selected runtime type, registered module IDs and scene/gameplay state; do not expose a replacement JavaScript game loop.

- [ ] Write manifest tests: all four runtime modules and expanded scene requirements are included; editor-only modules are excluded; omitted required units or unresolved runtime requirements fail with the original module/feature identity. Browser tests assert exactly one physics runtime registration, falling body motion/contact, trigger enter/exit, kinematic interaction and scene unload/reload.
- [ ] Run M and X with Task 7 selection; retain first producer/compiler/runtime error and its manifest provenance. A successful compile without physics registration is not success.
- [ ] Resolve required generated units through the official handoff and register generated modules before loading splash. Fix ordinary conversion defects in their producers, with a minimal regression in the producer's existing test project. Preserve the selected backend and solve schedule. If the official generated registration already installs physics, add no extra registration. If the selected backend cannot run without replacement/threads/model changes, document the evidence and request architectural feedback while continuing independent renderer/content work.
- [ ] Run M/X/P demodisc_runtime.spec.js. Verify original C# components produce motion, triggers and game state; keyboard/gamepad/mouse work through engine input; focus loss releases controls; audio unlock follows real interaction and playback failures are surfaced. Keep exceptions and RTTI enabled. Do not substitute simplified physics or JavaScript gameplay.
- [ ] Commit each independent producer defect with its regression, then backend integration: `feat: integrate DemoDisc runtime modules in WebGPU player`.

### Task 9: Accept every scene through the real DemoDisc flow

**Files:** Create W\tests\browser\demodisc_full_flow.spec.js, demodisc_scene_cases.js and W\scripts\verify-demodisc-webgpu.ps1. Modify W\tests\browser\playwright.config.js only for per-test timeouts/fixture roots. Create W\docs\superpowers\reports\2026-10-15-demodisc-renderer-parity-execution.md; update backend capability docs to verified facts.

**Interfaces:** Scene-case table names every Task 7 scene ID, actual menu/level-select action sequence, return action and one scene-specific behavioral assertion. Use normal keyboard/gamepad controls to drive engine menus and original code; diagnostics observe progress. The verifier invokes X, explicit-root P and validates Build Waiter state/manifest/artifact freshness. It never accepts an old Cube package as full-player evidence.

- [x] Write RED flow cases for splash→desktop menu→loading→each demo/game→return. Cover all 12 physics scenes, all 14 rendering scenes, Tilt hub and five levels, Zombislayer and benchmark; verify boot/menu/loading as their own three cases. Assert each scene's actual loaded ID, required runtime components, advancing updates and expected render/gameplay observation. Software ray scenes must show increasing original sample counts and changed uploaded texture pixels; a static image does not pass.
- [x] Run P demodisc_full_flow.spec.js against the explicit X output and report each unmet scene separately. Fix defects at their owning layer; maintain the scene table rather than weakening assertions or removing failing scenes.
- [x] Exercise three complete menu/game/return cycles, additive level load/unload, ray-tracer cancellation on return, focus loss/resume, canvas resize and benchmark object growth. Assert no stale controls, duplicate services, uncaptured GPU errors, unresolved assets, runaway retained resources or detached running scripts.
- [x] Run all builder tests, native readers, GPU fixtures, explicit-root browser suite and final X. Retain logs and per-scene results in B. Acceptance requires 37/37 scene cases and all required artifacts, authored gallery checks, all shadow families, selected physics and four original modules. Keep screenshots/video/traces off.
- [x] Obtain one fresh whole-branch review under executing-plans; fix findings and rerun affected checks before claiming completion. Reviewer default model is gpt-6.1-sol with high reasoning per Helena's AGENTS instructions unless she specifies otherwise. Preserve unrelated dirty files.
- [x] Commit final fixes/report; present the full-player preview URL, exact output path, scene count and any remaining evidenced limitation. A blocking architectural issue leaves the objective incomplete; do not label the Cube preview as the complete DemoDisc.

## Self-review and Handoff

- Spec coverage: content/HD/materials → Task 7; lights/shadows → 1–5; culling/transparency/targets → 3/4/6; original physics/modules/codegen → 8; real navigation/software ray tracer/fresh export → 9.
- Review Focus ownership: slot reassignment → 3/5; target lifetime → 6; off-camera caster → 4; material inheritance → 7; repeated lifecycle/uploads → 8/9.
- All generated-code defects are fixed in producers; no task authorizes a physics backend swap or an alternative shadow representation.
- New helper interfaces are backend-internal. Engine contracts, content identity and external build entrypoints remain the existing ones.
- The shadow architecture is approved. Implementation plan execution is approved; native execution is active.
