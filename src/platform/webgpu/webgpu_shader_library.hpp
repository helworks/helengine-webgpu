#pragma once
#include "webgpu_shader_artifact_reader.hpp"
#include "webgpu_pipeline_target.hpp"
#include <webgpu/webgpu.h>
#include <map>
#include <memory>
namespace helengine::webgpu {
class WebGpuFrame;
/// Retains one compiled pair and its common explicit resource layout.
struct WebGpuShaderProgram {
    WebGpuShaderArtifact Artifact;
    WGPUShaderModule Vertex = nullptr, Fragment = nullptr;
    WGPUBindGroupLayout Layout = nullptr;
    WGPUPipelineLayout PipelineLayout = nullptr;
    std::map<std::string,WGPURenderPipeline> Pipelines;
    ~WebGpuShaderProgram();
};
/// Shares device modules, layouts and complete-state pipelines across scene resources.
class WebGpuShaderLibrary {
public:
    explicit WebGpuShaderLibrary(WebGpuFrame& frame);
    ~WebGpuShaderLibrary();
    std::shared_ptr<WebGpuShaderProgram> Load(const std::string& path);
    std::shared_ptr<WebGpuShaderProgram> Load(const std::string& path, std::span<const std::byte> bytes);
    WGPURenderPipeline Pipeline(WebGpuShaderProgram& program, bool ui, const ArtifactJson& state);
    WGPURenderPipeline Pipeline(WebGpuShaderProgram& program, bool ui, const ArtifactJson& state, const WebGpuPipelineTarget& target);
    WGPUBindGroup CreateBinding(WebGpuShaderProgram& program, const std::map<std::string,WGPUTextureView>& textures);
    void Bind(WGPURenderPassEncoder pass, WebGpuShaderProgram& program, WGPUBindGroup binding, const std::map<std::string,std::vector<std::byte>>& constants);
private:
    WebGpuFrame& Frame;
    std::map<std::string,std::shared_ptr<WebGpuShaderProgram>> Programs;
    WGPUSampler Sampler = nullptr;
    WGPUSampler ComparisonSampler = nullptr;
    WGPUTexture White = nullptr, Black = nullptr, Cube = nullptr;
    WGPUTextureView WhiteView = nullptr, BlackView = nullptr, CubeView = nullptr;
    WGPUTexture InactiveDepth = nullptr, InactiveDepthCube = nullptr;
    WGPUTextureView InactiveDepthView = nullptr, InactiveDepthCubeView = nullptr;
    WGPUTexture CreateSolid(bool cube, bool black);
    WGPUTexture CreateInactiveDepth(bool cube);
};
}
