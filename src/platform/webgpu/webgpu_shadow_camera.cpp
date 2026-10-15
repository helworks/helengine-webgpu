#include "webgpu_shadow_plan.hpp"
#include "ICamera.hpp"
#include "Entity.hpp"
#include <stdexcept>
namespace helengine::webgpu {
void WebGpuShadowPlan::Prepare(ICamera* camera,const WebGpuLightState& lights,int32_t maximumShadowedLights) {
    if(!camera || !camera->get_Parent()) throw std::invalid_argument("Shadow planning requires an attached camera");
    PrepareForCamera(camera->get_Parent()->get_Position(),float4::RotateVector(float3(0,0,-1),camera->get_Parent()->get_Orientation()),lights,maximumShadowedLights);
}
}
