#pragma once
#include "webgpu_light_record.hpp"
#include "webgpu_shader_artifact_reader.hpp"
#include <vector>
class Core;
class ICamera;
namespace helengine::webgpu {
/// Retains one camera's selected lights and packs the original four-slot forward contract.
class WebGpuLightState {
public:
    // Selection includes ambient lights; only nonambient lights consume the four shader slots.
    static constexpr int32_t MaximumVisibleLights = 32;
    void Prepare(Core* core, ICamera* camera, int32_t maximumVisibleLights);
    void PrepareRecords(const std::vector<WebGpuLightRecord>& records, int32_t maximumVisibleLights, uint16_t cameraMask);
    ArtifactJson ForwardValues() const;
    std::vector<WebGpuLightRecord> Slots;
private:
    float4 Ambient = float4(0,0,0,0);
};
}
