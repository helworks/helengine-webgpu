#include "platform/webgpu/webgpu_light_state.hpp"
#include "platform/webgpu/webgpu_shadow_plan.hpp"
#include <emscripten.h>
#include <cmath>
#include <stdexcept>
#include <iostream>
using namespace helengine::webgpu;
void Require(bool value) { if (!value) throw std::runtime_error("Light/shadow planning assertion failed"); }
int main() {
    try {
        WebGpuLightRecord ambient{}; ambient.Type=3; ambient.Color=float4(0.2f,0.4f,0.6f,1); ambient.Intensity=2;
        WebGpuLightRecord directional{}; directional.Direction=float3(0,0,-1); directional.ShadowsEnabled=true; directional.ShadowDistance=30;
        WebGpuLightRecord point{}; point.Type=1; point.Position=float3(1,2,3); point.Range=20; point.ShadowsEnabled=true;
        WebGpuLightRecord spot{}; spot.Type=2; spot.Position=float3(0,4,0); spot.Direction=float3(0,-1,0); spot.Range=15; spot.InnerConeDegrees=20; spot.OuterConeDegrees=40; spot.ShadowsEnabled=true;
        WebGpuLightState lights; lights.PrepareRecords({ambient,directional,point,spot},4,1);
        auto values=lights.ForwardValues(); Require(values.at("lightMetadata")[0]==3);
        Require(std::abs(values.at("ambientLightColor")[0].get<float>()-0.4f)<0.00001f);
        Require(values.at("light1PositionAndRange")[3]==20); Require(values.at("light2ColorAndType")[3]==2);
        Require(std::abs(values.at("light2SpotAngles")[0].get<float>()-std::cos(20*3.141592653589793/180))<0.00001);
        WebGpuShadowPlan plan; plan.PrepareForCamera(float3(0,0,0),float3(0,0,-1),lights,4);
        Require(plan.Entries.size()==3); Require(plan.Entries[0].Width==1024 && plan.Entries[2].X==1024);
        Require(plan.Entries[1].CubeIndex==0); Require(plan.Entries[1].LightSlot==1);
        for(int face=0;face<6;++face) for(int element=0;element<16;++element) Require(std::isfinite(reinterpret_cast<const float*>(&plan.Entries[1].FaceMatrices[face])[element]));
        const float directions[6][3]={{1,0.22f,0.3f},{-1,0.22f,0.3f},{0.3f,1,0.22f},{0.3f,-1,0.22f},{0.3f,0.22f,1},{0.3f,0.22f,-1}};
        for(int face=0;face<6;++face) {
            auto& matrix=plan.Entries[1].FaceMatrices[face]; const auto& direction=directions[face];
            float x=point.Position.X+direction[0]*2,y=point.Position.Y+direction[1]*2,z=point.Position.Z+direction[2]*2;
            float clipX=x*matrix.M11+y*matrix.M21+z*matrix.M31+matrix.M41,clipY=x*matrix.M12+y*matrix.M22+z*matrix.M32+matrix.M42,clipW=x*matrix.M14+y*matrix.M24+z*matrix.M34+matrix.M44;
            float expectedU=0.5f+(face==0?-direction[2]:face==1?direction[2]:face==2 || face==3 || face==4?direction[0]:-direction[0])*0.5f;
            float expectedV=0.5f+(face==2?direction[2]:face==3?-direction[2]:-direction[1])*0.5f;
            float u=0.5f+clipX/clipW*0.5f,v=0.5f-clipY/clipW*0.5f;
            if(std::abs(u-expectedU)>0.0001f || std::abs(v-expectedV)>0.0001f) std::cout<<"cube face "<<face<<" projected UV="<<u<<","<<v<<" expected="<<expectedU<<","<<expectedV<<'\n';
            Require(std::abs(u-expectedU)<0.0001f && std::abs(v-expectedV)<0.0001f);
        }
        lights.PrepareRecords({point,spot},4,1); plan.PrepareForCamera(float3(0,0,0),float3(0,0,-1),lights,4);
        Require(plan.Entries[0].LightSlot==0 && plan.Entries[0].CubeIndex==0);
        point.Enabled=false; spot.LayerMask=2; lights.PrepareRecords({point,spot},4,1); Require(lights.Slots.empty());
        values=lights.ForwardValues(); Require(values.at("lightMetadata")[0]==0 && values.at("light0ColorAndType")[0]==0 && values.at("ambientLightColor")[0]==0);
        plan.PrepareForCamera(float3(0,0,0),float3(0,0,-1),lights,0); Require(plan.Entries.empty());
        point.Enabled=true; directional.Intensity=3; point.Intensity=2; spot.LayerMask=1;
        lights.PrepareRecords({point,spot,directional,directional,point},4,1); Require(lights.Slots.size()==4 && lights.Slots[0].Intensity==3);
        EM_ASM({if(typeof window!=='undefined')window['lightStateChecks']='passed';}); std::cout<<"Light/shadow planning checks passed\n"; return 0;
    } catch(const std::exception& error) { EM_ASM({if(typeof window!=='undefined')window['lightStateChecks']=UTF8ToString($0);},error.what()); std::cerr<<error.what()<<'\n'; return 1; }
}
