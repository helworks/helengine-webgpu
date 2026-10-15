#pragma once
#include "RuntimeMaterial.hpp"
#include "float4.hpp"
#include "webgpu_material_artifact_reader.hpp"
#include "webgpu_shader_library.hpp"
#include "webgpu_runtime_texture.hpp"

namespace helengine::webgpu {
/// Owns authored WGPM data, its retained shader pair and explicit texture resources.
class WebGpuRuntimeMaterial : public RuntimeMaterial {
public:
    ~WebGpuRuntimeMaterial() override { if (Binding) wgpuBindGroupRelease(Binding); }
    WebGpuMaterialArtifact Artifact;
    std::shared_ptr<WebGpuShaderProgram> Program;
    std::vector<std::unique_ptr<WebGpuRuntimeTexture>> Textures;
    WGPUBindGroup Binding = nullptr;
};
}
