#pragma once
#include <webgpu/webgpu.h>
#include <cstdint>
namespace helengine::webgpu {
/// Describes attachment compatibility and raster depth bias for a cached pipeline.
struct WebGpuPipelineTarget {
    WGPUTextureFormat ColorFormat = WGPUTextureFormat_Undefined;
    WGPUTextureFormat DepthFormat = WGPUTextureFormat_Undefined;
    bool HasColor = true;
    int32_t DepthBias = 0;
    float DepthBiasSlopeScale = 0;
};
}
