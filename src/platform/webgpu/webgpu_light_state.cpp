#include "webgpu_light_state.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace helengine::webgpu {
void WebGpuLightState::PrepareRecords(const std::vector<WebGpuLightRecord>& records, int32_t maximumVisibleLights, uint16_t cameraMask) {
    if (maximumVisibleLights < 0) throw std::invalid_argument("Maximum visible-light count cannot be negative");
    Slots.clear(); Ambient=float4(0,0,0,0); std::vector<WebGpuLightRecord> selected;
    for (auto& record:records) if (record.Enabled && (record.LayerMask&cameraMask)) selected.push_back(record);
    std::stable_sort(selected.begin(),selected.end(),[](auto& left,auto& right) {
        return (left.ShadowsEnabled?1000:0)+std::round(left.Intensity*100.0) > (right.ShadowsEnabled?1000:0)+std::round(right.Intensity*100.0);
    });
    if (selected.size()>static_cast<size_t>(maximumVisibleLights)) selected.resize(maximumVisibleLights);
    for (auto& light:selected) {
        if(light.Type==3) { Ambient.X+=light.Color.X*light.Intensity; Ambient.Y+=light.Color.Y*light.Intensity; Ambient.Z+=light.Color.Z*light.Intensity; }
        else if (Slots.size()<4) Slots.push_back(light);
    }
}
ArtifactJson WebGpuLightState::ForwardValues() const {
    ArtifactJson values={{"ambientLightColor",{Ambient.X,Ambient.Y,Ambient.Z,0}},{"lightMetadata",{Slots.size(),0,0,0}}};
    for(size_t index=0;index<4;++index) {
        std::string prefix="light"+std::to_string(index);
        values[prefix+"ColorAndType"]={0,0,0,0}; values[prefix+"DirectionAndShadow"]={0,0,0,0}; values[prefix+"PositionAndRange"]={0,0,0,0}; values[prefix+"SpotAngles"]={0,0,0,0};
        if(index>=Slots.size()) continue;
        auto& light=Slots[index]; values[prefix+"ColorAndType"]={light.Color.X*light.Intensity,light.Color.Y*light.Intensity,light.Color.Z*light.Intensity,light.Type};
        values[prefix+"DirectionAndShadow"]={light.Direction.X,light.Direction.Y,light.Direction.Z,light.ShadowStrength};
        if(light.Type==1 || light.Type==2) values[prefix+"PositionAndRange"]={light.Position.X,light.Position.Y,light.Position.Z,light.Range};
        if(light.Type==2) values[prefix+"SpotAngles"]={std::cos(light.InnerConeDegrees*3.141592653589793/180),std::cos(light.OuterConeDegrees*3.141592653589793/180),0,0};
    }
    return values;
}
}
