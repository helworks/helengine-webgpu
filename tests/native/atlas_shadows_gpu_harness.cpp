#include "platform/webgpu/webgpu_shadow_renderer.hpp"
#include "platform/webgpu/webgpu_material_bindings.hpp"
#include <emscripten.h>
#include <cstring>
#include <memory>
#include <stdexcept>
using namespace helengine::webgpu;
namespace {
WebGpuDeviceContext Context;
std::unique_ptr<WebGpuFrame> Frame;
std::unique_ptr<WebGpuShadowRenderer> Shadows;
std::shared_ptr<WebGpuShaderProgram> Forward;
WebGpuLightState Lights;
WebGpuShadowPlan Plan;
WebGpuShadowCaster Caster;
WGPUBuffer Receiver = nullptr;
WGPUBindGroup Binding = nullptr;
float4x4 ViewProjection;
float3 CameraPosition(0,0,3);
int Mode = 0;
std::vector<WebGpuLightRecord> LiveRecords;
bool AppliedLightChange = false;
ArtifactJson State={{"alphaMode","opaque"},{"doubleSided",true},{"useVertexColor",false},{"depthTest",true},{"depthWrite",true}};
void Fail(const std::string& message) { EM_ASM({window['atlasShadowError']=UTF8ToString($0);},message.c_str()); emscripten_cancel_main_loop(); }
void Float(const ArtifactJson& member,const ArtifactJson& value,std::vector<std::byte>& bytes) {
    float data[4]={value[0].get<float>(),value[1].get<float>(),value[2].get<float>(),value[3].get<float>()}; std::memcpy(bytes.data()+member.at("offset").get<size_t>(),data,16);
}
std::map<std::string,std::vector<std::byte>> Constants() {
    std::map<std::string,std::vector<std::byte>> values; auto lightValues=Lights.ForwardValues(); auto shadowValues=Shadows->ShadowValues(); auto identity=float4x4::get_Identity();
    for(auto& binding:Forward->Artifact.Metadata.at("bindings")) {
        if(binding.at("class")!="constant-buffer") continue;
        std::string name=binding.at("name"); auto& bytes=values[name]; bytes.assign(binding.at("sourceLayout").at("size").get<size_t>(),std::byte{});
        for(auto& member:binding.at("sourceLayout").at("members")) {
            std::string field=member.at("name");
            if(name=="TransformBuffer") {
                if(field=="world" || field=="normalMatrix") WriteEngineMatrix(member,reinterpret_cast<const float*>(&identity),bytes);
                else if(field=="worldViewProj") WriteEngineMatrix(member,reinterpret_cast<const float*>(&ViewProjection),bytes);
                else if(field=="cameraPosition") Float(member,{CameraPosition.X,CameraPosition.Y,CameraPosition.Z,1},bytes);
                else if(field=="materialFlags") Float(member,{Mode==2?0:1,0,0,0},bytes);
                else throw std::runtime_error("Unexpected transform fixture field: "+field);
            } else if(name=="ForwardLightBuffer") Float(member,lightValues.at(field),bytes);
            else if(name=="ShadowBuffer") {
                auto& value=shadowValues.at(field); if(value.size()==16) {auto matrix=value.get<std::vector<float>>(); WriteEngineMatrix(member,matrix.data(),bytes);} else Float(member,value,bytes);
            } else if(name=="BaseColorBuffer") Float(member,{1,1,1,1},bytes);
            else if(name=="RoughnessBuffer" || name=="SpecularBuffer") Float(member,{0.5f,0.5f,0.5f,0.5f},bytes);
            else if(name=="MetallicBuffer" || name=="EmissiveColorBuffer") Float(member,{0,0,0,0},bytes);
            else throw std::runtime_error("Unexpected receiver fixture buffer: "+name);
        }
    }
    return values;
}
void Draw() {
    try {
        if(!Frame->Begin(640,360)) return;
        if(!AppliedLightChange && EM_ASM_INT({return window['pointLightPhase']||0;})==1) {
            LiveRecords.erase(LiveRecords.begin()+1);
            for(auto& record:LiveRecords) if(record.Type==1) record.Range=20;
            Lights.PrepareRecords(LiveRecords,4,1); Plan.PrepareForCamera(CameraPosition,float3::Normalize(CameraPosition),Lights,4); AppliedLightChange=true;
        }
        Shadows->RenderCasters({Caster},Plan);
        if(AppliedLightChange) {
            auto state=Shadows->ShadowValues(); state["lightMetadata"]=Lights.ForwardValues().at("lightMetadata"); auto json=state.dump();
            EM_ASM({window['pointShadowState']=JSON.parse(UTF8ToString($0));window['pointLightPhaseApplied']=1;},json.c_str());
        }
        if(!Binding) Binding=Frame->Shaders->CreateBinding(*Forward,Shadows->TextureBindings());
        WGPURenderPassColorAttachment color=WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT; color.view=Frame->Color; color.loadOp=WGPULoadOp_Clear; color.storeOp=WGPUStoreOp_Store; color.clearValue={0,0,0,1};
        WGPURenderPassDepthStencilAttachment depth=WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT; depth.view=Frame->Depth; depth.depthLoadOp=WGPULoadOp_Clear; depth.depthStoreOp=WGPUStoreOp_Store; depth.depthClearValue=1;
        WGPURenderPassDescriptor descriptor=WGPU_RENDER_PASS_DESCRIPTOR_INIT; descriptor.colorAttachmentCount=1; descriptor.colorAttachments=&color; descriptor.depthStencilAttachment=&depth;
        auto pass=wgpuCommandEncoderBeginRenderPass(Frame->Encoder,&descriptor); wgpuRenderPassEncoderSetPipeline(pass,Frame->Shaders->Pipeline(*Forward,false,State)); Frame->Shaders->Bind(pass,*Forward,Binding,Constants());
        wgpuRenderPassEncoderSetVertexBuffer(pass,0,Receiver,0,WGPU_WHOLE_SIZE); wgpuRenderPassEncoderDraw(pass,3,1,0,0); wgpuRenderPassEncoderEnd(pass); wgpuRenderPassEncoderRelease(pass); Frame->End();
        EM_ASM({window['atlasShadowFrames']=(window['atlasShadowFrames']||0)+1;});
    } catch(const std::exception& error) {Fail(error.what());}
}
void Ready() {
    try {
        Frame=std::make_unique<WebGpuFrame>(Context); Shadows=std::make_unique<WebGpuShadowRenderer>(*Frame);
        Forward=Frame->Shaders->Load(ShaderArtifactPath({{"shaderAssetId","ForwardStandardShader"},{"vertexProgram","ForwardStandardShader.vs"},{"pixelProgram","ForwardStandardShader.ps"},{"variant","default"}}));
        Mode=EM_ASM_INT({return (['shadow','no-caster','no-receiver','zero-strength']).indexOf(new URLSearchParams(location.search).get('mode'));});
        int kind=EM_ASM_INT({return (['directional','spot','mixed','point','point-mixed']).indexOf(new URLSearchParams(location.search).get('light'));});
        WebGpuLightRecord ambient{}; ambient.Type=3; ambient.Color=float4(0.1f,0.1f,0.1f,1);
        WebGpuLightRecord directional{}; directional.Intensity=3; directional.ShadowsEnabled=true; directional.ShadowDistance=6; directional.ShadowStrength=Mode==3?0:1;
        bool offCamera=EM_ASM_INT({return new URLSearchParams(location.search).has('off-camera');});
        if(offCamera) {directional.Direction=float3(-0.948683298f,0,-0.316227766f); directional.ShadowDistance=30;}
        WebGpuLightRecord spot=directional; spot.Type=2; spot.Position=float3(0,0,6); spot.Range=20; spot.InnerConeDegrees=20; spot.OuterConeDegrees=40;
        std::vector<WebGpuLightRecord> records{ambient}; if(kind<3 && kind!=1)records.push_back(directional); if(kind<3 && kind!=0)records.push_back(spot);
        if(kind>=3) {
            WebGpuLightRecord point=directional; point.Type=1; point.Position=float3(0,0,0); point.Range=10; point.Intensity=30; records.push_back(point);
            if(kind==4) { point.Position=float3(0.05f,0.05f,0.05f); records.push_back(point); records.push_back(directional); }
        }
        LiveRecords=records;
        Lights.PrepareRecords(records,4,1); Plan.PrepareForCamera(float3(0,0,3),float3(0,0,-1),Lights,4);
        float3 position(0,0,3),target(0,0,0),up(0,1,0); float4x4 view,projection;
        float4x4::CreateLookAt__ref0_ref1_ref2_out3(position,target,up,view); float4x4::CreateOrthographicOffCenter__out6(-3,3,-1.6875f,1.6875f,0.1f,100,projection); float4x4::Multiply__ref0_ref1_out2(view,projection,ViewProjection);
        const float caster[24]={-1,-1,1,0,0,1,0,0,1,-1,1,0,0,1,0,0,0,1,1,0,0,1,0,0}; Caster.Vertices=Frame->Buffer(caster,sizeof(caster),WGPUBufferUsage_Vertex); Caster.Count=3; Caster.World=float4x4::get_Identity(); Caster.CastsShadows=Mode!=1;
        if(offCamera) float4x4::CreateTranslation__out3(4.5f,0,0.5f,Caster.World);
        const float receiver[24]={-4,-4,0,0,0,1,0,0,4,-4,0,0,0,1,0,0,0,4,0,0,0,1,0,0}; Receiver=Frame->Buffer(receiver,sizeof(receiver),WGPUBufferUsage_Vertex);
        if(kind>=3) {
            int face=EM_ASM_INT({return +(new URLSearchParams(location.search).get('face')||0);});
            float3 directions[7]={float3(1,0.22f,0.3f),float3(-1,0.22f,0.3f),float3(0.3f,1,0.22f),float3(0.3f,-1,0.22f),float3(0.3f,0.22f,1),float3(0.3f,0.22f,-1),float3(1,1,1)};
            auto direction=float3::Normalize(directions[face]); CameraPosition=direction; position=CameraPosition; target=direction*4; up=std::abs(direction.Y)>0.99f?float3(0,0,1):float3(0,1,0);
            float4x4::CreateLookAt__ref0_ref1_ref2_out3(position,target,up,view); float4x4::Multiply__ref0_ref1_out2(view,projection,ViewProjection);
            auto normal=direction*-1; auto right=float3::Normalize(float3::Cross(up,normal)); up=float3::Cross(normal,right);
            for(int mesh=0;mesh<2;++mesh) {
                float distance=mesh?4:2, size=mesh?4:0.25f; auto center=direction*distance;
                float3 positions[3]={center-right*size-up*size,center+right*size-up*size,center+up*size}; std::vector<float> data;
                for(auto vertex:positions) data.insert(data.end(),{vertex.X,vertex.Y,vertex.Z,normal.X,normal.Y,normal.Z,0,0});
                auto buffer=Frame->Buffer(data.data(),data.size()*sizeof(float),WGPUBufferUsage_Vertex);
                if(mesh) {wgpuBufferRelease(Receiver); Receiver=buffer;} else {wgpuBufferRelease(Caster.Vertices); Caster.Vertices=buffer; Caster.World=float4x4::get_Identity();}
            }
        }
        emscripten_set_main_loop(Draw,0,false);
    } catch(const std::exception& error) {Fail(error.what());}
}
}
int main() {Context.Initialize("#helengine-canvas",Ready,Fail);return 0;}
