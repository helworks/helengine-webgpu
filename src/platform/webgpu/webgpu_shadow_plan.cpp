#include "webgpu_shadow_plan.hpp"
#include "RealMath.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace helengine::webgpu {
namespace {
float3 Normalize(float3 value) {
    auto length=RealMath::Sqrt(value.X*value.X+value.Y*value.Y+value.Z*value.Z);
    return length>0.0000001?float3(value.X/length,value.Y/length,value.Z/length):float3(0,0,-1);
}
float4x4 Projection(float3 position,float3 direction,float3 up,float4x4 projection) {
    float4x4 view,result; auto target=position+direction; float4x4::CreateLookAt__ref0_ref1_ref2_out3(position,target,up,view); float4x4::Multiply__ref0_ref1_out2(view,projection,result); return result;
}
}
void WebGpuShadowPlan::PrepareForCamera(float3 cameraPosition,float3 cameraForward,const WebGpuLightState& lights,int32_t maximumShadowedLights) {
    if(maximumShadowedLights<0) throw std::invalid_argument("Maximum shadow count cannot be negative");
    Entries.clear();
    for(size_t index=0;index<lights.Slots.size() && Entries.size()<static_cast<size_t>(maximumShadowedLights);++index) {
        if(!lights.Slots[index].ShadowsEnabled) continue;
        WebGpuShadowEntry entry; entry.Light=lights.Slots[index]; entry.LightSlot=index; Entries.push_back(entry);
    }
    int atlasCount=std::count_if(Entries.begin(),Entries.end(),[](auto& entry){return entry.Light.Type!=1;});
    uint32_t grid=atlasCount?static_cast<uint32_t>(std::ceil(std::sqrt(atlasCount))):1, tile=AtlasSize/grid, atlasIndex=0; int cubeIndex=0;
    const float3 faces[6]={float3(1,0,0),float3(-1,0,0),float3(0,1,0),float3(0,-1,0),float3(0,0,1),float3(0,0,-1)};
    const float3 faceUps[6]={float3(0,-1,0),float3(0,-1,0),float3(0,0,1),float3(0,0,-1),float3(0,1,0),float3(0,1,0)};
    for(auto& entry:Entries) {
        auto& light=entry.Light; float4x4 projection;
        if(light.Type==1) {
            if(light.Range<=0.1f) throw std::runtime_error("Point shadow range must exceed its 0.1 near plane");
            entry.CubeIndex=cubeIndex++; entry.Width=entry.Height=CubeSize;
            float4x4::CreatePerspectiveFieldOfView__out4(3.141592653589793/2,1,0.1f,light.Range,projection);
            for(int face=0;face<6;++face) {
                auto orientedProjection=projection;
                // Convert the desktop RH face bases to WebGPU's native cube sampling coordinates.
                if(face<4) orientedProjection.M22=-orientedProjection.M22;
                else orientedProjection.M11=-orientedProjection.M11;
                entry.FaceMatrices[face]=Projection(light.Position,faces[face],faceUps[face],orientedProjection);
            }
        } else {
            entry.X=(atlasIndex%grid)*tile; entry.Y=(atlasIndex/grid)*tile; entry.Width=entry.Height=tile; ++atlasIndex;
            auto direction=Normalize(light.Direction); float3 position=light.Position;
            if(light.Type==0) {
                float distance=std::max(1.0f,light.ShadowDistance); auto target=cameraPosition+cameraForward*(distance*0.5f); position=target-direction*distance;
                float half=distance*0.5f; float4x4::CreateOrthographicOffCenter__out6(-half,half,-half,half,0.1f,distance*2,projection);
            } else if(light.Type==2) {
                double field=std::min(light.OuterConeDegrees*3.141592653589793/90,3.141592653589793-0.0001); float range=std::max(0.1f,light.Range);
                float4x4::CreatePerspectiveFieldOfView__out4(field,1,0.1f,range+std::max(2.0f,range*0.15f),projection);
            } else throw std::runtime_error("Unsupported shadow light family");
            float3 up=RealMath::Abs(direction.Y)>0.99f?float3(0,0,1):float3(0,1,0); entry.Matrix=Projection(position,direction,up,projection);
        }
    }
}
}
