#include "webgpu_shadow_renderer.hpp"
#include "webgpu_material_bindings.hpp"
#include <cstring>
#include <stdexcept>
namespace helengine::webgpu {
WebGpuShadowRenderer::WebGpuShadowRenderer(WebGpuFrame& frame):Frame(frame) {}
WebGpuShadowRenderer::~WebGpuShadowRenderer() {
    if(DepthBinding) wgpuBindGroupRelease(DepthBinding); if(AtlasView) wgpuTextureViewRelease(AtlasView); if(Atlas) wgpuTextureRelease(Atlas);
}
void WebGpuShadowRenderer::AllocateAtlas() {
    if(Atlas) return;
    auto selection=ArtifactJson({{"shaderAssetId","EditorShadowDepth"},{"vertexProgram","EditorShadowDepth.vs"},{"pixelProgram","EditorShadowDepth.ps"},{"variant","default"}});
    DepthProgram=Frame.Shaders->Load(ShaderArtifactPath(selection)); DepthBinding=Frame.Shaders->CreateBinding(*DepthProgram,{});
    WGPUTextureDescriptor texture=WGPU_TEXTURE_DESCRIPTOR_INIT; texture.size={WebGpuShadowPlan::AtlasSize,WebGpuShadowPlan::AtlasSize,1}; texture.format=WGPUTextureFormat_Depth32Float;
    texture.usage=WGPUTextureUsage_RenderAttachment|WGPUTextureUsage_TextureBinding; Atlas=wgpuDeviceCreateTexture(Frame.Device(),&texture); AtlasView=wgpuTextureCreateView(Atlas,nullptr); ++ResourceGeneration;
}
void WebGpuShadowRenderer::RenderCasters(const std::vector<WebGpuShadowCaster>& casters,const WebGpuShadowPlan& plan) {
    CurrentPlan=plan; if(plan.Entries.empty()) return;
    for(auto& entry:plan.Entries) if(entry.Light.Type==1) throw std::runtime_error("Point shadow passes are not enabled yet");
    AllocateAtlas();
    WGPURenderPassDepthStencilAttachment depth=WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT; depth.view=AtlasView; depth.depthLoadOp=WGPULoadOp_Clear; depth.depthStoreOp=WGPUStoreOp_Store; depth.depthClearValue=1;
    WGPURenderPassDescriptor descriptor=WGPU_RENDER_PASS_DESCRIPTOR_INIT; descriptor.depthStencilAttachment=&depth;
    auto pass=wgpuCommandEncoderBeginRenderPass(Frame.Encoder,&descriptor);
    for(auto& entry:plan.Entries) {
        wgpuRenderPassEncoderSetViewport(pass,entry.X,entry.Y,entry.Width,entry.Height,0,1); wgpuRenderPassEncoderSetScissorRect(pass,entry.X,entry.Y,entry.Width,entry.Height);
        DrawCasters(pass,casters,entry.Matrix);
    }
    wgpuRenderPassEncoderEnd(pass); wgpuRenderPassEncoderRelease(pass);
}
void WebGpuShadowRenderer::DrawCasters(WGPURenderPassEncoder pass,const std::vector<WebGpuShadowCaster>& casters,float4x4 matrix) {
    ArtifactJson state={{"alphaMode","opaque"},{"doubleSided",true},{"useVertexColor",false},{"depthTest",true},{"depthWrite",true}};
    WebGpuPipelineTarget target{}; target.HasColor=false; target.DepthFormat=WGPUTextureFormat_Depth32Float;
    wgpuRenderPassEncoderSetPipeline(pass,Frame.Shaders->Pipeline(*DepthProgram,false,state,target));
    for(auto& caster:casters) {
        if(!caster.CastsShadows || !caster.Count) continue;
        auto world=caster.World; float4x4 mvp; float4x4::Multiply__ref0_ref1_out2(world,matrix,mvp);
        std::map<std::string,std::vector<std::byte>> constants;
        for(auto& binding:DepthProgram->Artifact.Metadata.at("bindings")) {
            if(binding.at("class")!="constant-buffer") continue;
            auto& bytes=constants[binding.at("name").get<std::string>()]; bytes.assign(binding.at("sourceLayout").at("size").get<size_t>(),std::byte{});
            for(auto& member:binding.at("sourceLayout").at("members")) {
                auto name=member.at("name").get<std::string>();
                if(name=="worldViewProj") WriteEngineMatrix(member,reinterpret_cast<const float*>(&mvp),bytes);
                else if(name!="color") throw std::runtime_error("Unknown atlas shadow shader constant: "+name);
            }
        }
        Frame.Shaders->Bind(pass,*DepthProgram,DepthBinding,constants); wgpuRenderPassEncoderSetVertexBuffer(pass,0,caster.Vertices,0,WGPU_WHOLE_SIZE);
        if(caster.Indices) { wgpuRenderPassEncoderSetIndexBuffer(pass,caster.Indices,WGPUIndexFormat_Uint32,0,WGPU_WHOLE_SIZE); wgpuRenderPassEncoderDrawIndexed(pass,caster.Count,1,caster.First,0,0); }
        else wgpuRenderPassEncoderDraw(pass,caster.Count,1,caster.First,0);
    }
}
std::map<std::string,WGPUTextureView> WebGpuShadowRenderer::TextureBindings() const {
    std::map<std::string,WGPUTextureView> views; if(AtlasView) views.emplace("shadowAtlasTexture",AtlasView); return views;
}
ArtifactJson WebGpuShadowRenderer::ShadowValues() const {
    ArtifactJson values; size_t atlasCount=0;
    for(auto& entry:CurrentPlan.Entries) if(entry.Light.Type!=1) ++atlasCount;
    values["shadowMetadata"]={atlasCount?1:0,1.0f/WebGpuShadowPlan::AtlasSize,1.0f/WebGpuShadowPlan::AtlasSize,atlasCount};
    for(int slot=0;slot<4;++slot) {
        std::string prefix="shadowLight"+std::to_string(slot); values[prefix+"AtlasRect"]={0,0,0,0}; values[prefix+"Metadata"]={0,0,0,0}; values[prefix+"WorldToShadowClip"]=std::vector<float>(16,0);
    }
    for(auto& entry:CurrentPlan.Entries) {
        std::string prefix="shadowLight"+std::to_string(entry.LightSlot);
        values[prefix+"AtlasRect"]={float(entry.X)/WebGpuShadowPlan::AtlasSize,float(entry.Y)/WebGpuShadowPlan::AtlasSize,float(entry.Width)/WebGpuShadowPlan::AtlasSize,float(entry.Height)/WebGpuShadowPlan::AtlasSize};
        values[prefix+"Metadata"]={1,entry.Light.ShadowStrength,1,0};
        const float* matrix=reinterpret_cast<const float*>(&entry.Matrix); values[prefix+"WorldToShadowClip"]=std::vector<float>(matrix,matrix+16);
    }
    return values;
}
}
