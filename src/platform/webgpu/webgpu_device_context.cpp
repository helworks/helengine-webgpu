#include "webgpu_device_context.hpp"
#include <cstring>
#include <stdexcept>
namespace helengine::webgpu {
/// Keeps browser callbacks alive independently of the host wrapper.
struct WebGpuDeviceContext::State {
    bool Active = true;
    WGPUInstance Instance = nullptr;
    WGPUSurface Surface = nullptr;
    WGPUAdapter Adapter = nullptr;
    WGPUDevice Device = nullptr;
    WGPUTextureFormat Format = WGPUTextureFormat_Undefined;
    std::function<void()> Ready;
    std::function<void(const std::string&)> Failed;

    /// Releases handles after pending callbacks surrender their state references.
    ~State() {
        if (Device) wgpuDeviceRelease(Device);
        if (Adapter) wgpuAdapterRelease(Adapter);
        if (Surface) wgpuSurfaceRelease(Surface);
        if (Instance) wgpuInstanceRelease(Instance);
    }
    /// Publishes the first terminal error only while the host is active.
    void Fail(const std::string& message) {
        if (!Active) return;
        Active = false;
        Failed("WebGPU: " + message);
    }
    /// Copies a nullable API string before the callback returns.
    static std::string Message(WGPUStringView view) {
        if (!view.data) return "GPU request failed";
        return std::string(view.data, view.length == WGPU_STRLEN ? std::strlen(view.data) : view.length);
    }
    /// Acquires a device only if adapter acquisition still belongs to an active host.
    static void AdapterReady(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void* userdata, void*) {
        std::unique_ptr<std::shared_ptr<State>> holder(static_cast<std::shared_ptr<State>*>(userdata));
        auto state = *holder;
        if (!state->Active) { if (adapter) wgpuAdapterRelease(adapter); return; }
        if (status != WGPURequestAdapterStatus_Success || !adapter) { state->Fail(Message(message)); return; }
        state->Adapter = adapter;
        WGPUDeviceDescriptor descriptor = WGPU_DEVICE_DESCRIPTOR_INIT;
        descriptor.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
        descriptor.deviceLostCallbackInfo.callback = DeviceLost;
        descriptor.deviceLostCallbackInfo.userdata1 = new std::weak_ptr<State>(state);
        WGPURequestDeviceCallbackInfo callback = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
        callback.mode = WGPUCallbackMode_AllowSpontaneous;
        callback.callback = DeviceReady;
        callback.userdata1 = new std::shared_ptr<State>(state);
        wgpuAdapterRequestDevice(adapter, &descriptor, callback);
    }
    /// Configures the surface format and hands control back to the host after acquisition.
    static void DeviceReady(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void* userdata, void*) {
        std::unique_ptr<std::shared_ptr<State>> holder(static_cast<std::shared_ptr<State>*>(userdata));
        auto state = *holder;
        if (!state->Active) { if (device) { wgpuDeviceDestroy(device); wgpuDeviceRelease(device); } return; }
        if (status != WGPURequestDeviceStatus_Success || !device) { state->Fail(Message(message)); return; }
        state->Device = device;
        WGPUSurfaceCapabilities capabilities = WGPU_SURFACE_CAPABILITIES_INIT;
        if (wgpuSurfaceGetCapabilities(state->Surface, state->Adapter, &capabilities) != WGPUStatus_Success || capabilities.formatCount == 0) {
            state->Fail("Canvas has no supported presentation format"); return;
        }
        state->Format = capabilities.formats[0];
        wgpuSurfaceCapabilitiesFreeMembers(capabilities);
        state->Ready();
    }
    /// Retires presentation after loss, including intentional device destruction during tests.
    static void DeviceLost(WGPUDevice const*, WGPUDeviceLostReason, WGPUStringView message, void* userdata, void*) {
        std::unique_ptr<std::weak_ptr<State>> holder(static_cast<std::weak_ptr<State>*>(userdata));
        if (auto state = holder->lock()) state->Fail("Device lost: " + Message(message));
    }
};

void WebGpuDeviceContext::Initialize(const char* selector, std::function<void()> ready, std::function<void(const std::string&)> failed) {
    if (!selector || !ready || !failed) throw std::invalid_argument("Canvas and GPU callbacks are required");
    Shutdown();
    Current = std::make_shared<State>();
    Current->Ready = std::move(ready); Current->Failed = std::move(failed);
    WGPUInstanceDescriptor instance = WGPU_INSTANCE_DESCRIPTOR_INIT;
    Current->Instance = wgpuCreateInstance(&instance);
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvas = WGPU_EMSCRIPTEN_SURFACE_SOURCE_CANVAS_HTML_SELECTOR_INIT;
    canvas.selector = {selector, WGPU_STRLEN};
    WGPUSurfaceDescriptor surface = WGPU_SURFACE_DESCRIPTOR_INIT;
    surface.nextInChain = &canvas.chain;
    Current->Surface = wgpuInstanceCreateSurface(Current->Instance, &surface);
    WGPURequestAdapterOptions options = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    options.compatibleSurface = Current->Surface;
    WGPURequestAdapterCallbackInfo callback = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    callback.mode = WGPUCallbackMode_AllowSpontaneous; callback.callback = State::AdapterReady;
    callback.userdata1 = new std::shared_ptr<State>(Current);
    wgpuInstanceRequestAdapter(Current->Instance, &options, callback);
}
bool WebGpuDeviceContext::Resize(uint32_t width, uint32_t height) {
    if (!Current || !Current->Active || !Current->Device || !width || !height) return false;
    WGPUSurfaceConfiguration configuration = WGPU_SURFACE_CONFIGURATION_INIT;
    configuration.device = Current->Device; configuration.format = Current->Format;
    configuration.usage = WGPUTextureUsage_RenderAttachment;
    configuration.width = width; configuration.height = height;
    configuration.presentMode = WGPUPresentMode_Fifo;
    configuration.alphaMode = WGPUCompositeAlphaMode_Opaque;
    wgpuSurfaceConfigure(Current->Surface, &configuration);
    return true;
}
void WebGpuDeviceContext::Shutdown() {
    if (!Current) return;
    Current->Active = false;
    if (Current->Device) { wgpuSurfaceUnconfigure(Current->Surface); wgpuDeviceDestroy(Current->Device); }
    Current.reset();
}
WGPUDevice WebGpuDeviceContext::GetDevice() const { return Current && Current->Active ? Current->Device : nullptr; }
WGPUTextureFormat WebGpuDeviceContext::GetSurfaceFormat() const { return Current ? Current->Format : WGPUTextureFormat_Undefined; }
WGPUSurface WebGpuDeviceContext::GetSurface() const { return Current && Current->Active ? Current->Surface : nullptr; }
}
