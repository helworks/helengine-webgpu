#include "webgpu_light_state.hpp"
#include "Core.hpp"
#include "ObjectManager.hpp"
#include "Entity.hpp"
#include "ICamera.hpp"
#include "LightComponent.hpp"
#include "DirectionalLightComponent.hpp"
#include "PointLightComponent.hpp"
#include "SpotLightComponent.hpp"
#include "LightDirectionUtility.hpp"
#include <stdexcept>
namespace helengine::webgpu {
void WebGpuLightState::Prepare(Core* core, ICamera* camera, int32_t maximumVisibleLights) {
    if(!core || !camera) throw std::invalid_argument("Light preparation requires the owning core and camera");
    std::vector<WebGpuLightRecord> records; auto* entities=core->get_ObjectManager()->get_Entities();
    for(int32_t entityIndex=0;entityIndex<entities->Count();++entityIndex) {
        auto* entity=(*entities)[entityIndex]; if(!entity || !entity->get_IsHierarchyEnabled() || !(entity->get_LayerMask()&camera->get_LayerMask())) continue;
        auto* components=entity->get_Components();
        for(int32_t index=0;index<components->Count();++index) {
            auto* light=dynamic_cast<LightComponent*>((*components)[index]); if(!light) continue;
            WebGpuLightRecord record; record.Source=light; record.Type=static_cast<int>(light->get_LightType()); record.Color=light->get_Color(); record.Intensity=light->get_Intensity();
            record.ShadowsEnabled=light->get_ShadowsEnabled(); record.ShadowStrength=light->get_ShadowStrength(); record.Position=entity->get_Position(); record.Direction=LightDirectionUtility::GetEntityForwardDirection(entity); record.LayerMask=entity->get_LayerMask();
            if(auto* point=dynamic_cast<PointLightComponent*>(light)) record.Range=point->get_Range();
            if(auto* spot=dynamic_cast<SpotLightComponent*>(light)) { record.Range=spot->get_Range(); record.InnerConeDegrees=spot->get_InnerConeAngleDegrees(); record.OuterConeDegrees=spot->get_OuterConeAngleDegrees(); }
            if(auto* directional=dynamic_cast<DirectionalLightComponent*>(light)) record.ShadowDistance=directional->get_ShadowDistance();
            records.push_back(record);
        }
    }
    PrepareRecords(records,maximumVisibleLights,camera->get_LayerMask());
}
}
