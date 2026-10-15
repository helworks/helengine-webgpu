#pragma once
#include "float3.hpp"
#include "float4.hpp"
#include <cstdint>
class LightComponent;
namespace helengine::webgpu {
/// Snapshots one authored light for deterministic per-camera planning without owning the component.
struct WebGpuLightRecord {
    LightComponent* Source = nullptr;
    int Type = 0;
    float4 Color = float4(1,1,1,1);
    float Intensity = 1, ShadowStrength = 1, Range = 0, InnerConeDegrees = 0, OuterConeDegrees = 0, ShadowDistance = 0;
    float3 Position = float3(0,0,0), Direction = float3(0,0,-1);
    bool Enabled = true, ShadowsEnabled = false;
    uint16_t LayerMask = 1;
};
}
