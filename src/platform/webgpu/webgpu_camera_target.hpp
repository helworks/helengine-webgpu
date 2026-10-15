#pragma once
#include "webgpu_pipeline_target.hpp"
#include <cstdint>
class ICamera;
class RenderTarget;
namespace helengine::webgpu {
class WebGpuFrame;
/// Borrows the attachment views, dimensions and compatible pipeline formats of one camera output.
struct WebGpuCameraTarget {
    WGPUTextureView Color=nullptr,Depth=nullptr;
    uint32_t Width=0,Height=0;
    WebGpuPipelineTarget Pipeline;
};
WebGpuCameraTarget ResolveRenderTarget(WebGpuFrame& frame,RenderTarget* target);
WebGpuCameraTarget ResolveCameraTarget(WebGpuFrame& frame,ICamera* camera);
}
