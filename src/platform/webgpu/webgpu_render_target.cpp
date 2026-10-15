#include "webgpu_render_target.hpp"
#include <stdexcept>
namespace helengine::webgpu {
WebGpuRenderTarget::WebGpuRenderTarget(WebGpuFrame& frame,int32_t width,int32_t height) {
    if(width<=0 || height<=0) throw std::invalid_argument("Camera render target dimensions must be positive");
    set_Width(width);set_Height(height);set_CanSampleAsTexture(true);set_HasDepthBuffer(true);
    WGPUTextureDescriptor descriptor=WGPU_TEXTURE_DESCRIPTOR_INIT;descriptor.size={static_cast<uint32_t>(width),static_cast<uint32_t>(height),1};descriptor.format=Format;
    descriptor.usage=WGPUTextureUsage_RenderAttachment|WGPUTextureUsage_TextureBinding|WGPUTextureUsage_CopySrc|WGPUTextureUsage_CopyDst;
    Texture=wgpuDeviceCreateTexture(frame.Device(),&descriptor);View=wgpuTextureCreateView(Texture,nullptr);
    descriptor.format=WGPUTextureFormat_Depth24Plus;descriptor.usage=WGPUTextureUsage_RenderAttachment;
    DepthTexture=wgpuDeviceCreateTexture(frame.Device(),&descriptor);DepthView=wgpuTextureCreateView(DepthTexture,nullptr);
}
WebGpuRenderTarget::~WebGpuRenderTarget(){Dispose();}
void WebGpuRenderTarget::Dispose() {
    if(Binding){wgpuBindGroupRelease(Binding);Binding=nullptr;}
    if(View){wgpuTextureViewRelease(View);View=nullptr;}if(DepthView){wgpuTextureViewRelease(DepthView);DepthView=nullptr;}
    if(Texture){wgpuTextureRelease(Texture);Texture=nullptr;}if(DepthTexture){wgpuTextureRelease(DepthTexture);DepthTexture=nullptr;}
    RuntimeTexture::Dispose();
}
}
