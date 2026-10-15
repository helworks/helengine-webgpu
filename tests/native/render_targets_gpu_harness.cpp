#include "platform/webgpu/webgpu_frame.hpp"
#include "platform/webgpu/webgpu_render_target.hpp"
#include "platform/webgpu/webgpu_camera_target.hpp"
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
std::unique_ptr<WebGpuRenderTarget> Target;
std::shared_ptr<WebGpuShaderProgram> Program;
WebGpuMaterialArtifact Material;
WGPUBindGroup SourceBinding=nullptr, TargetBinding=nullptr;
WGPUBuffer Vertices=nullptr,Indices=nullptr;
WGPUBuffer BoundUniforms=nullptr;
WGPUTexture Source=nullptr;
WGPUTextureView SourceView=nullptr;
int Frames=0;
void Fail(const std::string& message) {EM_ASM({window['renderTargetError']=UTF8ToString($0);},message.c_str());emscripten_cancel_main_loop();}
void Pass(const WebGpuCameraTarget& target,WGPUBindGroup binding,bool load) {
    WGPURenderPassColorAttachment color=WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;color.view=target.Color;color.loadOp=load?WGPULoadOp_Load:WGPULoadOp_Clear;color.storeOp=WGPUStoreOp_Store;color.clearValue={0,0,0,1};
    WGPURenderPassDepthStencilAttachment depth=WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;depth.view=target.Depth;depth.depthLoadOp=WGPULoadOp_Clear;depth.depthStoreOp=WGPUStoreOp_Store;depth.depthClearValue=1;
    WGPURenderPassDescriptor descriptor=WGPU_RENDER_PASS_DESCRIPTOR_INIT;descriptor.colorAttachmentCount=1;descriptor.colorAttachments=&color;descriptor.depthStencilAttachment=target.Depth?&depth:nullptr;
    auto pass=wgpuCommandEncoderBeginRenderPass(Frame->Encoder,&descriptor);
    if(!load) {wgpuRenderPassEncoderSetPipeline(pass,Frame->Shaders->Pipeline(*Program,false,Material.Metadata.at("renderState"),target.Pipeline));Frame->Shaders->Bind(pass,*Program,binding,Material.Constants);wgpuRenderPassEncoderSetVertexBuffer(pass,0,Vertices,0,WGPU_WHOLE_SIZE);wgpuRenderPassEncoderSetIndexBuffer(pass,Indices,WGPUIndexFormat_Uint32,0,WGPU_WHOLE_SIZE);wgpuRenderPassEncoderDrawIndexed(pass,6,1,0,0,0);}
    wgpuRenderPassEncoderEnd(pass);wgpuRenderPassEncoderRelease(pass);
}
void Draw() {
    try {
        if(!Frame->Begin(640,360)) return;
        if(BoundUniforms!=Frame->Uniforms) {
            wgpuBindGroupRelease(SourceBinding);wgpuBindGroupRelease(TargetBinding);
            SourceBinding=Frame->Shaders->CreateBinding(*Program,{{"FixtureTexture",SourceView}});TargetBinding=Frame->Shaders->CreateBinding(*Program,{{"FixtureTexture",Target->View}});BoundUniforms=Frame->Uniforms;
            float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0.5f,0,0,1};auto& reflected=Program->Artifact.Metadata.at("bindings")[0];WriteEngineMatrix(reflected.at("sourceLayout").at("members")[0],matrix,Material.Constants.at("FixtureBuffer"));
            EM_ASM({window['renderTargetUniformGrew']=true;});
        }
        if(Target) Pass(ResolveRenderTarget(*Frame,Target.get()),SourceBinding,Frames>0);
        Pass(ResolveRenderTarget(*Frame,nullptr),TargetBinding,false);
        if(Frames<2) {uint32_t data[4]={0,0,0,0};int count=Frames==0?12*1024:24*1024;for(int index=0;index<count;++index)Frame->Uniform(data,sizeof(data));}
        if(Frames==2) {
            Target->Dispose();bool rejectsDisposed=false;try {ResolveRenderTarget(*Frame,Target.get());}catch(const std::exception&){rejectsDisposed=true;}
            if(!rejectsDisposed)throw std::runtime_error("Disposed target remained renderable");Target.reset();
            EM_ASM({window['renderTargetRetired']=true;});
        }
        Frame->End();++Frames;EM_ASM({window['renderTargetFrames']=$0;},Frames);
    }catch(const std::exception& error){Fail(error.what());}
}
void Ready() {
    try {
        Frame=std::make_unique<WebGpuFrame>(Context);Target=std::make_unique<WebGpuRenderTarget>(*Frame,64,64);
        Material=ReadMaterialArtifact(ReadArtifactFile("/content/cooked/matrix_fixture.wgpm"));Program=Frame->Shaders->Load(Material.Metadata.at("shaderPath").get<std::string>());
        float matrix[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};auto& binding=Program->Artifact.Metadata.at("bindings")[0];WriteEngineMatrix(binding.at("sourceLayout").at("members")[0],matrix,Material.Constants.at("FixtureBuffer"));
        float vertices[32]={-1,-1,0,0,0,1,0,0,1,-1,0,0,0,1,0,0,-1,1,0,0,0,1,0,0,1,1,0,0,0,1,0,0};Vertices=Frame->Buffer(vertices,sizeof(vertices),WGPUBufferUsage_Vertex);
        uint32_t indices[6]={0,1,2,2,1,3};Indices=Frame->Buffer(indices,sizeof(indices),WGPUBufferUsage_Index);
        auto lines=Material.Metadata.at("renderState");lines["primitiveTopology"]="line-list";Frame->Shaders->Pipeline(*Program,false,lines);
        WGPUTextureDescriptor texture=WGPU_TEXTURE_DESCRIPTOR_INIT;texture.size={1,1,1};texture.format=WGPUTextureFormat_RGBA8Unorm;texture.usage=WGPUTextureUsage_TextureBinding|WGPUTextureUsage_CopyDst;Source=wgpuDeviceCreateTexture(Frame->Device(),&texture);SourceView=wgpuTextureCreateView(Source,nullptr);
        uint8_t pixel[4]={64,128,192,255};WGPUTexelCopyTextureInfo destination=WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;destination.texture=Source;WGPUTexelCopyBufferLayout layout=WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;layout.bytesPerRow=4;layout.rowsPerImage=1;WGPUExtent3D extent{1,1,1};wgpuQueueWriteTexture(Frame->Queue,&destination,pixel,4,&layout,&extent);
        SourceBinding=Frame->Shaders->CreateBinding(*Program,{{"FixtureTexture",SourceView}});TargetBinding=Frame->Shaders->CreateBinding(*Program,{{"FixtureTexture",Target->View}});
        BoundUniforms=Frame->Uniforms;
        bool rejectsSize=false;try {WebGpuRenderTarget invalid(*Frame,0,64);}catch(const std::exception&){rejectsSize=true;}
        auto state=Material.Metadata.at("renderState");state["useVertexColor"]=true;bool rejectsColor=false;try {Frame->Shaders->Pipeline(*Program,false,state);}catch(const std::exception&){rejectsColor=true;}
        if(!rejectsSize || !rejectsColor)throw std::runtime_error("Required render target/mesh input validation was bypassed");
        EM_ASM({window['renderTargetValidation']=({'invalidSizeRejected':!!$0,'missingColorStreamRejected':!!$1});},rejectsSize,rejectsColor);emscripten_set_main_loop(Draw,0,false);
    }catch(const std::exception& error){Fail(error.what());}
}
}
int main(){Context.Initialize("#helengine-canvas",Ready,Fail);return 0;}
