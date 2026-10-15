#pragma once
#include "webgpu_shadow_entry.hpp"
#include "webgpu_light_state.hpp"
class ICamera;
namespace helengine::webgpu {
/// Mirrors desktop shadow budgets, atlas packing and light-space projections for one camera.
class WebGpuShadowPlan {
public:
    void Prepare(ICamera* camera, const WebGpuLightState& lights, int32_t maximumShadowedLights);
    void PrepareForCamera(float3 cameraPosition, float3 cameraForward, const WebGpuLightState& lights, int32_t maximumShadowedLights);
    std::vector<WebGpuShadowEntry> Entries;
    static constexpr uint32_t AtlasSize=2048, CubeSize=512;
};
}
