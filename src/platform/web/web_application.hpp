#pragma once
#include "platform/webgpu/webgpu_device_context.hpp"
#include "web_frame_clock.hpp"
#include <emscripten/html5.h>
class Core;
namespace helengine::webgpu {
class WebGpuFrame;
class WebGpuRenderManager3D;
class WebGpuRenderManager2D;
class WebInputBackend;
class WebAudioBackend;
/// Boots the generated engine only after content preload and asynchronous GPU acquisition.
class WebApplication {
public:
    /// Requests WebGPU and returns control to the browser event loop.
    void Start();
    /// Advances the real engine and renders its scene for one browser frame.
    void Frame(double timestampMilliseconds);
    /// Stops callbacks and disposes the core before releasing the GPU.
    void Stop();
private:
    /// Owns the acquired WebGPU device and surface.
    WebGpuDeviceContext Device;
    /// Owns timestamp conversion across visibility changes.
    WebFrameClock Clock;
    /// Retains the initialized engine and its borrowed backend pointers.
    Core* Engine = nullptr;
    WebGpuFrame* Graphics = nullptr;
    WebGpuRenderManager3D* Render3D = nullptr;
    WebGpuRenderManager2D* Render2D = nullptr;
    WebInputBackend* Input = nullptr;
    /// Retains the browser audio bridge while the engine owns voice requests.
    WebAudioBackend* Audio = nullptr;
    /// Controls animation callback lifetime.
    bool Running = false;
    /// Distinguishes a completed core initialization from a partially failed boot.
    bool CoreInitialized = false;
    /// Records completed frames for runtime diagnostics.
    uint32_t Frames = 0;
    /// Tracks density independently of CSS resize events, which need not fire on monitor/emulation changes.
    double Density = 0;
    /// Constructs catalogs, backends and module registration before loading the startup scene.
    void Boot();
    /// Reports a terminal error in the page and browser console.
    void Fail(const std::string& message);
    /// Forwards one animation callback to the owning application.
    static EM_BOOL Animate(double timestamp, void* userdata);
    /// Resets timing even when hidden tabs stop receiving animation callbacks entirely.
    static EM_BOOL Visibility(int type, const EmscriptenVisibilityChangeEvent* event, void* userdata);
};
}
