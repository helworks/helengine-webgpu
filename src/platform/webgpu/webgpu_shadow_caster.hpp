#pragma once
#include "float4x4.hpp"
#include <webgpu/webgpu.h>
#include <cstdint>
namespace helengine::webgpu {
/// Borrows one eligible triangle submesh and its world transform for a camera's light passes.
struct WebGpuShadowCaster {
    WGPUBuffer Vertices = nullptr, Indices = nullptr;
    uint32_t First = 0, Count = 0;
    float4x4 World;
    bool CastsShadows = true;
};
}
