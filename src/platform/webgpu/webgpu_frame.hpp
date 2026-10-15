#pragma once
#include "webgpu_device_context.hpp"
#include <cstddef>
#include <string>
#include <memory>

namespace helengine::webgpu {
class WebGpuShaderLibrary;
/// Owns reusable frame resources and the command encoder shared by 3D and UI passes.
class WebGpuFrame {
public:
    /// Allocates bounded upload arenas on an acquired device.
    explicit WebGpuFrame(WebGpuDeviceContext& context);
    /// Releases retained GPU resources after the host stops its loop.
    ~WebGpuFrame();
    /// Acquires the current canvas texture and recreates depth only after size changes.
    bool Begin(uint32_t width, uint32_t height);
    /// Submits the completed frame and releases the acquired surface texture.
    void End();
    /// Allocates an aligned uniform slice and copies its value into the reusable GPU arena.
    uint32_t Uniform(const void* data, size_t size);
    /// Shares versioned shader modules and complete-state pipelines across both renderers.
    std::unique_ptr<WebGpuShaderLibrary> Shaders;
    /// Creates an initialized GPU buffer with WebGPU-aligned storage size.
    WGPUBuffer Buffer(const void* data, size_t size, WGPUBufferUsage usage);
    /// Returns the shared device.
    WGPUDevice Device() const { return Context.GetDevice(); }
    /// Returns the configured presentation format.
    WGPUTextureFormat Format() const { return Context.GetSurfaceFormat(); }
    /// Returns the frame width.
    uint32_t Width() const { return CanvasWidth; }
    /// Returns the frame height.
    uint32_t Height() const { return CanvasHeight; }
    /// Stores the command encoder borrowed by active render passes.
    WGPUCommandEncoder Encoder = nullptr;
    /// Stores the current color attachment view.
    WGPUTextureView Color = nullptr;
    /// Stores the persistent size-matched depth view.
    WGPUTextureView Depth = nullptr;
    /// Stores the reusable dynamic uniform buffer.
    WGPUBuffer Uniforms = nullptr;
    uint64_t UniformGeneration = 1;
    /// Stores the device's ordered submission queue.
    WGPUQueue Queue = nullptr;
private:
    /// Borrows the device owner for the frame lifetime.
    WebGpuDeviceContext& Context;
    /// Holds the current canvas texture reference.
    WGPUTexture SurfaceTexture = nullptr;
    /// Holds the size-matched depth texture.
    WGPUTexture DepthTexture = nullptr;
    /// Records the current canvas extent.
    uint32_t CanvasWidth = 0, CanvasHeight = 0;
    /// Advances through the frame's aligned uniform storage.
    uint32_t UniformCursor = 0;
    uint64_t UniformCapacity = 4 * 1024 * 1024;
};
}
