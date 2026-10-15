#pragma once
#include "webgpu_shader_artifact_reader.hpp"
#include <map>
namespace helengine::webgpu {
/// Owns checked material metadata and decoded original named constant bytes.
struct WebGpuMaterialArtifact { ArtifactJson Metadata; std::map<std::string,std::vector<std::byte>> Constants; };
/// Verifies WGPM v1 before creating material or device resources.
WebGpuMaterialArtifact ReadMaterialArtifact(std::span<const std::byte> bytes);
}
