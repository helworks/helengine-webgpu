# WebGPU cooked artifact schema v1

Implementation status: the managed shader writer/reader and cooker exist. Native readers and GPU renderer migration are not implemented yet. ForwardStandard fragment compilation is awaiting Helena's decision about explicit shadow-map LOD; do not treat this document as full DemoDisc acceptance.

## WGSP envelope

All integers are unsigned little-endian uint32. The total file length is at most 67,108,864 bytes.

| Offset | Field | Required value |
| --- | --- | --- |
| 0 | ASCII magic | WGSP |
| 4 | version | 1 |
| 8 | header size | 28 |
| 12 | total file size | exact file size |
| 16 | metadata byte count | nonzero |
| 20 | vertex WGSL byte count | nonzero |
| 24 | fragment WGSL byte count | nonzero |

The sections immediately follow the header in the order metadata, vertex WGSL, fragment WGSL. There are no terminators, padding or trailing bytes. Each section uses strict UTF-8. Metadata keys are canonicalized in ordinal order, arrays retain their defined order, and duplicate keys are rejected recursively. JSON nesting is limited to 32.

## Shader metadata

| Property | Type | Meaning |
| --- | --- | --- |
| shaderAssetId | string | Original shared asset identity |
| vertexProgram | string | Explicit logical vertex program |
| pixelProgram | string | Explicit logical pixel program |
| variant | string | Explicit selected variant |
| target | string | webgpu |
| toolchain | object | fingerprint, dawnRevision, adapterVersion strings |
| dependencies | array | Objects with path and hash strings, unique by path and sorted ordinally |
| vertexEntryPoint | string | Final compiler vertex entry point |
| fragmentEntryPoint | string | Final compiler fragment entry point |
| vertexWgslSha256 | string | Lowercase SHA-256 of exact UTF-8 vertex section |
| fragmentWgslSha256 | string | Lowercase SHA-256 of exact UTF-8 fragment section |
| vertexInputs | array | Original semantics reconciled with final vertex locations |
| vertexOutputs | array | Original semantics reconciled with final varying locations |
| fragmentInputs | array | Original semantics reconciled with final varying locations |
| bindings | array | Merged active resources sorted by group and binding |

Stage text hashes bind the metadata to the exact two WGSL sections and reject altered sections before GPU use. The complete source snapshot and every active include participate in dependency content identities. The backend disables the shared compile cache until its cache contract includes the complete native tool identity.

An interface element contains `name`, `semantic`, `semanticIndex`, `location`, `type`, and `interpolation`. `type` is the original SPIRV-Cross base type, bit width, vector width and matrix-column count separated by colons. `interpolation` contains the final pinned Tint interpolation-type and interpolation-sampling integer values separated by a colon. Original semantic identities come from compiler decorations, not HLSL string parsing. Locations must be nonnegative and unique in each interface.

A binding contains `name`, `class`, `registerIndex`, `registerSpace`, `group`, `binding`, `size`, `layout`, `sourceLayout`, `textureDimension`, `sampleType`, `samplerType`, and `stages`. This profile accepts constant-buffer, texture and sampler classes in space/group zero. Stages are distinct `vertex`/`fragment` strings. Resource-class policy shifts default to 0/100/200; the original register index is preserved separately.

`layout` preserves the final Tint semantic type layout: type name, size and alignment; structures contain members with names, offsets, sizes and recursive type properties; arrays contain arrayStride, arrayCount and element; matrices contain matrixStride, columns, rows and column-major orientation. The native reader must use these properties for packing and validate their ranges before GPU use.

`sourceLayout` preserves the original compiled buffer size and members. Each member contains name, type, offset, size, arrayStride, matrixStride, matrixOrientation and aggregate. The aggregate records array dimensions, rows, columns and an optional recursive structure. These are compiled SPIR-V layouts; do not assume a source-language matrix annotation alone determines byte order after compilation. Non-buffer resources have empty layout objects.

Texture/sample/sampler values currently use the enum values from pinned Tint revision `31e25af254ab572c77054edec4946d2244e184dd`. They must not be interpreted using a different Tint revision's enum table. A future incompatible layout/schema requires a format version change.

## Selection and publication

Paths are `shaders/<sha256>.wgsp`. The digest input is the asset ID, vertex-program name, pixel-program name and variant, each framed by its UTF-8 byte length as little-endian uint32. Authored identifiers never become filenames directly.

Built-ins use the shared StandardShaderVariants and SpriteBatchShaderVariants catalogs. Custom HLSL follows the current shared ShaderPackageBuilder convention: source-file-name `.vs`/`.ps` logical programs, VS/PS entry points and default variant. Other selections fail explicitly; no arbitrary missing entry-point inference is allowed.

All requested pairs are compiled and serialized before publication begins. Each output is written to a unique sibling temporary file and atomically renamed only after preparation succeeds. No declaration is returned for a failed cook. Native inputs and diagnostics live outside the scanned cooked-content tree.

## WGPM v1

Magic WGPM, version uint32 1, total file length uint32, JSON length uint32 form a 16-byte header. Strict UTF-8 JSON follows immediately. The JSON payload limit is 16,777,216 bytes. Reject unknown versions, invalid lengths, malformed UTF-8, duplicate keys and trailing bytes.

The metadata object contains nonempty `materialAssetId`, `shaderAssetId`, `vertexProgram`, `pixelProgram`, `variant`, and `shaderPath` strings. `shaderPath` is `cooked/` plus the exact WGSP selection identity described above. Required booleans `castsShadows` and `receivesShadows` preserve authored intent independently of current renderer capabilities.

`renderState` contains `alphaMode` (`opaque`, `alpha-blend`, or `additive`), and booleans `doubleSided`, `useVertexColor`, `lit`, `depthTest`, `depthWrite`.

`constantBuffers` contains objects with unique nonempty `name` and canonical base64 `data`. Decoded data must be nonempty, a multiple of four bytes and at most 65,536 bytes per buffer. Standard named blocks are `BaseColorBuffer`, `RoughnessBuffer`, `MetallicBuffer`, `SpecularBuffer`, and `EmissiveColorBuffer`. RGBA channels use little-endian float4; scalar values broadcast to all four channels as in the shared engine material utilities. Optional `constant-buffers` input preserves explicitly provided raw named bytes; the editor does not currently project arbitrary custom buffers into that field.

`textures` contains unique binding `name`, authored `assetId`, and canonical content-relative `path`. Standard names are `DiffuseTexture`, `RoughnessTexture`, and `EmissiveTexture`. Absent authored slots are omitted. Imported paths use the existing editor convention `cooked/imported/<assetId>` without an extension. Explicit supplied cooked paths are preserved and traversal is rejected.

The platform schema declares editor defaults and the cooker requires all selected shader/PBR/state fields. It does not manufacture a shader selection when the request omits one. Packaging retains its existing scene material paths (`cooked/<source-relative-path>`, commonly ending in `.hasset`); the WGPM magic/version identifies the payload. Standalone fixtures use `.wgpm`. This preserves the shared packaging path contract.
