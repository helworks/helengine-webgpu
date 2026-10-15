#pragma once
#include "webgpu_light_record.hpp"
#include "float4x4.hpp"
#include <array>
namespace helengine::webgpu {
/// Links one forward light slot to its atlas tile or six radial cube passes.
struct WebGpuShadowEntry {
    WebGpuLightRecord Light;
    int LightSlot=0, CubeIndex=-1;
    uint32_t X=0,Y=0,Width=0,Height=0;
    float4x4 Matrix;
    std::array<float4x4,6> FaceMatrices;
};
}
