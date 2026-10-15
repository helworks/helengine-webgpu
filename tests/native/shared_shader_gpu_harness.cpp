#include "platform/webgpu/webgpu_device_context.hpp"
#include "platform/webgpu/webgpu_frame.hpp"
#include "platform/webgpu/webgpu_shader_library.hpp"
#include "platform/webgpu/webgpu_material_artifact_reader.hpp"
#include "platform/webgpu/webgpu_material_bindings.hpp"
#include <emscripten.h>
#include <memory>
#include <stdexcept>
using namespace helengine::webgpu;
namespace {
WebGpuDeviceContext Context;
std::unique_ptr<WebGpuFrame> Frame;
std::shared_ptr<WebGpuShaderProgram> Program;
WebGpuMaterialArtifact Material;
WGPUBindGroup Binding=nullptr;
WGPUBuffer Vertices=nullptr;
WGPUTexture Texture=nullptr;
WGPUTextureView TextureView=nullptr;
void Fail(const std::string& message) { EM_ASM({window.sharedShaderError=UTF8ToString($0);},message.c_str()); emscripten_cancel_main_loop(); }
void Draw() {
    try {
        if(!Frame->Begin(640,360)) return;
        WGPURenderPassColorAttachment color=WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT; color.view=Frame->Color; color.loadOp=WGPULoadOp_Clear; color.storeOp=WGPUStoreOp_Store; color.clearValue={0,0,0,1};
        WGPURenderPassDepthStencilAttachment depth=WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT; depth.view=Frame->Depth; depth.depthLoadOp=WGPULoadOp_Clear; depth.depthStoreOp=WGPUStoreOp_Store; depth.depthClearValue=1;
        WGPURenderPassDescriptor passDescriptor=WGPU_RENDER_PASS_DESCRIPTOR_INIT; passDescriptor.colorAttachmentCount=1; passDescriptor.colorAttachments=&color; passDescriptor.depthStencilAttachment=&depth;
        auto pass=wgpuCommandEncoderBeginRenderPass(Frame->Encoder,&passDescriptor);
        auto pipeline=Frame->Shaders->Pipeline(*Program,false,Material.Metadata.at("renderState"));
        wgpuRenderPassEncoderSetPipeline(pass,pipeline); Frame->Shaders->Bind(pass,*Program,Binding,Material.Constants);
        wgpuRenderPassEncoderSetVertexBuffer(pass,0,Vertices,0,WGPU_WHOLE_SIZE); wgpuRenderPassEncoderDraw(pass,3,1,0,0);
        wgpuRenderPassEncoderEnd(pass); wgpuRenderPassEncoderRelease(pass); Frame->End();
        EM_ASM({window.sharedShaderFrames=(window.sharedShaderFrames||0)+1;});
    } catch(const std::exception& error) { Fail(error.what()); }
}
void Ready() {
    try {
        Frame=std::make_unique<WebGpuFrame>(Context);
        Material=ReadMaterialArtifact(ReadArtifactFile("/content/cooked/matrix_fixture.wgpm"));
        Program=Frame->Shaders->Load(Material.Metadata.at("shaderPath").get<std::string>());
        const auto& b=Program->Artifact.Metadata.at("bindings").at(0);
        float matrix[16]={0.5f,0,0,0,0,0.5f,0,0,0,0,1,0,0.25f,-0.25f,0,1};
        WriteEngineMatrix(b.at("sourceLayout").at("members").at(0),matrix,Material.Constants.at("FixtureBuffer"));
        float vertices[24]={-1,-1,0,0,0,1,0,0, 3,-1,0,0,0,1,0,0, -1,3,0,0,0,1,0,0};
        Vertices=Frame->Buffer(vertices,sizeof(vertices),WGPUBufferUsage_Vertex);
        WGPUTextureDescriptor texture=WGPU_TEXTURE_DESCRIPTOR_INIT; texture.size={1,1,1}; texture.format=WGPUTextureFormat_RGBA8Unorm; texture.usage=WGPUTextureUsage_TextureBinding|WGPUTextureUsage_CopyDst;
        Texture=wgpuDeviceCreateTexture(Frame->Device(),&texture); TextureView=wgpuTextureCreateView(Texture,nullptr);
        uint8_t pixel[4]={64,128,192,255}; WGPUTexelCopyTextureInfo destination=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT; destination.texture=Texture;
        WGPUTexelCopyBufferLayout layout=WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT; layout.bytesPerRow=4; layout.rowsPerImage=1; WGPUExtent3D extent{1,1,1};
        wgpuQueueWriteTexture(Frame->Queue,&destination,pixel,4,&layout,&extent);
        Binding=Frame->Shaders->CreateBinding(*Program,{{"FixtureTexture",TextureView}});
        emscripten_set_main_loop(Draw,0,false);
    } catch(const std::exception& error) { Fail(error.what()); }
}
}
int main() { Context.Initialize("#helengine-canvas",Ready,Fail); return 0; }
