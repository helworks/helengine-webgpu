#include "webgpu_render_manager_3d.hpp"
#include "webgpu_render_manager_2d.hpp"
#include "webgpu_runtime_model.hpp"
#include "webgpu_runtime_material.hpp"
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
WebGpuRenderManager3D::WebGpuRenderManager3D(WebGpuFrame& frame) : Frame(frame) {
    WGPUBindGroupLayoutEntry entry = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    entry.binding = 0; entry.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    entry.buffer.type = WGPUBufferBindingType_Uniform; entry.buffer.hasDynamicOffset = true; entry.buffer.minBindingSize = 176;
    WGPUBindGroupLayoutDescriptor layout = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    layout.entryCount = 1; layout.entries = &entry; Layout = wgpuDeviceCreateBindGroupLayout(Frame.Device(), &layout);
    WGPUPipelineLayoutDescriptor pipelineLayout = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pipelineLayout.bindGroupLayoutCount = 1; pipelineLayout.bindGroupLayouts = &Layout;
    WGPUPipelineLayout pipelineLayoutHandle = wgpuDeviceCreatePipelineLayout(Frame.Device(), &pipelineLayout);
    WGPUBindGroupEntry binding = WGPU_BIND_GROUP_ENTRY_INIT;
    binding.binding = 0; binding.buffer = Frame.Uniforms; binding.size = 176;
    WGPUBindGroupDescriptor bind = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bind.layout = Layout; bind.entryCount = 1; bind.entries = &binding; Binding = wgpuDeviceCreateBindGroup(Frame.Device(), &bind);
    WGPUShaderModule shader = Frame.Shader("basic_mesh.wgsl");
    WGPUVertexAttribute attributes[2] = {WGPU_VERTEX_ATTRIBUTE_INIT, WGPU_VERTEX_ATTRIBUTE_INIT};
    attributes[0].format = WGPUVertexFormat_Float32x3; attributes[0].shaderLocation = 0;
    attributes[1].format = WGPUVertexFormat_Float32x3; attributes[1].shaderLocation = 1; attributes[1].offset = 12;
    WGPUVertexBufferLayout vertices = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    vertices.arrayStride = 24; vertices.attributeCount = 2; vertices.attributes = attributes; vertices.stepMode = WGPUVertexStepMode_Vertex;
    WGPUColorTargetState color = WGPU_COLOR_TARGET_STATE_INIT; color.format = Frame.Format();
    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT; fragment.module = shader; fragment.entryPoint = {"fs_main", WGPU_STRLEN}; fragment.targetCount = 1; fragment.targets = &color;
    WGPUDepthStencilState depth = WGPU_DEPTH_STENCIL_STATE_INIT;
    depth.format = WGPUTextureFormat_Depth24Plus; depth.depthWriteEnabled = WGPUOptionalBool_True; depth.depthCompare = WGPUCompareFunction_Less;
    WGPURenderPipelineDescriptor pipeline = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pipeline.layout = pipelineLayoutHandle; pipeline.vertex.module = shader; pipeline.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
    pipeline.vertex.bufferCount = 1; pipeline.vertex.buffers = &vertices; pipeline.fragment = &fragment; pipeline.depthStencil = &depth;
    pipeline.primitive.topology = WGPUPrimitiveTopology_TriangleList; pipeline.primitive.frontFace = WGPUFrontFace_CCW; pipeline.primitive.cullMode = WGPUCullMode_Back;
    Pipeline = wgpuDeviceCreateRenderPipeline(Frame.Device(), &pipeline);
    wgpuShaderModuleRelease(shader); wgpuPipelineLayoutRelease(pipelineLayoutHandle);
}
WebGpuRenderManager3D::~WebGpuRenderManager3D() {
    if (Binding) wgpuBindGroupRelease(Binding);
    if (Layout) wgpuBindGroupLayoutRelease(Layout);
    if (Pipeline) wgpuRenderPipelineRelease(Pipeline);
}
RuntimeModel* WebGpuRenderManager3D::BuildModelFromRaw(ModelAsset* data) {
    if (!data || !data->Positions || !data->Normals || !data->Positions->Length || data->Positions->Length != data->Normals->Length) throw std::runtime_error("WebGPU mesh requires matching nonempty positions and normals");
    std::unique_ptr<WebGpuRuntimeModel> model(new WebGpuRuntimeModel());
    model->set_Id(data->get_Id()); model->SetBounds(data->BoundsMin, data->BoundsMax);
    struct Vertex { float Position[3], Normal[3]; };
    std::vector<Vertex> vertices(data->Positions->Length);
    for (int32_t i = 0; i < data->Positions->Length; ++i) {
        auto p = (*data->Positions)[i]; auto n = (*data->Normals)[i];
        vertices[i] = {{p.X, p.Y, p.Z}, {n.X, n.Y, n.Z}};
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
RuntimeModel* WebGpuRenderManager3D::BuildModelFromCooked(std::string path, IContentStreamSource* source) {
    std::unique_ptr<Stream> stream(source->OpenRead(path));
    std::unique_ptr<Asset> asset(AssetSerializer::Deserialize(stream.get()));
    auto* model = dynamic_cast<ModelAsset*>(asset.get());
    if (!model) throw std::runtime_error("Prepared model has the wrong asset type: " + path);
    return BuildModelFromRaw(model);
}
RuntimeMaterial* WebGpuRenderManager3D::BuildMaterialFromCooked(PlatformMaterialAsset* data) {
    if (!data || data->RendererFamilyId != "webgpu") throw std::runtime_error("Material was not cooked for WebGPU");
    if (data->DoubleSided || data->UseVertexColor || !data->TextureRelativePath.empty()) throw std::runtime_error("WebGPU basic 3D material does not support the requested extended fields");
    auto* material = new WebGpuRuntimeMaterial(); material->set_Id(data->get_Id());
    material->Color = float4(data->BaseColorR / 255.f, data->BaseColorG / 255.f, data->BaseColorB / 255.f, data->BaseColorA / 255.f);
    material->Lit = data->Lit;
    material->set_CastsShadows(false); material->set_ReceivesShadows(false);
    return material;
}
RuntimeMaterial* WebGpuRenderManager3D::BuildMaterialFromCooked(std::string path, IContentStreamSource* source) {
    std::unique_ptr<Stream> stream(source->OpenRead(path)); std::unique_ptr<Asset> asset(AssetSerializer::Deserialize(stream.get()));
    auto* material = dynamic_cast<PlatformMaterialAsset*>(asset.get());
    if (!material) throw std::runtime_error("Prepared material has the wrong asset type: " + path);
    return BuildMaterialFromCooked(material);
}
RendererBackendCapabilityProfile* WebGpuRenderManager3D::GetCapabilityProfile() { return new RendererBackendCapabilityProfile(true, false, false, false, 1, 0); }
void WebGpuRenderManager3D::Draw() {
    Core* core = get_OwnerCore();
    if (!core) throw std::runtime_error("WebGPU renderer has no owning core");
    auto* cameras = core->get_ObjectManager()->get_Cameras();
    if (Ui) Ui->BeginFrame();
    for (int32_t i = 0; i < cameras->Count(); ++i) {
        ICamera* camera = (*cameras)[i];
        if (!camera->get_Parent()->get_IsHierarchyEnabled() || !IsCameraVisibleInWindow(camera, get_RenderWindowHandle())) continue;
        if (camera->get_RenderTarget()) throw std::runtime_error("Offscreen camera targets are unsupported in the initial WebGPU renderer");
        auto viewport = camera->get_Viewport();
        float width = viewport.Z * Frame.Width(), height = viewport.W * Frame.Height();
        if (width <= 0 || height <= 0) continue;
        float3 position = camera->get_Parent()->get_Position();
        auto orientation = camera->get_Parent()->get_Orientation();
        float3 target = position + float4::RotateVector(float3(0, 0, -1), orientation);
        float3 up = float4::RotateVector(float3(0, 1, 0), orientation);
        float4x4 view, projection;
        float4x4::CreateLookAt__ref0_ref1_ref2_out3(position, target, up, view);
        projection = CameraProjectionUtils::CreateProjection(camera, width / height);
        float4x4::Multiply__ref0_ref1_out2(view, projection, ViewProjection);
        LightDirection = float4(0, 0, 1, 0); LightColor = float4(0, 0, 0, 0);
        auto* lights = core->get_ObjectManager()->get_DirectionalLights();
        for (int32_t lightIndex = 0; lightIndex < lights->Count(); ++lightIndex) {
            auto* light = (*lights)[lightIndex];
            if (!light->get_Parent()->get_IsHierarchyEnabled() || !(light->get_Parent()->get_LayerMask() & camera->get_LayerMask())) continue;
            auto direction = float4::RotateVector(float3(0, 0, -1), light->get_Parent()->get_Orientation());
            LightDirection = float4(direction.X, direction.Y, direction.Z, 1);
            LightColor = light->get_Color() * light->get_Intensity(); break;
        }
        auto settings = camera->get_ClearSettings();
        auto clear = settings.get_ClearColor();
        WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        color.view = Frame.Color; color.loadOp = settings.get_ClearColorEnabled() ? WGPULoadOp_Clear : WGPULoadOp_Load; color.storeOp = WGPUStoreOp_Store;
        color.clearValue = {clear.X, clear.Y, clear.Z, clear.W};
        WGPURenderPassDepthStencilAttachment depth = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
        depth.view = Frame.Depth; depth.depthLoadOp = settings.get_ClearDepthEnabled() ? WGPULoadOp_Clear : WGPULoadOp_Load; depth.depthStoreOp = WGPUStoreOp_Store; depth.depthClearValue = settings.get_ClearDepth();
        WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT; descriptor.colorAttachmentCount = 1; descriptor.colorAttachments = &color; descriptor.depthStencilAttachment = &depth;
        Pass = wgpuCommandEncoderBeginRenderPass(Frame.Encoder, &descriptor);
        wgpuRenderPassEncoderSetPipeline(Pass, Pipeline);
        wgpuRenderPassEncoderSetViewport(Pass, viewport.X * Frame.Width(), viewport.Y * Frame.Height(), width, height, 0, 1);
        camera->get_RenderQueue3D()->VisitOrdered(this);
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
        if (!materials || i >= materials->Length || !(*materials)[i]) throw std::runtime_error("Mesh submesh has no cooked material");
        auto* material = static_cast<WebGpuRuntimeMaterial*>((*materials)[i]->ResolveRootMaterial());
        struct Constants { float4x4 Mvp, Normal; float4 Color, Direction, Light; } constants {mvp, normal, material->Color, LightDirection, LightColor};
        static_assert(sizeof(Constants) == 176, "WGSL uniform packing must remain explicit");
        if (!material->Lit) constants.Direction.W = 0;
        uint32_t offset = Frame.Uniform(&constants, sizeof(constants));
        wgpuRenderPassEncoderSetBindGroup(Pass, 0, Binding, 1, &offset);
        uint32_t start = ranges && ranges->Length ? (*ranges)[i]->IndexStart : 0;
        uint32_t indices = ranges && ranges->Length ? (*ranges)[i]->IndexCount : model->IndexCount;
        if (model->Indices) wgpuRenderPassEncoderDrawIndexed(Pass, indices, 1, start, 0, 0);
        else wgpuRenderPassEncoderDraw(Pass, ranges && ranges->Length ? indices : model->VertexCount, 1, start, 0);
    }
}
}
