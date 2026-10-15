# WebGPU Shared Shader Pipeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the original HelEngine HLSL shaders through the shared contracts and consume their cooked WebGPU artifacts in the browser, establishing the shader/material foundation for the complete DemoDisc.

**Architecture:** Register a WebGPU shader backend and artifact cooker in the existing compilation/build pipeline. Translate HLSL with shaderc, lower SPIR-V through pinned Tint, preserve reflection and material identity, and load versioned artifacts into retained WebGPU modules/layouts/pipelines. Renderer parity for the remaining DemoDisc features follows this foundation.

**Tech Stack:** .NET 9, C#, shaderc and SPIRV-Cross via Silk.NET 2.21.0, Tint, C++20, Emscripten 6.0.12/emdawnwebgpu, WebGPU, xUnit, Playwright.

**Spec:** `C:\dev\helworks\helengine-webgpu\docs\superpowers\specs\2026-10-14-demodisc-shared-shaders-design.md` (approved by Helena).

## Global Constraints

- Register `ShaderCompileTarget.WebGpu = 10`, name `webgpu`, define `HEL_API_WEBGPU`; preserve existing target values 0–9.
- Use the supplied `ShaderBindingPolicy`: space → group, `GetSlot` → binding. Default offsets: buffers 0, textures 100, samplers 200. Reject nonzero spaces and storage resources in this stage.
- WGSP v1: 28-byte header, six little-endian uint32 fields after `WGSP`; metadata/vertex/fragment UTF-8 sections, no padding, total at most 64 MiB.
- WGPM v1: 16-byte header (`WGPM`, version, total length, JSON length); JSON payload at most 16 MiB; reject malformed lengths, UTF-8, versions and trailing bytes.
- Use `CookedPlatformOwned`; no expansion of the shared `PlatformMaterialAsset`. Do not patch generated code or depend on another platform's builder.
- All work/builds/logs stay in the workspace. Preserve unrelated changes in the dirty engine checkout, especially the existing material loader edit. Never stage whole directories there.
- One class per C# file, substantive XML comments on every member, PascalCase fields, no tuples/local helpers/nullable annotations.
- No screenshots, video or traces. Browser acceptance uses runtime diagnostics and numeric GPU readback. Tests listed here are proposed for authorization with approval of this plan; none have run during planning.
- This stage must not claim all 37 desktop scenes work. Physics replacement, execution-model changes and content reductions require Helena's feedback.

## Review Focus

1. Same HLSL register number in different resource classes must map without collision (Task 3).
2. Matrix orientation, arrays and uniform padding must retain their values after lowering (Tasks 3 and 6).
3. Unicode include paths, inactive missing includes and include content changes must preserve resolution/diagnostics/identity (Tasks 3 and 4).
4. Malformed artifact lengths/UTF-8, duplicate JSON properties and incompatible cross-stage bindings must fail before GPU use (Tasks 4 and 6).
5. A failed recompile must not publish half a shader pair or reuse stale valid-looking output (Tasks 4 and 7).

## Execution and ownership

Recommended execution: native in this session, sequential tasks, then one independent review. The tasks share reflection and artifact interfaces; keeping one implementer avoids repeated handoffs. If Helena chooses subagents instead, use gpt-6.1-sol/high, isolated context, explicit file ownership and the same review gates. Workers must preserve other people's edits.

At execution start, use the worktree skill to evaluate existing isolation. The backend is already on `feat/webgpu`; do not relocate it or the dirty engine checkout blindly. Commit each completed task separately, staging only named changes. Update this checklist and keep evidence under `C:\dev\helworks\builds\helengine-webgpu\shared-shaders`.

## Task 1: Register the WebGPU compile target

**Files:** modify `C:\dev\helworks\helengine\engine\helengine.shader\shaders\runtime\ShaderCompileTarget.cs`, `C:\dev\helworks\helengine\engine\helengine.shader\shaders\runtime\ShaderTargetDescriptors.cs`, and `C:\dev\helworks\helengine\engine\helengine.editor.tests\ShaderTargetDescriptorTests.cs`.

**Interfaces:** consume the existing enum/table API; produce `ShaderCompileTarget.WebGpu = 10` and the descriptor `(WebGpu, "webgpu", "HEL_API_WEBGPU")`. No other shared API changes.

- [ ] Add tests `WebGpu_has_stable_identity_and_define` and `Existing_target_numeric_values_are_preserved`: assert target 10, name/define above, round-trip name parsing, and unchanged values 0–9.
- [ ] Run `dotnet test C:\dev\helworks\helengine\engine\helengine.editor.tests\helengine.editor.tests.csproj --filter FullyQualifiedName~ShaderTargetDescriptorTests`; confirm the new assertions fail for the missing WebGPU target.
- [ ] Add the enum member and table row; retain all old entries and ownership attributes.
- [ ] Rerun the same command; expect all descriptor tests to pass. Commit only these files as `feat: register WebGPU shader compile target`.

## Task 2: Reproducible shader tools and Tint process boundary

**Files:** create `C:\dev\helworks\helengine-webgpu\scripts\setup-shader-toolchain.ps1`, `C:\dev\helworks\helengine-webgpu\tools\shader-tool\CMakeLists.txt`, `C:\dev\helworks\helengine-webgpu\tools\shader-tool\main.cpp`, `C:\dev\helworks\helengine-webgpu\builder\shaders\WebGpuShaderToolchain.cs`, `C:\dev\helworks\helengine-webgpu\builder\shaders\WebGpuTintCompiler.cs`, and `C:\dev\helworks\helengine-webgpu\builder.tests\WebGpuShaderToolchainTests.cs`; modify `C:\dev\helworks\helengine-webgpu\builder\helengine.webgpu.builder.csproj`.

**Interfaces:** `WebGpuShaderToolchain.Resolve(string toolRoot)` returns validated absolute paths and content fingerprints; `WebGpuTintCompiler.Lower(byte[] spirv, string workRoot, CancellationToken cancellationToken)` returns WGSL plus final-layout reflection in `WebGpuTintResult` (a separate new file beside the compiler). The native tool accepts explicit input/output paths, returns nonzero on failure, and writes WGSL and reflection JSON only after validation. Its reflection reports the lowered program, not a regex interpretation of WGSL.

- [ ] Add failing tests for missing executables, changed fingerprints, paths with spaces, cancellation, nonzero exit and malformed tool output. Assert diagnostic includes the failing tool and captured stderr.
- [ ] Run `dotnet test C:\dev\helworks\helengine-webgpu\builder.tests\helengine.webgpu.builder.tests.csproj --filter FullyQualifiedName~WebGpuShaderToolchainTests`; expect failure for the missing adapter.
- [ ] Add shader.compilation project reference, unsafe compilation, and Silk.NET Shaderc/Shaderc.Native/SPIRV.Cross/SPIRV.Cross.Native 2.21.0 package references. Keep them in the backend builder.
- [ ] Pin Tint to Dawn revision `31e25af254ab572c77054edec4946d2244e184dd`, matching the installed emdawnwebgpu VERSION. Build only its required compiler libraries and the small native adapter. Fetch official source/dependencies into `C:\dev\helworks\tools\webgpu-shaders`; store source revisions, executable/library hashes and licenses in a generated lock file there. No moving `latest` dependency.
- [ ] Use Tint's structured SPIR-V reader/lowering and WGSL writer; validate the result and collect final entry points, resources, interface and member layout from its program/IR. Do not rewrite generated WGSL text. Add the process wrapper with argument-list execution, bounded stderr/stdout, cancellation and a build-owned job directory.
- [ ] Build the tool and lower a tiny vertex/fragment fixture; confirm valid WGSL and reflection. Rerun toolchain tests; expect pass. Commit as `feat: add pinned WebGPU shader translation tools`.

If the pinned Tint cannot represent an actual shader requirement, preserve its diagnostic and stop for architectural feedback before changing compiler strategy.

## Task 3: HLSL backend, includes, reflection and pair linking

**Files:** create separate classes under `C:\dev\helworks\helengine-webgpu\builder\shaders`: `WebGpuShaderBackend.cs`, `WebGpuShaderSourceCompiler.cs`, `WebGpuShaderIncludeSession.cs`, `WebGpuShaderCompileResult.cs`, `WebGpuShaderStage.cs`, `WebGpuShaderBinding.cs`, `WebGpuShaderBufferMember.cs`, `WebGpuShaderInterfaceElement.cs`, `WebGpuShaderDependency.cs`, `WebGpuShaderProgramPair.cs`; create `C:\dev\helworks\helengine-webgpu\builder.tests\WebGpuShaderBackendTests.cs` and `C:\dev\helworks\helengine-webgpu\builder.tests\WebGpuShaderPairTests.cs`. Each class owns its named concern; do not create a single compiler file containing all DTOs.

**Interfaces:** `WebGpuShaderBackend : IShaderBackend, IShaderBackendCachePolicy`, with `Target=WebGpu`, vertex/pixel HLSL models 4.0–5.0, ray tracing false and `UseSharedCache=false`. Its shared `Compile(ShaderCompileRequest, IShaderIncludeResolver)` returns `WebGpuShaderCompileResult : ShaderCompileResult`, exposing a typed `WebGpuShaderStage Stage`. `WebGpuShaderProgramPair.Link(WebGpuShaderStage vertex, WebGpuShaderStage fragment)` returns a validated pair with merged bindings.

- [ ] Add failing tests for default b0/t0/s0 → group 0 bindings 0/100/200, deliberate policy collision, unsupported stage/space/storage, entry-point selection and variant defines.
- [ ] Add fixture tests with a nested Unicode include, an inactive missing include, include failure and changed include contents. Assert the supplied resolver is used and active dependencies are recorded by hash. Add matrix/array fixture assertions for offsets, strides and orientation from the final reflection.
- [ ] Add pair tests rejecting nonmatching varyings, resource classes/dimensions/layouts at the same slot and duplicate locations; accept a binding used by both stages with merged visibility.
- [ ] Run the two test classes using `dotnet test C:\dev\helworks\helengine-webgpu\builder.tests\helengine.webgpu.builder.tests.csproj --filter "FullyQualifiedName~WebGpuShaderBackendTests|FullyQualifiedName~WebGpuShaderPairTests"`; confirm failures before implementation.
- [ ] Implement shaderc with caller source/defines/options/includes and register-class binding shifts from the supplied policy. Use compiled reflection (SPIRV-Cross) for original semantic identity and reconcile it with Task 2's final reflection. Populate the shared `ShaderProgramDefinition` and richer WebGPU metadata; shared binary bytes contain UTF-8 WGSL. All compile failures identify source, stage, entry and variant.
- [ ] Implement pair reconciliation and deterministic ordering. Compile all three `StandardShaderVariants` and both `SpriteBatchShaderVariants` using the actual built-in source snapshots. Rerun the tests; expect pass. Commit as `feat: compile shared HLSL shaders for WebGPU`.

## Task 4: Shader artifacts and builder registration

**Files:** create `C:\dev\helworks\helengine-webgpu\builder\shaders\WebGpuShaderArtifact.cs`, `C:\dev\helworks\helengine-webgpu\builder\shaders\WebGpuShaderArtifactSerializer.cs`, `C:\dev\helworks\helengine-webgpu\builder\shaders\WebGpuShaderArtifactIdentity.cs`, `C:\dev\helworks\helengine-webgpu\builder\shaders\WebGpuShaderArtifactCooker.cs`, `C:\dev\helworks\helengine-webgpu\docs\WebGpuArtifactSchema.md`, `C:\dev\helworks\helengine-webgpu\builder.tests\WebGpuShaderArtifactTests.cs`, and `C:\dev\helworks\helengine-webgpu\builder.tests\WebGpuShaderArtifactCookerTests.cs`; modify `C:\dev\helworks\helengine-webgpu\builder\WebGpuPlatformAssetBuilder.cs`.

**Interfaces:** `WebGpuShaderArtifactSerializer.Serialize(WebGpuShaderArtifact artifact) -> byte[]`; `Deserialize(byte[] bytes) -> WebGpuShaderArtifact`; `WebGpuShaderArtifactIdentity.RelativePath(PlatformShaderDependency dependency) -> string`; `WebGpuShaderArtifactCooker.Cook(PlatformShaderArtifactCookRequest request) -> PlatformShaderArtifactCookResult`. Register the backend through `IShaderBackendRegistryContributor` and implement `IPlatformShaderArtifactBuilder` plus `IPlatformRendererShaderDependencyProvider` on the existing builder.

- [ ] Add failing byte-contract tests: exact WGSP header/lengths, deterministic serialization, UTF-8 round trip, 64 MiB limit, overflow/truncation/trailing bytes, unknown version, duplicate JSON fields and absent required metadata.
- [ ] Add cooker tests: duplicate selection compiles once; missing source/variant fails; both UI dependencies are declared; second-stage failure publishes no prepared pair; changed include/tool fingerprint changes compilation identity. Shared cache remains explicitly disabled, so no unkeyed reuse occurs.
- [ ] Run the two new classes with a `FullyQualifiedName` OR filter; confirm failure.
- [ ] Implement stable paths `shaders/<sha256>.wgsp`, hashing a length-prefixed UTF-8 sequence of asset ID, vertex program, pixel program and variant. Metadata also stores source/include/tool fingerprints. Build every requested pair in memory before publishing new files; use temporary sibling files and atomic rename, and return declarations only when the full request succeeds. The editor manifest remains authoritative after a failed request.
- [ ] Document JSON properties/types/order in the schema file. Built-ins use their shared variant catalogs; custom assets use their actual source definitions and explicit program selection. Validate request platform/profile, never supply guessed entry points.
- [ ] Rerun the tests and a real cooker invocation on all five built-in variants; expect valid declarations and no dependency on the fixed WGSL directory. Commit as `feat: cook declared WebGPU shader artifacts`.

## Task 5: Versioned material payloads preserving authored data

**Files:** create `C:\dev\helworks\helengine-webgpu\builder\materials\WebGpuMaterialArtifact.cs`, `C:\dev\helworks\helengine-webgpu\builder\materials\WebGpuMaterialArtifactSerializer.cs`, `C:\dev\helworks\helengine-webgpu\builder.tests\WebGpuMaterialArtifactTests.cs`; modify `C:\dev\helworks\helengine-webgpu\builder\WebGpuMaterialCooker.cs`, `C:\dev\helworks\helengine-webgpu\builder\WebGpuPlatformDefinitionFactory.cs`, `C:\dev\helworks\helengine-webgpu\builder.tests\WebGpuMaterialCookerTests.cs`, and `C:\dev\helworks\helengine-webgpu\docs\WebGpuArtifactSchema.md`.

**Interfaces:** preserve `WebGpuMaterialCooker.Cook(PlatformMaterialCookRequest) -> PlatformMaterialCookResult`; add `WebGpuMaterialArtifactSerializer.Serialize/Deserialize` matching Task 4's signatures for its own artifact type. Materials reference Task 4's identity function and return explicit shader dependencies.

- [ ] Add failing tests for WGPM v1, exact 16-byte header, JSON limit of 16 MiB, malformed base64/UTF-8/length/version and round-trip identity.
- [ ] Add a PBR material fixture with roughness/metallic/specular/emissive constants, texture identities, alpha/culling, vertex color, casts/receives shadow and explicit program/variant selection. Assert cooker → deserialize preserves every value and dependency. Missing required selection must fail, not select a default shader.
- [ ] Run `dotnet test C:\dev\helworks\helengine-webgpu\builder.tests\helengine.webgpu.builder.tests.csproj --filter "FullyQualifiedName~WebGpuMaterialArtifactTests|FullyQualifiedName~WebGpuMaterialCookerTests"`; confirm the missing behavior fails.
- [ ] Extend the platform schema using the existing shared field IDs and read their authored values. Serialize constants with their names and logical values/layout identity, preserving raw data where present; record imported texture references using the same identity/path rules as packaging. Add no fields to the global material asset.
- [ ] Rerun material tests; inspect cooked outputs from the actual gallery material inputs and verify PBR fields survive. Commit as `feat: preserve authored WebGPU materials and shader selections`.

## Task 6: Native readers and retained GPU programs in both renderers

**Files:** create paired `.hpp/.cpp` units under `C:\dev\helworks\helengine-webgpu\src\platform\webgpu` for `webgpu_shader_artifact_reader`, `webgpu_material_artifact_reader`, `webgpu_shader_library`, `webgpu_material_bindings`; create `C:\dev\helworks\helengine-webgpu\tests\native\shader_artifact_harness.cpp`, `C:\dev\helworks\helengine-webgpu\tests\browser\shared_shaders.spec.js`, and `C:\dev\helworks\helengine-webgpu\scripts\build-shader-tests.ps1`. Modify the existing 2D/3D renderer `.hpp/.cpp` units in that directory, `C:\dev\helworks\helengine-webgpu\src\platform\webgpu\webgpu_runtime_material.hpp`, `C:\dev\helworks\helengine-webgpu\src\platform\webgpu\webgpu_frame.hpp`, `C:\dev\helworks\helengine-webgpu\src\platform\webgpu\webgpu_frame.cpp`, `C:\dev\helworks\helengine-webgpu\builder\WebGpuBuildWorkspace.cs`, and `C:\dev\helworks\helengine-webgpu\CMakeLists.txt`.

**Interfaces:** readers consume `(std::span<const std::byte> bytes)` and return validated native artifact records matching the published schema. `WebGpuShaderLibrary` borrows the acquired device, caches shader modules/layouts/pipelines by artifact identity and complete render state, and owns their release. `WebGpuMaterialBindings` reconciles named material/engine constants and textures with final reflection. Keep the existing engine renderer override signatures; the path/stream overload loads WGPM. The legacy basic-asset overload must no longer serve as an automatic production fallback.

- [ ] Add a native harness consuming Task 4/5 generated fixtures; assert round-trip fields and rejection of corrupted lengths, UTF-8, duplicate metadata, invalid member ranges and mismatched stage metadata. Add GPU tests for b0/t0/s0 separation and a matrix/array fixture rendered to numeric readback.
- [ ] Run the new native/browser cases against the existing implementation; expect missing artifact support. Preserve the existing screenshot/video/trace-off settings.
- [ ] Use nlohmann/json 3.12.0 as a backend-local single-header dependency for the approved JSON artifact format; fetch the official tagged release with recorded SHA-256 and license in the setup script, and compile it through CMake. Reject duplicate keys and schema/type/range/depth violations at the reader boundary. No handwritten general JSON parser or generated-source rewriting.
- [ ] Implement readers, constant packing by final member layout and retained shader/pipeline ownership. Derive vertex layouts from semantic/location metadata and the actual mesh/batch streams. Maintain default textures only where an absent authored texture has a defined material meaning; an explicit missing reference must fail.
- [ ] Migrate UI Textured/RoundedShape and the Cube's ForwardStandard path to cooked artifacts. Fill the real ForwardStandard transform/light/material buffers; support the shader's required inactive-resource bindings without falsely advertising new lighting/shadow capabilities. Preserve color-space, straight-alpha/blending, clipping, camera order and resource teardown behavior.
- [ ] Stage shader artifacts from manifest paths and validate renderer-owned dependencies before native compilation. Remove the fixed WGSL copy step and production calls that load `basic_mesh.wgsl`/`ui.wgsl`. Update fixture staging tests as needed; retain the old source files only until references are removed.
- [ ] Run the harness, new shared shader browser tests and the affected existing mesh/UI tests. Expect no WebGPU validation errors, unchanged Cube/UI behavior and nonconstant numeric readbacks. Commit as `feat: consume cooked shaders and materials in WebGPU renderers`.

## Task 7: Official export, failure acceptance and documentation

**Files:** modify `C:\dev\helworks\helengine-webgpu\tests\browser\cube_test.spec.js`, `C:\dev\helworks\helengine-webgpu\tests\browser\server.js`, `C:\dev\helworks\helengine-webgpu\builder.tests\WebGpuBuildWorkspaceTests.cs`, and `C:\dev\helworks\helengine-webgpu\README.md`; create `C:\dev\helworks\helengine-webgpu\docs\superpowers\reports\2026-10-14-demodisc-shared-shaders-execution.md`.

**Interfaces:** consume the unchanged official editor CLI/platform build request. Preserve the final `.html/.js/.wasm/.data` browser package and Build Waiter state contract. Evidence must identify tool revisions, artifacts loaded, shaders/variants, exact checks run and remaining DemoDisc work.

- [ ] Update the invalid-WGSL test to target the cooked artifact identity/entry point rather than the obsolete hand-authored source marker. Add a failed recompile case and a missing shader artifact case; assert terminal diagnostics and failed publication/build status, not a stale success.
- [ ] Run only these new cases before their integration changes; confirm failure. Implement error-path fixes in their producer/owner without swallowing failures.
- [ ] Export the actual Cube selection from `C:\dev\helprojs\demodisc\project.heproj` through `C:\dev\helworks\helengine\scripts\build-platform.ps1` with platform `webgpu`, profile `release`, output `C:\dev\helworks\builds\helengine-webgpu\shared-shaders\player` and cache `C:\dev\helworks\builds\helengine-webgpu\cache`. Wrap it with the existing Build Waiter and require `index.html`, `helengine_webgpu.js`, `helengine_webgpu.wasm`, `helengine_webgpu.data`.
- [ ] Add explicit package selection to the fixture server: read `HELENGINE_WEBGPU_TEST_PLAYER_ROOT`, require its absolute path to contain the four exported files, and serve it for `/game/`. Set this variable to `C:\dev\helworks\builds\helengine-webgpu\shared-shaders\player` for this acceptance; do not silently fall back when a configured path is invalid. Keep the old Cube path only as the documented unset-variable default for existing workflows.
- [ ] Run `dotnet test C:\dev\helworks\helengine-webgpu\builder.tests\helengine.webgpu.builder.tests.csproj`, the targeted engine descriptor tests, and `npm test --prefix C:\dev\helworks\helengine-webgpu\tests\browser` against the newly exported package, with the environment variable above set. Confirm hardware adapter diagnostics and shader identities, without screenshots.
- [ ] Exercise one real custom HLSL selection through cook → package → browser and confirm its identity and output. Recompile after an include change; verify new fingerprints/output. Reintroduce an intentional compile error in an isolated fixture and confirm failure leaves no accepted new manifest.
- [ ] Record actual results, failures and limitations. Document that PBR/shadow variants are compiled/prepared but multi-light/shadow rendering and full scene acceptance remain follow-up work; retain the inventory and next steps for all 37 scenes. Commit as `docs: record shared WebGPU shader pipeline acceptance` only after claims match evidence.

## Self-review and handoff

Spec coverage: shared integration → Tasks 1/3/4; pinned translation/includes/reflection → Tasks 2/3; both versioned formats → Tasks 4/5/6; runtime ownership/migration → Task 6; acceptance/error behavior → Task 7. Every Review Focus item has an owning test step. The complete DemoDisc objective remains open after this plan; the next design/plan covers renderer parity and scene/game acceptance without silently reducing the inventory.

No product code, new tests, tool installs or test execution were performed while writing this plan. Execution begins after Helena reviews this file and chooses native or subagent-driven execution. Plan approval should explicitly authorize the validation steps above.
