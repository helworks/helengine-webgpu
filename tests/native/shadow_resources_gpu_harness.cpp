#include "platform/webgpu/webgpu_frame.hpp"
#include "platform/webgpu/webgpu_shader_library.hpp"
#include "platform/webgpu/webgpu_pipeline_target.hpp"
#include <emscripten.h>
#include <memory>
#include <stdexcept>
using namespace helengine::webgpu;
namespace {
WebGpuDeviceContext Context;
std::unique_ptr<WebGpuFrame> Frame;
std::shared_ptr<WebGpuShaderProgram> Probe, DepthProgram;
WGPUBindGroup Binding = nullptr;
WGPUBuffer Vertices = nullptr;
WGPUTexture Atlas = nullptr, Cube = nullptr;
WGPUTextureView AtlasView = nullptr, CubeView = nullptr;
ArtifactJson State = {{"alphaMode","opaque"},{"doubleSided",true},{"useVertexColor",false},{"depthTest",true},{"depthWrite",true}};
void Fail(const std::string& message) { EM_ASM({window['shadowResourceError']=UTF8ToString($0);},message.c_str()); emscripten_cancel_main_loop(); }
std::shared_ptr<WebGpuShaderProgram> Load(const char* asset) {
    return Frame->Shaders->Load(ShaderArtifactPath({{"shaderAssetId",asset},{"vertexProgram",std::string(asset)+".vs"},{"pixelProgram",std::string(asset)+".ps"},{"variant","default"}}));
}
void Draw() {
    try {
        if (!Frame->Begin(640,360)) return;
        WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT; color.view=Frame->Color; color.loadOp=WGPULoadOp_Clear; color.storeOp=WGPUStoreOp_Store; color.clearValue={0,0,0,1};
        WGPURenderPassDepthStencilAttachment depth = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT; depth.view=Frame->Depth; depth.depthLoadOp=WGPULoadOp_Clear; depth.depthStoreOp=WGPUStoreOp_Store; depth.depthClearValue=1;
        WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT; descriptor.colorAttachmentCount=1; descriptor.colorAttachments=&color; descriptor.depthStencilAttachment=&depth;
        auto pass=wgpuCommandEncoderBeginRenderPass(Frame->Encoder,&descriptor);
        wgpuRenderPassEncoderSetPipeline(pass,Frame->Shaders->Pipeline(*Probe,false,State)); Frame->Shaders->Bind(pass,*Probe,Binding,{});
        wgpuRenderPassEncoderSetVertexBuffer(pass,0,Vertices,0,WGPU_WHOLE_SIZE); wgpuRenderPassEncoderDraw(pass,3,1,0,0);
        wgpuRenderPassEncoderEnd(pass); wgpuRenderPassEncoderRelease(pass); Frame->End();
        EM_ASM({window['shadowResourceFrames']=(window['shadowResourceFrames']||0)+1;});
    } catch (const std::exception& error) { Fail(error.what()); }
}
void Ready() {
    try {
        Frame=std::make_unique<WebGpuFrame>(Context); Probe=Load("shadow_probe"); DepthProgram=Load("shadow_depth");
        float vertices[24]={-1,-1,0,0,0,1,0,0,3,-1,0,0,0,1,0,0,-1,3,0,0,0,1,0,0}; Vertices=Frame->Buffer(vertices,sizeof(vertices),WGPUBufferUsage_Vertex);
        WebGpuPipelineTarget target{}; target.HasColor=false; target.DepthFormat=WGPUTextureFormat_Depth32Float;
        auto pipeline=Frame->Shaders->Pipeline(*DepthProgram,false,State,target);
        bool reuse=pipeline==Frame->Shaders->Pipeline(*DepthProgram,false,State,target);
        auto other=target; other.DepthFormat=WGPUTextureFormat_Depth24Plus; bool format=pipeline!=Frame->Shaders->Pipeline(*DepthProgram,false,State,other);
        other=target; other.DepthBias=1; bool bias=pipeline!=Frame->Shaders->Pipeline(*DepthProgram,false,State,other);
        EM_ASM({window['shadowPipelineCache']=({'reusesSameTarget':!!$0,'separatesDepthFormat':!!$1,'separatesBias':!!$2});},reuse,format,bias);
        WGPUTextureDescriptor texture=WGPU_TEXTURE_DESCRIPTOR_INIT; texture.size={1,1,1}; texture.format=WGPUTextureFormat_Depth32Float; texture.usage=WGPUTextureUsage_RenderAttachment|WGPUTextureUsage_TextureBinding;
        Atlas=wgpuDeviceCreateTexture(Frame->Device(),&texture); AtlasView=wgpuTextureCreateView(Atlas,nullptr); texture.size.depthOrArrayLayers=6; Cube=wgpuDeviceCreateTexture(Frame->Device(),&texture);
        WGPUTextureViewDescriptor view=WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT; view.dimension=WGPUTextureViewDimension_Cube; view.arrayLayerCount=6; CubeView=wgpuTextureCreateView(Cube,&view);
        auto encoder=wgpuDeviceCreateCommandEncoder(Frame->Device(),nullptr); auto empty=Frame->Shaders->CreateBinding(*DepthProgram,{});
        for (int face=-1;face<6;++face) {
            WGPUTextureView faceView=AtlasView;
            if (face>=0) { view.dimension=WGPUTextureViewDimension_2D; view.baseArrayLayer=face; view.arrayLayerCount=1; faceView=wgpuTextureCreateView(Cube,&view); }
            WGPURenderPassDepthStencilAttachment depth=WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT; depth.view=faceView; depth.depthLoadOp=WGPULoadOp_Clear; depth.depthStoreOp=WGPUStoreOp_Store; depth.depthClearValue=face>=0 && face%2?0.75:1;
            WGPURenderPassDescriptor passDescriptor=WGPU_RENDER_PASS_DESCRIPTOR_INIT; passDescriptor.depthStencilAttachment=&depth;
            auto pass=wgpuCommandEncoderBeginRenderPass(encoder,&passDescriptor);
            if (face<0 || face%2==0) { wgpuRenderPassEncoderSetPipeline(pass,pipeline); Frame->Shaders->Bind(pass,*DepthProgram,empty,{}); wgpuRenderPassEncoderSetVertexBuffer(pass,0,Vertices,0,WGPU_WHOLE_SIZE); wgpuRenderPassEncoderDraw(pass,3,1,0,0); }
            wgpuRenderPassEncoderEnd(pass); wgpuRenderPassEncoderRelease(pass); if(face>=0) wgpuTextureViewRelease(faceView);
        }
        auto commands=wgpuCommandEncoderFinish(encoder,nullptr); wgpuQueueSubmit(Frame->Queue,1,&commands); wgpuCommandBufferRelease(commands); wgpuCommandEncoderRelease(encoder); wgpuBindGroupRelease(empty);
        Binding=Frame->Shaders->CreateBinding(*Probe,{{"DepthAtlas",AtlasView},{"DepthCube",CubeView}}); emscripten_set_main_loop(Draw,0,false);
    } catch (const std::exception& error) { Fail(error.what()); }
}
}
int main() { Context.Initialize("#helengine-canvas",Ready,Fail); return 0; }
