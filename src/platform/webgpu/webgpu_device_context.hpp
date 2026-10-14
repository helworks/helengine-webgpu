#pragma once
#include <webgpu/webgpu.h>
#include <functional>
#include <memory>
#include <string>

namespace helengine::webgpu {
/// Owns asynchronous browser GPU acquisition and the canvas presentation surface.
class WebGpuDeviceContext {
public:
    /// Requests the browser adapter/device and reports completion without blocking the event loop.
    void Initialize(const char* canvasSelector, std::function<void()> ready, std::function<void(const std::string&)> failed);
    /// Reconfigures a nonempty surface, returning false when drawing must be suspended.
    bool Resize(uint32_t width, uint32_t height);
    /// Retires callbacks and releases owned GPU resources.
    void Shutdown();
    /// Returns the live GPU device, or null before acquisition/after shutdown.
    WGPUDevice GetDevice() const;
    /// Returns the configured surface format.
    WGPUTextureFormat GetSurfaceFormat() const;
    /// Returns the browser canvas surface.
    WGPUSurface GetSurface() const;
private:
    /// Retains callback state independently of this wrapper's lifetime.
    struct State;
    /// Owns the state observed by asynchronous callbacks.
    std::shared_ptr<State> Current;
};
}
