#include "webgpu_render_manager_3d.hpp"
#include "webgpu_render_manager_2d.hpp"
#include "webgpu_runtime_model.hpp"
#include "webgpu_runtime_material.hpp"
#include "webgpu_material_bindings.hpp"
#include "webgpu_shadow_renderer.hpp"
#include "webgpu_render_target.hpp"
#include "webgpu_camera_target.hpp"
#include "webgpu_submesh_material.hpp"
#include "AssetSerializer.hpp"
#include "Core.hpp"
#include "ObjectManager.hpp"
#include "Entity.hpp"
#include "ICamera.hpp"
#include "CameraProjectionUtils.hpp"
#include "IRenderQueue3D.hpp"
#include "IDrawable3D.hpp"
#include "DirectionalLightComponent.hpp"
#include "RendererBackendCapabilityProfile.hpp"
#include "PlatformMaterialAsset.hpp"
#include "MaterialRenderState.hpp"
#include "MaterialBlendMode.hpp"
#include "MaterialCullMode.hpp"
#include "RenderList3D.hpp"
#include <emscripten.h>
#include "ModelAsset.hpp"
#include "ModelSubmeshResolver.hpp"
#include "RuntimeSubmesh.hpp"
#include "IContentStreamSource.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace helengine::webgpu {
namespace {
std::vector<std::byte> ContentBytes(const std::string& path, IContentStreamSource* source) {
    if (!source) throw std::runtime_error("Cooked material requires a content stream source");
    std::unique_ptr<Stream> stream(source->OpenRead(path));
    if (!stream) throw std::runtime_error("Missing cooked material/shader: "+path);
    std::vector<std::byte> bytes; uint8_t block[8192];
    while (size_t count=stream->Read(block,0,sizeof(block))) {
        if (bytes.size()+count>64*1024*1024) throw std::runtime_error("Cooked material/shader exceeds its size limit: "+path);
        bytes.insert(bytes.end(),reinterpret_cast<std::byte*>(block),reinterpret_cast<std::byte*>(block)+count);
    }
    return bytes;
}
void FloatMember(const ArtifactJson& member, const float4& value, std::span<std::byte> bytes) {
    size_t offset=member.at("offset").get<size_t>();
    if (member.at("size") != 16 || offset+16>bytes.size()) throw std::runtime_error("Engine float4 does not match shader reflection");
    std::memcpy(bytes.data()+offset,&value,16);
}
}
WebGpuRenderManager3D::WebGpuRenderManager3D(WebGpuFrame& frame) : Frame(frame),Shadows(std::make_unique<WebGpuShadowRenderer>(frame)) {}
WebGpuRenderManager3D::~WebGpuRenderManager3D() = default;
WebGpuRuntimeMaterial* WebGpuRenderManager3D::GetMissingMaterial() {
    if(!MissingMaterial) {
        auto material=std::make_unique<WebGpuRuntimeMaterial>();
        material->Program=Frame.Shaders->Load(ShaderArtifactPath({{"shaderAssetId","MissingMaterial"},{"vertexProgram","MissingMaterial.vs"},{"pixelProgram","MissingMaterial.ps"},{"variant","default"}}));
        material->Artifact.Metadata=ArtifactJson({{"textures",ArtifactJson::array()},{"renderState",{{"alphaMode","opaque"},{"doubleSided",false},{"depthTest",true},{"depthWrite",true},{"lit",false},{"useVertexColor",false}}}});
        material->set_CastsShadows(true); material->set_ReceivesShadows(false);
        MissingMaterial=std::move(material);
    }
    return MissingMaterial.get();
}
RenderTarget* WebGpuRenderManager3D::CreateRenderTarget(int32_t width,int32_t height) {return new WebGpuRenderTarget(Frame,width,height);}
RuntimeModel* WebGpuRenderManager3D::BuildModelFromRaw(ModelAsset* data) {
    if (!data || !data->Positions || !data->Normals || !data->Positions->Length || data->Positions->Length != data->Normals->Length) throw std::runtime_error("WebGPU mesh requires matching nonempty positions and normals");
    std::unique_ptr<WebGpuRuntimeModel> model(new WebGpuRuntimeModel());
    model->set_Id(data->get_Id()); model->SetBounds(data->BoundsMin, data->BoundsMax);
    struct Vertex { float Position[3], Normal[3], Uv[2]; };
    std::vector<Vertex> vertices(data->Positions->Length);
    for (int32_t i = 0; i < data->Positions->Length; ++i) {
        auto p = (*data->Positions)[i]; auto n = (*data->Normals)[i];
        float2 uv = data->TexCoords && i<data->TexCoords->Length ? (*data->TexCoords)[i] : float2(0,0);
        // Vertex attributes have an explicit float32 GPU layout independent of engine arithmetic.
        vertices[i] = {{static_cast<float>(p.X), static_cast<float>(p.Y), static_cast<float>(p.Z)}, {static_cast<float>(n.X), static_cast<float>(n.Y), static_cast<float>(n.Z)}, {static_cast<float>(uv.X), static_cast<float>(uv.Y)}};
    }
    model->VertexCount = static_cast<uint32_t>(vertices.size());
    model->Vertices = Frame.Buffer(vertices.data(), vertices.size() * sizeof(Vertex), WGPUBufferUsage_Vertex);
    std::vector<uint32_t> indices;
    if (data->Indices32 && data->Indices32->Length) indices.assign(data->Indices32->Data, data->Indices32->Data + data->Indices32->Length);
    else if (data->Indices16 && data->Indices16->Length) indices.assign(data->Indices16->Data, data->Indices16->Data + data->Indices16->Length);
    if (std::any_of(indices.begin(), indices.end(), [&model](uint32_t index) { return index >= model->VertexCount; })) throw std::runtime_error("WebGPU mesh index is outside the vertex stream");
    if (!indices.empty()) { model->IndexCount = indices.size(); model->Indices = Frame.Buffer(indices.data(), indices.size() * 4, WGPUBufferUsage_Index); }
    auto* ranges = ModelSubmeshResolver::BuildRuntimeSubmeshes(data); model->SetSubmeshes(ranges); delete ranges;
    return model.release();
}
RuntimeModel* WebGpuRenderManager3D::BuildModelFromCooked(HeCppString path, IContentStreamSource* source) {
    std::unique_ptr<Stream> stream(source->OpenRead(path));
    std::unique_ptr<Asset> asset(AssetSerializer::Deserialize(stream.get()));
    auto* model = dynamic_cast<ModelAsset*>(asset.get());
    if (!model) throw std::runtime_error("Prepared model has the wrong asset type: " + path.raw());
    return BuildModelFromRaw(model);
}
RuntimeMaterial* WebGpuRenderManager3D::BuildMaterialFromCooked(PlatformMaterialAsset*) {
    throw std::runtime_error("WebGPU materials require versioned WGPM data through the path/stream overload");
}
RuntimeMaterial* WebGpuRenderManager3D::BuildMaterialFromCooked(HeCppString path, IContentStreamSource* source) {
    auto material=std::make_unique<WebGpuRuntimeMaterial>(); material->Artifact=ReadMaterialArtifact(ContentBytes(path.raw(),source));
    auto& metadata=material->Artifact.Metadata; std::string shaderPath=metadata.at("shaderPath");
    material->Program=Frame.Shaders->Load(shaderPath,ContentBytes(shaderPath,source));
    material->set_Id(metadata.at("materialAssetId").get<std::string>());
    material->set_CastsShadows(metadata.at("castsShadows").get<bool>()); material->set_ReceivesShadows(metadata.at("receivesShadows").get<bool>());
    auto& authoredState=metadata.at("renderState"); MaterialRenderState engineState;
    engineState.set_BlendMode(authoredState.at("alphaMode")=="opaque"?MaterialBlendMode::Opaque:MaterialBlendMode::AlphaBlend);
    engineState.set_CullMode(authoredState.at("doubleSided").get<bool>()?MaterialCullMode::None:MaterialCullMode::Back);
    engineState.set_DepthTestEnabled(authoredState.at("depthTest").get<bool>()); engineState.set_DepthWriteEnabled(authoredState.at("depthWrite").get<bool>());
    material->SetRenderState(&engineState);
    auto* observed=material->get_RenderState();
    auto observedJson=ArtifactJson({{"materialAssetId",metadata.at("materialAssetId")},{"blendMode",static_cast<int>(observed->get_BlendMode())},
        {"cullMode",static_cast<int>(observed->get_CullMode())},{"depthTest",observed->get_DepthTestEnabled()},{"depthWrite",observed->get_DepthWriteEnabled()}}).dump();
    EM_ASM({window['helengineMaterialDiagnostics']=window['helengineMaterialDiagnostics']||[]; window['helengineMaterialDiagnostics'].push(JSON.parse(UTF8ToString($0)));},observedJson.c_str());
    std::map<std::string,WGPUTextureView> views;
    for (auto& texture:metadata.at("textures")) {
        if(!Ui) throw std::runtime_error("Material texture upload requires the shared UI texture service");
        auto owned=std::unique_ptr<WebGpuRuntimeTexture>(static_cast<WebGpuRuntimeTexture*>(Ui->BuildTextureFromCooked(texture.at("path").get<std::string>(),source)));
        views.emplace(texture.at("name").get<std::string>(),owned->View); material->Textures.push_back(std::move(owned));
    }
    material->TextureViews=views;
    material->Binding=Frame.Shaders->CreateBinding(*material->Program,views); material->UniformGeneration=Frame.UniformGeneration;
    Frame.Shaders->Pipeline(*material->Program,false,metadata.at("renderState"));
    return material.release();
}
RendererBackendCapabilityProfile* WebGpuRenderManager3D::GetCapabilityProfile() { return new RendererBackendCapabilityProfile(true, false, false, false, WebGpuLightState::MaximumVisibleLights, 4); }
void WebGpuRenderManager3D::Draw() {
    Core* core = get_OwnerCore();
    if (!core) throw std::runtime_error("WebGPU renderer has no owning core");
    auto* cameras = core->get_ObjectManager()->get_Cameras();
    if (Ui) Ui->BeginFrame();
    for (int32_t i = 0; i < cameras->Count(); ++i) {
        ICamera* camera = (*cameras)[i];
        if (!camera->get_Parent()->get_IsHierarchyEnabled() || !IsCameraVisibleInWindow(camera, get_RenderWindowHandle())) continue;
        auto output=ResolveCameraTarget(Frame,camera);ActiveTarget=output.Pipeline;
        auto viewport = camera->get_Viewport();
        auto width = viewport.Z * output.Width, height = viewport.W * output.Height;
        if (width <= 0 || height <= 0) continue;
        float3 position = camera->get_Parent()->get_Position();
        CameraPosition = float4(position.X,position.Y,position.Z,1);
        auto orientation = camera->get_Parent()->get_Orientation();
        float3 target = position + float4::RotateVector(float3(0, 0, -1), orientation);
        float3 up = float4::RotateVector(float3(0, 1, 0), orientation);
        float4x4 view, projection;
        float4x4::CreateLookAt__ref0_ref1_ref2_out3(position, target, up, view);
        projection = CameraProjectionUtils::CreateProjection(camera, width / height);
        float4x4::Multiply__ref0_ref1_out2(view, projection, ViewProjection);
        Lights.Prepare(core,camera,std::min(get_MaximumVisibleLights(),WebGpuLightState::MaximumVisibleLights));
        ForwardValues=Lights.ForwardValues();
        ShadowPlan.Prepare(camera,Lights,get_ShadowsEnabled()?4:0);
        Shadows->Render(camera,ShadowPlan);
        ShadowValues=Shadows->ShadowValues();
        auto settings = camera->get_ClearSettings();
        auto clear = settings.get_ClearColor();
        WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        color.view = output.Color; color.loadOp = settings.get_ClearColorEnabled() ? WGPULoadOp_Clear : WGPULoadOp_Load; color.storeOp = WGPUStoreOp_Store;
        color.clearValue = {static_cast<double>(clear.X), static_cast<double>(clear.Y), static_cast<double>(clear.Z), static_cast<double>(clear.W)};
        WGPURenderPassDepthStencilAttachment depth = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
        depth.view = output.Depth; depth.depthLoadOp = settings.get_ClearDepthEnabled() ? WGPULoadOp_Clear : WGPULoadOp_Load; depth.depthStoreOp = WGPUStoreOp_Store; depth.depthClearValue = settings.get_ClearDepth();
        WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT; descriptor.colorAttachmentCount = 1; descriptor.colorAttachments = &color; descriptor.depthStencilAttachment = output.Depth?&depth:nullptr;
        Pass = wgpuCommandEncoderBeginRenderPass(Frame.Encoder, &descriptor);
        wgpuRenderPassEncoderSetViewport(Pass, static_cast<float>(viewport.X * output.Width), static_cast<float>(viewport.Y * output.Height), static_cast<float>(width), static_cast<float>(height), 0, 1);
        auto* queue=camera->get_RenderQueue3D();
        if(auto* list=dynamic_cast<RenderList3D*>(queue)) list->VisitCameraOrdered(this, camera);
        else queue->VisitOrdered(this);
        wgpuRenderPassEncoderEnd(Pass); wgpuRenderPassEncoderRelease(Pass); Pass = nullptr;
        if (Ui) Ui->RenderCamera(camera);
    }
}
void WebGpuRenderManager3D::Visit(IDrawable3D* drawable) {
    if (!drawable || !drawable->get_Parent()->get_IsHierarchyEnabled()) return;
    auto* model = static_cast<WebGpuRuntimeModel*>(drawable->get_Model());
    if (!model) throw std::runtime_error("Drawable has no WebGPU model");
    auto world = drawable->get_Parent()->get_WorldTransformMatrix();
    float4x4 mvp, normal;
    float4x4::Multiply__ref0_ref1_out2(world, ViewProjection, mvp);
    if (!float4x4::TryInverseTranspose__ref0_out1(world, normal)) throw std::runtime_error("Mesh has a singular world transform");
    auto* ranges = model->get_Submeshes(); auto* materials = drawable->get_Materials();
    int32_t count = ranges && ranges->Length ? ranges->Length : 1;
    wgpuRenderPassEncoderSetVertexBuffer(Pass, 0, model->Vertices, 0, WGPU_WHOLE_SIZE);
    if (model->Indices) wgpuRenderPassEncoderSetIndexBuffer(Pass, model->Indices, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
    for (int32_t i = 0; i < count; ++i) {
        int materialIndex=ResolveSubmeshMaterialIndex(i,materials?materials->Length:0);
        auto* authored=materialIndex>=0?(*materials)[materialIndex]:nullptr;
        auto* material=authored?dynamic_cast<WebGpuRuntimeMaterial*>(authored->ResolveRootMaterial()):GetMissingMaterial();
        if(!material) throw std::runtime_error("Mesh material requires a WebGPU runtime material");
        if(!material->Binding || material->ShadowResourceGeneration!=Shadows->ResourceGeneration || material->UniformGeneration!=Frame.UniformGeneration) {
            auto views=material->TextureViews; for(auto& pair:Shadows->TextureBindings()) views.insert_or_assign(pair.first,pair.second);
            auto binding=Frame.Shaders->CreateBinding(*material->Program,views); if(material->Binding) wgpuBindGroupRelease(material->Binding); material->Binding=binding; material->ShadowResourceGeneration=Shadows->ResourceGeneration; material->UniformGeneration=Frame.UniformGeneration;
        }
        auto& program=*material->Program; auto constants=material->Artifact.Constants; auto state=material->Artifact.Metadata.at("renderState");
        auto topology=ranges && ranges->Length?(*ranges)[i]->PrimitiveTopology:ModelPrimitiveTopology::TriangleList;
        if(topology==ModelPrimitiveTopology::TriangleList) state["primitiveTopology"]="triangle-list";
        else if(topology==ModelPrimitiveTopology::LineList) state["primitiveTopology"]="line-list";
        else throw std::runtime_error("Submesh uses an unknown engine primitive topology");
        for (auto& binding:program.Artifact.Metadata.at("bindings")) {
            if(binding.at("class")!="constant-buffer") continue;
            std::string name=binding.at("name");
            bool missing=material==MissingMaterial.get();
            if(!missing && name!="TransformBuffer" && name!="ForwardLightBuffer" && name!="ShadowBuffer") continue;
            auto& bytes=constants[name]; bytes.assign(binding.at("sourceLayout").at("size").get<size_t>(),std::byte{});
            for(auto& member:binding.at("sourceLayout").at("members")) {
                std::string field=member.at("name");
                if(missing) {
                    if(field!="worldViewProj") throw std::runtime_error("Unknown required missing-material member: "+field);
                    WriteEngineMatrix(member,reinterpret_cast<const float*>(&mvp),bytes);
                } else if(name=="TransformBuffer") {
                    if(field=="world") WriteEngineMatrix(member,reinterpret_cast<const float*>(&world),bytes);
                    else if(field=="worldViewProj") WriteEngineMatrix(member,reinterpret_cast<const float*>(&mvp),bytes);
                    else if(field=="normalMatrix") WriteEngineMatrix(member,reinterpret_cast<const float*>(&normal),bytes);
                    else if(field=="cameraPosition") FloatMember(member,CameraPosition,bytes);
                    else if(field=="materialFlags") {
                        bool emissive=false; for(auto& t:material->Artifact.Metadata.at("textures")) if(t.at("name")=="EmissiveTexture") emissive=true;
                        FloatMember(member,float4(material->get_ReceivesShadows()?1:0,emissive?1:0,0,0),bytes);
                    }
                    else throw std::runtime_error("Unknown required engine transform member: "+field);
                } else if(name=="ForwardLightBuffer") {
                    bool lit=state.at("lit").get<bool>();
                    if(!ForwardValues.contains(field)) throw std::runtime_error("Unknown required engine light member: "+field);
                    auto& value=ForwardValues.at(field);
                    FloatMember(member,lit?float4(value[0].get<float>(),value[1].get<float>(),value[2].get<float>(),value[3].get<float>()):field=="ambientLightColor"?float4(1,1,1,1):float4(0,0,0,0),bytes);
                } else if(name=="ShadowBuffer") {
                    if(!ShadowValues.contains(field)) throw std::runtime_error("Unknown required engine shadow member: "+field);
                    auto& value=ShadowValues.at(field);
                    if(value.size()==16) { auto matrix=value.get<std::vector<float>>(); WriteEngineMatrix(member,matrix.data(),bytes); }
                    else FloatMember(member,float4(value[0].get<float>(),value[1].get<float>(),value[2].get<float>(),value[3].get<float>()),bytes);
                }
            }
        }
        wgpuRenderPassEncoderSetPipeline(Pass,Frame.Shaders->Pipeline(program,false,state,ActiveTarget));
        Frame.Shaders->Bind(Pass,program,material->Binding,constants);
        uint32_t start = ranges && ranges->Length ? (*ranges)[i]->IndexStart : 0;
        uint32_t indices = ranges && ranges->Length ? (*ranges)[i]->IndexCount : model->IndexCount;
        if (model->Indices) wgpuRenderPassEncoderDrawIndexed(Pass, indices, 1, start, 0, 0);
        else wgpuRenderPassEncoderDraw(Pass, ranges && ranges->Length ? indices : model->VertexCount, 1, start, 0);
    }
}
}
