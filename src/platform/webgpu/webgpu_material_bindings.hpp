#pragma once
#include "webgpu_shader_artifact_reader.hpp"
namespace helengine::webgpu {
/// Converts original compiled buffer storage into final WGSL layout, including matrix/array strides.
std::vector<std::byte> PackMaterialBuffer(const ArtifactJson& binding, std::span<const std::byte> original);
/// Writes an engine row-vector float4x4 into the shader's original physical member layout.
void WriteEngineMatrix(const ArtifactJson& sourceMember, const float* engineRows, std::span<std::byte> original);
}
