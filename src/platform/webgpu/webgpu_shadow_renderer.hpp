#pragma once
#include "webgpu_frame.hpp"
#include "webgpu_shadow_plan.hpp"
#include "webgpu_shadow_caster.hpp"
#include "webgpu_shader_library.hpp"
class ICamera;
namespace helengine::webgpu {
/// Retains native depth resources and executes the original cooked shadow shaders before forward sampling.
class WebGpuShadowRenderer {
public:
    explicit WebGpuShadowRenderer(WebGpuFrame& frame);
    ~WebGpuShadowRenderer();
    void Render(ICamera* camera, const WebGpuShadowPlan& plan);
    void RenderCasters(const std::vector<WebGpuShadowCaster>& casters, const WebGpuShadowPlan& plan);
    std::map<std::string,WGPUTextureView> TextureBindings() const;
    ArtifactJson ShadowValues() const;
    uint64_t ResourceGeneration = 0;
private:
    WebGpuFrame& Frame;
    WebGpuShadowPlan CurrentPlan;
    WGPUTexture Atlas = nullptr;
    WGPUTextureView AtlasView = nullptr;
    std::shared_ptr<WebGpuShaderProgram> DepthProgram;
    WGPUBindGroup DepthBinding = nullptr;
    void AllocateAtlas();
    void DrawCasters(WGPURenderPassEncoder pass, const std::vector<WebGpuShadowCaster>& casters, float4x4 matrix);
};
}
