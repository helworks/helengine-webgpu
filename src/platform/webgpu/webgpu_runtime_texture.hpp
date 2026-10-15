#pragma once
#include "RuntimeTexture.hpp"
#include <webgpu/webgpu.h>
namespace helengine::webgpu {
/// Owns one uploaded RGBA texture and its reusable UI binding.
class WebGpuRuntimeTexture : public RuntimeTexture {
public:
    /// Releases retained WebGPU handles when the runtime wrapper is destroyed.
    ~WebGpuRuntimeTexture() override { if (Binding) wgpuBindGroupRelease(Binding); if (View) wgpuTextureViewRelease(View); if (Texture) wgpuTextureRelease(Texture); }
    /// Holds the uploaded texture storage.
    WGPUTexture Texture = nullptr;
    /// Holds the sampling view.
    WGPUTextureView View = nullptr;
    /// Holds the reusable texture/sampler/uniform binding.
    WGPUBindGroup Binding = nullptr;
    uint64_t UniformGeneration = 0;
};
}
