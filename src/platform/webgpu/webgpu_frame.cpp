#include "webgpu_frame.hpp"
#include "webgpu_shader_library.hpp"
#include <fstream>
#include <iterator>
#include <stdexcept>

namespace helengine::webgpu {
WebGpuFrame::WebGpuFrame(WebGpuDeviceContext& context) : Context(context) {
    Queue = wgpuDeviceGetQueue(Device());
    WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    descriptor.size = 1024 * 1024; descriptor.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    Uniforms = wgpuDeviceCreateBuffer(Device(), &descriptor);
    Shaders = std::make_unique<WebGpuShaderLibrary>(*this);
}
WebGpuFrame::~WebGpuFrame() {
    Shaders.reset();
    if (Color) wgpuTextureViewRelease(Color);
    if (SurfaceTexture) wgpuTextureRelease(SurfaceTexture);
    if (Encoder) wgpuCommandEncoderRelease(Encoder);
    if (Depth) wgpuTextureViewRelease(Depth);
    if (DepthTexture) wgpuTextureRelease(DepthTexture);
    if (Uniforms) wgpuBufferRelease(Uniforms);
    if (Queue) wgpuQueueRelease(Queue);
}
bool WebGpuFrame::Begin(uint32_t width, uint32_t height) {
    if (!width || !height || !Device()) return false;
    if (width != CanvasWidth || height != CanvasHeight) {
        if (!Context.Resize(width, height)) return false;
        if (Depth) wgpuTextureViewRelease(Depth);
        if (DepthTexture) wgpuTextureRelease(DepthTexture);
        WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT;
        descriptor.size = {width, height, 1}; descriptor.format = WGPUTextureFormat_Depth24Plus;
        descriptor.usage = WGPUTextureUsage_RenderAttachment;
        DepthTexture = wgpuDeviceCreateTexture(Device(), &descriptor);
        Depth = wgpuTextureCreateView(DepthTexture, nullptr);
        CanvasWidth = width; CanvasHeight = height;
    }
    WGPUSurfaceTexture texture = WGPU_SURFACE_TEXTURE_INIT;
    wgpuSurfaceGetCurrentTexture(Context.GetSurface(), &texture);
    if (!texture.texture) throw std::runtime_error("WebGPU could not acquire a canvas texture");
    SurfaceTexture = texture.texture;
    Color = wgpuTextureCreateView(SurfaceTexture, nullptr);
    WGPUCommandEncoderDescriptor descriptor = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    Encoder = wgpuDeviceCreateCommandEncoder(Device(), &descriptor);
    UniformCursor = 0;
    return true;
}
void WebGpuFrame::End() {
    WGPUCommandBufferDescriptor descriptor = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
    WGPUCommandBuffer commands = wgpuCommandEncoderFinish(Encoder, &descriptor);
    wgpuQueueSubmit(Queue, 1, &commands); wgpuCommandBufferRelease(commands);
    wgpuCommandEncoderRelease(Encoder); Encoder = nullptr;
    wgpuTextureViewRelease(Color); Color = nullptr;
    wgpuTextureRelease(SurfaceTexture); SurfaceTexture = nullptr;
}
uint32_t WebGpuFrame::Uniform(const void* data, size_t size) {
    uint32_t offset = UniformCursor;
    UniformCursor += static_cast<uint32_t>((size + 255) & ~size_t(255));
    if (UniformCursor > 1024 * 1024) throw std::runtime_error("WebGPU frame uniform budget exceeded");
    wgpuQueueWriteBuffer(Queue, Uniforms, offset, data, size);
    return offset;
}
WGPUBuffer WebGpuFrame::Buffer(const void* data, size_t size, WGPUBufferUsage usage) {
    if (!size) throw std::invalid_argument("GPU buffers require nonempty data");
    WGPUBufferDescriptor descriptor = WGPU_BUFFER_DESCRIPTOR_INIT;
    descriptor.size = (size + 3) & ~size_t(3); descriptor.usage = usage | WGPUBufferUsage_CopyDst;
    WGPUBuffer buffer = wgpuDeviceCreateBuffer(Device(), &descriptor);
    if (data) {
        if (size % 4) { wgpuBufferRelease(buffer); throw std::invalid_argument("GPU upload length must be a multiple of four bytes"); }
        wgpuQueueWriteBuffer(Queue, buffer, 0, data, size);
    }
    return buffer;
}
}
