#pragma once
#include "RenderTarget.hpp"
#include "webgpu_frame.hpp"
namespace helengine::webgpu {
/// Owns a sampleable camera color target and its optional depth attachment through normal runtime disposal.
class WebGpuRenderTarget:public RenderTarget {
public:
    WebGpuRenderTarget(WebGpuFrame& frame,int32_t width,int32_t height);
    ~WebGpuRenderTarget() override;
    void Dispose() override;
    WGPUTexture Texture=nullptr,DepthTexture=nullptr;
    WGPUTextureView View=nullptr,DepthView=nullptr;
    WGPUBindGroup Binding=nullptr;
    uint64_t UniformGeneration=0;
    WGPUTextureFormat Format=WGPUTextureFormat_RGBA8Unorm;
};
}
