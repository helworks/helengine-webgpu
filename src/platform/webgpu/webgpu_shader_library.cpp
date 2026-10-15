#include "webgpu_shader_library.hpp"
#include "webgpu_material_bindings.hpp"
#include "webgpu_frame.hpp"
#include <emscripten.h>
#include <stdexcept>
#include <algorithm>
namespace helengine::webgpu {
WebGpuShaderProgram::~WebGpuShaderProgram() {
    for (auto& pair : Pipelines) wgpuRenderPipelineRelease(pair.second);
    if (PipelineLayout) wgpuPipelineLayoutRelease(PipelineLayout); if (Layout) wgpuBindGroupLayoutRelease(Layout);
    if (Vertex) wgpuShaderModuleRelease(Vertex); if (Fragment) wgpuShaderModuleRelease(Fragment);
}
WebGpuShaderLibrary::WebGpuShaderLibrary(WebGpuFrame& frame) : Frame(frame) {
    WGPUSamplerDescriptor sampler = WGPU_SAMPLER_DESCRIPTOR_INIT; sampler.magFilter = WGPUFilterMode_Linear; sampler.minFilter = WGPUFilterMode_Linear;
    Sampler = wgpuDeviceCreateSampler(Frame.Device(),&sampler);
    White = CreateSolid(false,false); WhiteView = wgpuTextureCreateView(White,nullptr);
    Black = CreateSolid(false,true); BlackView = wgpuTextureCreateView(Black,nullptr);
    Cube = CreateSolid(true,false); WGPUTextureViewDescriptor view = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT; view.dimension = WGPUTextureViewDimension_Cube; view.arrayLayerCount = 6;
    CubeView = wgpuTextureCreateView(Cube,&view);
    WGPUSamplerDescriptor comparison = WGPU_SAMPLER_DESCRIPTOR_INIT; comparison.compare = WGPUCompareFunction_LessEqual;
    comparison.magFilter = WGPUFilterMode_Nearest; comparison.minFilter = WGPUFilterMode_Nearest;
    ComparisonSampler = wgpuDeviceCreateSampler(Frame.Device(),&comparison);
    InactiveDepth = CreateInactiveDepth(false); InactiveDepthView = wgpuTextureCreateView(InactiveDepth,nullptr);
    InactiveDepthCube = CreateInactiveDepth(true); InactiveDepthCubeView = wgpuTextureCreateView(InactiveDepthCube,&view);
}
WebGpuShaderLibrary::~WebGpuShaderLibrary() {
    Programs.clear(); wgpuTextureViewRelease(WhiteView); wgpuTextureViewRelease(BlackView); wgpuTextureViewRelease(CubeView);
    wgpuTextureRelease(White); wgpuTextureRelease(Black); wgpuTextureRelease(Cube); wgpuSamplerRelease(Sampler);
    wgpuTextureViewRelease(InactiveDepthView); wgpuTextureViewRelease(InactiveDepthCubeView);
    wgpuTextureRelease(InactiveDepth); wgpuTextureRelease(InactiveDepthCube); wgpuSamplerRelease(ComparisonSampler);
}
WGPUTexture WebGpuShaderLibrary::CreateInactiveDepth(bool cube) {
    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT; descriptor.size = {1,1,cube?6u:1u}; descriptor.format = WGPUTextureFormat_Depth32Float;
    descriptor.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
    auto texture = wgpuDeviceCreateTexture(Frame.Device(),&descriptor); auto encoder = wgpuDeviceCreateCommandEncoder(Frame.Device(),nullptr);
    for (uint32_t layer=0;layer<descriptor.size.depthOrArrayLayers;++layer) {
        WGPUTextureViewDescriptor view = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT; view.dimension=WGPUTextureViewDimension_2D; view.baseArrayLayer=layer; view.arrayLayerCount=1;
        auto face = wgpuTextureCreateView(texture,&view);
        WGPURenderPassDepthStencilAttachment depth = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT; depth.view=face; depth.depthLoadOp=WGPULoadOp_Clear; depth.depthStoreOp=WGPUStoreOp_Store; depth.depthClearValue=1;
        WGPURenderPassDescriptor passDescriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT; passDescriptor.depthStencilAttachment=&depth;
        auto pass = wgpuCommandEncoderBeginRenderPass(encoder,&passDescriptor); wgpuRenderPassEncoderEnd(pass); wgpuRenderPassEncoderRelease(pass); wgpuTextureViewRelease(face);
    }
    auto commands=wgpuCommandEncoderFinish(encoder,nullptr); wgpuQueueSubmit(Frame.Queue,1,&commands); wgpuCommandBufferRelease(commands); wgpuCommandEncoderRelease(encoder);
    return texture;
}
WGPUTexture WebGpuShaderLibrary::CreateSolid(bool cube, bool black) {
    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT; descriptor.size = {1,1,cube?6u:1u}; descriptor.format = WGPUTextureFormat_RGBA8Unorm;
    descriptor.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    auto texture = wgpuDeviceCreateTexture(Frame.Device(),&descriptor); uint8_t pixel[4] = {uint8_t(black?0:255),uint8_t(black?0:255),uint8_t(black?0:255),255};
    for (uint32_t layer=0;layer<descriptor.size.depthOrArrayLayers;++layer) {
        WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT; destination.texture = texture; destination.origin.z = layer;
        WGPUTexelCopyBufferLayout layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT; layout.bytesPerRow = 4; layout.rowsPerImage = 1; WGPUExtent3D extent{1,1,1};
        wgpuQueueWriteTexture(Frame.Queue,&destination,pixel,4,&layout,&extent);
    }
    return texture;
}
std::shared_ptr<WebGpuShaderProgram> WebGpuShaderLibrary::Load(const std::string& path) {
    ValidateContentPath(path); auto found = Programs.find(path); if (found != Programs.end()) return found->second;
    return Load(path,ReadArtifactFile("/content/"+path));
}
std::shared_ptr<WebGpuShaderProgram> WebGpuShaderLibrary::Load(const std::string& path, std::span<const std::byte> bytes) {
    ValidateContentPath(path); auto artifact = ReadShaderArtifact(bytes);
    if (ShaderArtifactPath(artifact.Metadata) != path) throw std::runtime_error("Shader artifact path does not match its selection: "+path);
    auto found = Programs.find(path);
    if (found != Programs.end()) {
        if (found->second->Artifact.Metadata != artifact.Metadata || found->second->Artifact.Vertex != artifact.Vertex || found->second->Artifact.Fragment != artifact.Fragment) throw std::runtime_error("Shader selection changed inside an immutable package: "+path);
        return found->second;
    }
    auto program = std::make_shared<WebGpuShaderProgram>(); program->Artifact = std::move(artifact);
    auto module = [&](const std::string& code) {
        WGPUShaderSourceWGSL source = WGPU_SHADER_SOURCE_WGSL_INIT; source.code = {code.c_str(),code.size()};
        WGPUShaderModuleDescriptor descriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT; descriptor.nextInChain = &source.chain; descriptor.label = {path.c_str(),path.size()};
        return wgpuDeviceCreateShaderModule(Frame.Device(),&descriptor);
    };
    program->Vertex = module(program->Artifact.Vertex); program->Fragment = module(program->Artifact.Fragment);
    std::vector<WGPUBindGroupLayoutEntry> entries;
    for (auto& b : program->Artifact.Metadata.at("bindings")) {
        WGPUBindGroupLayoutEntry entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT; entry.binding = b.at("binding").get<uint32_t>();
        for (auto& stage : b.at("stages")) entry.visibility |= stage == "vertex" ? WGPUShaderStage_Vertex : WGPUShaderStage_Fragment;
        auto resource = b.at("class").get<std::string>();
        if (resource == "constant-buffer") { entry.buffer.type = WGPUBufferBindingType_Uniform; entry.buffer.hasDynamicOffset = true; entry.buffer.minBindingSize = b.at("size").get<uint64_t>(); }
        else if (resource == "texture" || resource == "depth-texture") { entry.texture.sampleType = resource == "depth-texture" ? WGPUTextureSampleType_Depth : WGPUTextureSampleType_Float; entry.texture.viewDimension = b.at("textureDimension") == 4 ? WGPUTextureViewDimension_Cube : WGPUTextureViewDimension_2D; }
        else entry.sampler.type = b.at("samplerType") == 0 ? WGPUSamplerBindingType_Comparison : WGPUSamplerBindingType_Filtering;
        entries.push_back(entry);
    }
    WGPUBindGroupLayoutDescriptor layout = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT; layout.entryCount = entries.size(); layout.entries = entries.data();
    program->Layout = wgpuDeviceCreateBindGroupLayout(Frame.Device(),&layout);
    WGPUPipelineLayoutDescriptor pipeline = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT; pipeline.bindGroupLayoutCount=1; pipeline.bindGroupLayouts=&program->Layout;
    program->PipelineLayout = wgpuDeviceCreatePipelineLayout(Frame.Device(),&pipeline);
    Programs.emplace(path,program);
    auto diagnostic = program->Artifact.Metadata; diagnostic.erase("bindings"); diagnostic.erase("dependencies"); diagnostic.erase("vertexInputs"); diagnostic.erase("vertexOutputs"); diagnostic.erase("fragmentInputs");
    std::string identity = diagnostic.dump();
    EM_ASM({window['helengineShaderDiagnostics']=window['helengineShaderDiagnostics']||[]; window['helengineShaderDiagnostics'].push(JSON.parse(UTF8ToString($0)));},identity.c_str());
    return program;
}
WGPURenderPipeline WebGpuShaderLibrary::Pipeline(WebGpuShaderProgram& program, bool ui, const ArtifactJson& state) {
    return Pipeline(program,ui,state,{Frame.Format(),ui?WGPUTextureFormat_Undefined:WGPUTextureFormat_Depth24Plus,true,0,0});
}
WGPURenderPipeline WebGpuShaderLibrary::Pipeline(WebGpuShaderProgram& program, bool ui, const ArtifactJson& state, const WebGpuPipelineTarget& target) {
    std::string key = (ui?"ui:":"mesh:")+state.dump()+":"+ArtifactJson::array({static_cast<uint32_t>(target.ColorFormat),static_cast<uint32_t>(target.DepthFormat),target.HasColor,target.DepthBias,target.DepthBiasSlopeScale}).dump();
    auto cached = program.Pipelines.find(key); if(cached!=program.Pipelines.end()) return cached->second;
    if (!ui && state.at("useVertexColor").get<bool>()) throw std::runtime_error("Authored vertex-color materials require a mesh color stream, which this renderer has not implemented");
    std::vector<WGPUVertexAttribute> attributes;
    for (auto& input : program.Artifact.Metadata.at("vertexInputs")) {
        std::string semantic = input.at("semantic"); unsigned index = input.at("semanticIndex").get<unsigned>(); size_t offset = 0; unsigned width = 0;
        if(ui) {
            if (semantic=="POSITION" && index==0) { offset=0; width=4; }
            else if(semantic=="TEXCOORD" && index<=2) { offset=index==0?16:index==1?48:64; width=4; }
            else if(semantic=="COLOR" && index<=1) { offset=index==0?32:80; width=4; }
        } else {
            if(semantic=="POSITION" && index==0) { offset=0; width=3; }
            else if(semantic=="NORMAL" && index==0) { offset=12; width=3; }
            else if(semantic=="TEXCOORD" && index==0) { offset=24; width=2; }
        }
        if(!width || input.at("type") != "FP32:32:"+std::to_string(width)+":1") throw std::runtime_error("Shader vertex semantic cannot be supplied: "+semantic+std::to_string(index));
        WGPUVertexAttribute a = WGPU_VERTEX_ATTRIBUTE_INIT; a.offset=offset; a.shaderLocation=input.at("location").get<uint32_t>(); a.format=width==2?WGPUVertexFormat_Float32x2:width==3?WGPUVertexFormat_Float32x3:WGPUVertexFormat_Float32x4; attributes.push_back(a);
    }
    WGPUVertexBufferLayout vertices = WGPU_VERTEX_BUFFER_LAYOUT_INIT; vertices.arrayStride=ui?96:32; vertices.stepMode=WGPUVertexStepMode_Vertex; vertices.attributeCount=attributes.size(); vertices.attributes=attributes.data();
    WGPUBlendState blend = WGPU_BLEND_STATE_INIT; auto alpha=state.at("alphaMode").get<std::string>();
    blend.color.srcFactor=WGPUBlendFactor_SrcAlpha; blend.color.dstFactor=alpha=="additive"?WGPUBlendFactor_One:WGPUBlendFactor_OneMinusSrcAlpha;
    blend.alpha.srcFactor=WGPUBlendFactor_One; blend.alpha.dstFactor=alpha=="additive"?WGPUBlendFactor_One:WGPUBlendFactor_OneMinusSrcAlpha;
    WGPUColorTargetState color = WGPU_COLOR_TARGET_STATE_INIT; color.format=target.ColorFormat; if(alpha!="opaque") color.blend=&blend;
    std::string vertexEntry=program.Artifact.Metadata.at("vertexEntryPoint"),fragmentEntry=program.Artifact.Metadata.at("fragmentEntryPoint");
    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT; fragment.module=program.Fragment; fragment.entryPoint={fragmentEntry.c_str(),fragmentEntry.size()}; fragment.targetCount=target.HasColor?1:0; fragment.targets=target.HasColor?&color:nullptr;
    WGPUDepthStencilState depth = WGPU_DEPTH_STENCIL_STATE_INIT; depth.format=target.DepthFormat; depth.depthWriteEnabled=state.at("depthWrite").get<bool>()?WGPUOptionalBool_True:WGPUOptionalBool_False; depth.depthCompare=state.at("depthTest").get<bool>()?WGPUCompareFunction_Less:WGPUCompareFunction_Always;
    depth.depthBias=target.DepthBias; depth.depthBiasSlopeScale=target.DepthBiasSlopeScale;
    WGPURenderPipelineDescriptor descriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT; descriptor.layout=program.PipelineLayout;
    descriptor.vertex.module=program.Vertex; descriptor.vertex.entryPoint={vertexEntry.c_str(),vertexEntry.size()}; descriptor.vertex.bufferCount=1; descriptor.vertex.buffers=&vertices;
    descriptor.fragment=&fragment; if(target.DepthFormat!=WGPUTextureFormat_Undefined) descriptor.depthStencil=&depth;
    std::string topology=state.contains("primitiveTopology")?state.at("primitiveTopology").get<std::string>():"triangle-list";
    if(topology=="triangle-list") descriptor.primitive.topology=WGPUPrimitiveTopology_TriangleList;
    else if(topology=="line-list") descriptor.primitive.topology=WGPUPrimitiveTopology_LineList;
    else throw std::runtime_error("Unsupported engine primitive topology: "+topology);
    descriptor.primitive.frontFace=WGPUFrontFace_CCW; descriptor.primitive.cullMode=ui||state.at("doubleSided").get<bool>()?WGPUCullMode_None:WGPUCullMode_Back;
    auto pipeline=wgpuDeviceCreateRenderPipeline(Frame.Device(),&descriptor); program.Pipelines.emplace(key,pipeline); return pipeline;
}
WGPUBindGroup WebGpuShaderLibrary::CreateBinding(WebGpuShaderProgram& program, const std::map<std::string,WGPUTextureView>& textures) {
    std::vector<WGPUBindGroupEntry> entries;
    for(auto& b:program.Artifact.Metadata.at("bindings")) {
        WGPUBindGroupEntry entry=WGPU_BIND_GROUP_ENTRY_INIT; entry.binding=b.at("binding").get<uint32_t>(); std::string resource=b.at("class"),name=b.at("name");
        if(resource=="constant-buffer") { entry.buffer=Frame.Uniforms; entry.size=b.at("size").get<uint64_t>(); }
        else if(resource=="sampler") entry.sampler=b.at("samplerType")==0?ComparisonSampler:Sampler;
        else {
            auto texture=textures.find(name);
            if(texture!=textures.end()) entry.textureView=texture->second;
            else if(resource=="depth-texture" && name=="shadowAtlasTexture") entry.textureView=InactiveDepthView;
            else if(resource=="depth-texture" && name.starts_with("pointShadowTexture")) entry.textureView=InactiveDepthCubeView;
            else if(resource=="depth-texture") throw std::runtime_error("Shader requires an unbound authored depth texture: "+name);
            else if(name=="DiffuseTexture" || name=="RoughnessTexture" || name=="shadowAtlasTexture") entry.textureView=WhiteView;
            else if(name=="EmissiveTexture") entry.textureView=BlackView;
            else if(name.starts_with("pointShadowTexture")) entry.textureView=CubeView;
            else throw std::runtime_error("Shader requires an unbound authored texture: "+name);
        }
        entries.push_back(entry);
    }
    WGPUBindGroupDescriptor descriptor=WGPU_BIND_GROUP_DESCRIPTOR_INIT; descriptor.layout=program.Layout; descriptor.entries=entries.data(); descriptor.entryCount=entries.size(); return wgpuDeviceCreateBindGroup(Frame.Device(),&descriptor);
}
void WebGpuShaderLibrary::Bind(WGPURenderPassEncoder pass, WebGpuShaderProgram& program, WGPUBindGroup binding, const std::map<std::string,std::vector<std::byte>>& constants) {
    std::vector<std::pair<uint32_t,uint32_t>> offsets;
    for(auto& b:program.Artifact.Metadata.at("bindings")) if(b.at("class")=="constant-buffer") {
        std::string name=b.at("name"); auto data=constants.find(name); if(data==constants.end()) throw std::runtime_error("Shader requires an unbound named constant buffer: "+name);
        auto packed=PackMaterialBuffer(b,data->second); offsets.emplace_back(b.at("binding").get<uint32_t>(),Frame.Uniform(packed.data(),packed.size()));
    }
    std::sort(offsets.begin(),offsets.end()); std::vector<uint32_t> values; for(auto& offset:offsets) values.push_back(offset.second);
    wgpuRenderPassEncoderSetBindGroup(pass,0,binding,values.size(),values.data());
}
}
