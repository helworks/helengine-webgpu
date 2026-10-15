#include "webgpu_camera_target.hpp"
#include "webgpu_render_target.hpp"
#include "ICamera.hpp"
#include <stdexcept>
namespace helengine::webgpu {
WebGpuCameraTarget ResolveRenderTarget(WebGpuFrame& frame,RenderTarget* runtime) {
    if(!runtime) return {frame.Color,frame.Depth,frame.Width(),frame.Height(),{frame.Format(),WGPUTextureFormat_Depth24Plus,true,0,0}};
    auto* target=dynamic_cast<WebGpuRenderTarget*>(runtime);
    if(!target || target->get_IsDisposed() || !target->View || target->get_Width()<=0 || target->get_Height()<=0) throw std::runtime_error("Camera references a disposed or foreign WebGPU render target");
    bool depth=target->get_HasDepthBuffer();
    return {target->View,depth?target->DepthView:nullptr,static_cast<uint32_t>(target->get_Width()),static_cast<uint32_t>(target->get_Height()),{target->Format,depth?WGPUTextureFormat_Depth24Plus:WGPUTextureFormat_Undefined,true,0,0}};
}
WebGpuCameraTarget ResolveCameraTarget(WebGpuFrame& frame,ICamera* camera) {
    if(!camera) throw std::invalid_argument("Camera output resolution requires a camera");
    return ResolveRenderTarget(frame,camera->get_RenderTarget());
}
}
