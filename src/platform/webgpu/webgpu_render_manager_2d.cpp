#include "webgpu_render_manager_2d.hpp"
#include "webgpu_runtime_texture.hpp"
#include "TextureAsset.hpp"
#include "AssetSerializer.hpp"
#include "IContentStreamSource.hpp"
#include "RenderCommandListBuilder2D.hpp"
#include "RenderCommandList2D.hpp"
#include "RenderCommand2DType.hpp"
#include "Batch2DGeometry.hpp"
#include "Batch2DCameraProjection.hpp"
#include "Batch2DBuffer.hpp"
#include "ICamera.hpp"
#include "IRenderQueue2D.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace helengine::webgpu {
WebGpuRenderManager2D::WebGpuRenderManager2D(WebGpuFrame& frame) : Frame(frame), Writer(this, 512), Emitter(&Writer, this), Commands(new RenderCommandListBuilder2D()) {
    WGPUBindGroupLayoutEntry entries[3] = {WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT, WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT, WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT};
    entries[0].binding = 0; entries[0].visibility = WGPUShaderStage_Vertex; entries[0].buffer.type = WGPUBufferBindingType_Uniform; entries[0].buffer.hasDynamicOffset = true; entries[0].buffer.minBindingSize = 64;
    entries[1].binding = 1; entries[1].visibility = WGPUShaderStage_Fragment; entries[1].sampler.type = WGPUSamplerBindingType_Filtering;
    entries[2].binding = 2; entries[2].visibility = WGPUShaderStage_Fragment; entries[2].texture.sampleType = WGPUTextureSampleType_Float; entries[2].texture.viewDimension = WGPUTextureViewDimension_2D;
    WGPUBindGroupLayoutDescriptor layout = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT; layout.entryCount = 3; layout.entries = entries;
    Layout = wgpuDeviceCreateBindGroupLayout(Frame.Device(), &layout);
    WGPUSamplerDescriptor sampler = WGPU_SAMPLER_DESCRIPTOR_INIT; sampler.magFilter = WGPUFilterMode_Linear; sampler.minFilter = WGPUFilterMode_Linear;
    Sampler = wgpuDeviceCreateSampler(Frame.Device(), &sampler);
    WGPUPipelineLayoutDescriptor pipelineLayout = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT; pipelineLayout.bindGroupLayoutCount = 1; pipelineLayout.bindGroupLayouts = &Layout;
    WGPUPipelineLayout pipelineLayoutHandle = wgpuDeviceCreatePipelineLayout(Frame.Device(), &pipelineLayout);
    WGPUShaderModule shader = Frame.Shader("ui.wgsl");
    WGPUVertexAttribute attributes[6];
    for (uint32_t i = 0; i < 6; ++i) { attributes[i] = WGPU_VERTEX_ATTRIBUTE_INIT; attributes[i].format = WGPUVertexFormat_Float32x4; attributes[i].shaderLocation = i; attributes[i].offset = i * 16; }
    WGPUVertexBufferLayout vertices = WGPU_VERTEX_BUFFER_LAYOUT_INIT; vertices.arrayStride = 96; vertices.attributeCount = 6; vertices.attributes = attributes; vertices.stepMode = WGPUVertexStepMode_Vertex;
    WGPUBlendState blend = WGPU_BLEND_STATE_INIT;
    blend.color.srcFactor = WGPUBlendFactor_SrcAlpha; blend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend.alpha.srcFactor = WGPUBlendFactor_One; blend.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    WGPUColorTargetState color = WGPU_COLOR_TARGET_STATE_INIT; color.format = Frame.Format(); color.blend = &blend;
    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT; fragment.module = shader; fragment.targetCount = 1; fragment.targets = &color;
    WGPURenderPipelineDescriptor pipeline = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pipeline.layout = pipelineLayoutHandle; pipeline.vertex.module = shader; pipeline.vertex.entryPoint = {"vs_main", WGPU_STRLEN}; pipeline.vertex.bufferCount = 1; pipeline.vertex.buffers = &vertices; pipeline.fragment = &fragment;
    pipeline.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    fragment.entryPoint = {"fs_texture", WGPU_STRLEN}; Textured = wgpuDeviceCreateRenderPipeline(Frame.Device(), &pipeline);
    fragment.entryPoint = {"fs_rounded", WGPU_STRLEN}; Rounded = wgpuDeviceCreateRenderPipeline(Frame.Device(), &pipeline);
    wgpuShaderModuleRelease(shader); wgpuPipelineLayoutRelease(pipelineLayoutHandle);
    Vertices = Frame.Buffer(nullptr, 4 * 1024 * 1024, WGPUBufferUsage_Vertex); Indices = Frame.Buffer(nullptr, 1024 * 1024, WGPUBufferUsage_Index);
    TextureAsset white; white.Width = 1; white.Height = 1; white.ColorFormat = TextureAssetColorFormat::Rgba32; white.Colors = new Array<uint8_t>(4);
    for (int i = 0; i < 4; ++i) (*white.Colors)[i] = 255;
    White.reset(static_cast<WebGpuRuntimeTexture*>(BuildTextureFromRaw(&white))); white.Dispose();
}
WebGpuRenderManager2D::~WebGpuRenderManager2D() {
    Writer.Dispose(); White.reset();
    if (Vertices) wgpuBufferRelease(Vertices); if (Indices) wgpuBufferRelease(Indices);
    if (Textured) wgpuRenderPipelineRelease(Textured); if (Rounded) wgpuRenderPipelineRelease(Rounded);
    if (Layout) wgpuBindGroupLayoutRelease(Layout); if (Sampler) wgpuSamplerRelease(Sampler);
}
RuntimeTexture* WebGpuRenderManager2D::BuildTextureFromRaw(TextureAsset* data) {
    if (!data || !data->Width || !data->Height || data->ColorFormat != TextureAssetColorFormat::Rgba32 || !data->Colors || data->Colors->Length != data->Width * data->Height * 4) throw std::runtime_error("WebGPU texture requires complete RGBA32 data");
    std::unique_ptr<WebGpuRuntimeTexture> texture(new WebGpuRuntimeTexture());
    texture->set_Id(data->get_Id()); texture->Width = data->Width; texture->Height = data->Height; texture->IsEngineOwned = data->IsEngineOwned;
    WGPUTextureDescriptor descriptor = WGPU_TEXTURE_DESCRIPTOR_INIT; descriptor.size = {data->Width, data->Height, 1}; descriptor.format = WGPUTextureFormat_RGBA8Unorm;
    descriptor.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    texture->Texture = wgpuDeviceCreateTexture(Frame.Device(), &descriptor); texture->View = wgpuTextureCreateView(texture->Texture, nullptr);
    WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT; destination.texture = texture->Texture;
    WGPUTexelCopyBufferLayout bytes = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT; bytes.bytesPerRow = data->Width * 4; bytes.rowsPerImage = data->Height;
    WGPUExtent3D extent {data->Width, data->Height, 1};
    wgpuQueueWriteTexture(Frame.Queue, &destination, data->Colors->Data, data->Colors->Length, &bytes, &extent);
    WGPUBindGroupEntry entries[3] = {WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT, WGPU_BIND_GROUP_ENTRY_INIT};
    entries[0].binding = 0; entries[0].buffer = Frame.Uniforms; entries[0].size = 64;
    entries[1].binding = 1; entries[1].sampler = Sampler; entries[2].binding = 2; entries[2].textureView = texture->View;
    WGPUBindGroupDescriptor bind = WGPU_BIND_GROUP_DESCRIPTOR_INIT; bind.layout = Layout; bind.entryCount = 3; bind.entries = entries;
    texture->Binding = wgpuDeviceCreateBindGroup(Frame.Device(), &bind);
    return texture.release();
}
RuntimeTexture* WebGpuRenderManager2D::BuildTextureFromCooked(std::string path, IContentStreamSource* source) {
    std::unique_ptr<Stream> stream(source->OpenRead(path)); std::unique_ptr<Asset> asset(AssetSerializer::Deserialize(stream.get()));
    auto* texture = dynamic_cast<TextureAsset*>(asset.get()); if (!texture) throw std::runtime_error("Prepared texture has the wrong asset type: " + path);
    return BuildTextureFromRaw(texture);
}
void WebGpuRenderManager2D::UpdateTextureRegionCore(RuntimeTexture* runtime, int32_t x, int32_t y, int32_t width, int32_t height, Array<uint8_t>* rgba8, int32_t sourceRowPitch) {
    auto* texture = static_cast<WebGpuRuntimeTexture*>(runtime);
    WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    destination.texture = texture->Texture; destination.origin = {static_cast<uint32_t>(x), static_cast<uint32_t>(y), 0};
    WGPUTexelCopyBufferLayout layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
    layout.bytesPerRow = sourceRowPitch; layout.rowsPerImage = height;
    WGPUExtent3D extent {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    wgpuQueueWriteTexture(Frame.Queue, &destination, rgba8->Data, rgba8->Length, &layout, &extent);
}
void WebGpuRenderManager2D::BeginFrame() { VertexCursor = 0; IndexCursor = 0; }
void WebGpuRenderManager2D::SetCamera(float4x4 projection, float4 viewport) { Projection = projection; Viewport = viewport; }
Batch2DRun WebGpuRenderManager2D::Run(Batch2DVariant variant, RuntimeTexture* texture) {
    auto clip = Clips.empty() ? Viewport : Clips.back();
    return Batch2DRun(variant, texture, static_cast<int32_t>(clip.X), static_cast<int32_t>(clip.Y), static_cast<int32_t>(clip.Z), static_cast<int32_t>(clip.W));
}
void WebGpuRenderManager2D::DrawSprite(ISpriteDrawable2D* sprite) { Emitter.EmitSprite(sprite, Run(Batch2DVariant::RoundedShape, nullptr)); }
void WebGpuRenderManager2D::DrawText(ITextDrawable2D* text) { Emitter.EmitText(text, Run(Batch2DVariant::RoundedShape, nullptr)); }
void WebGpuRenderManager2D::DrawRoundedRect(IRoundedRectDrawable2D* shape) { Emitter.EmitRoundedRect(shape, Run(Batch2DVariant::RoundedShape, nullptr)); }
void WebGpuRenderManager2D::Submit(Batch2DBuffer* buffer, Batch2DRun run) {
    static_assert(sizeof(Batch2DVertex) == 96, "GPU vertex packing must match shared batch fields");
    if (!run.get_ScissorWidth() || !run.get_ScissorHeight()) return;
    uint64_t vertexBytes = buffer->get_VertexCount() * 96, indexBytes = buffer->get_IndexCount() * 2;
    if (VertexCursor + vertexBytes > 4 * 1024 * 1024 || IndexCursor + indexBytes > 1024 * 1024) throw std::runtime_error("WebGPU UI frame upload budget exceeded");
    wgpuQueueWriteBuffer(Frame.Queue, Vertices, VertexCursor, buffer->Vertices->Data, vertexBytes);
    wgpuQueueWriteBuffer(Frame.Queue, Indices, IndexCursor, buffer->Indices->Data, indexBytes);
    auto* texture = run.get_Variant() == Batch2DVariant::RoundedShape ? White.get() : static_cast<WebGpuRuntimeTexture*>(run.get_Texture());
    if (!texture || texture->get_IsDisposed()) throw std::runtime_error("UI batch references a missing or disposed texture");
    uint32_t offset = Frame.Uniform(&Projection, sizeof(Projection));
    wgpuRenderPassEncoderSetPipeline(Pass, run.get_Variant() == Batch2DVariant::RoundedShape ? Rounded : Textured);
    wgpuRenderPassEncoderSetBindGroup(Pass, 0, texture->Binding, 1, &offset);
    wgpuRenderPassEncoderSetVertexBuffer(Pass, 0, Vertices, VertexCursor, vertexBytes);
    wgpuRenderPassEncoderSetIndexBuffer(Pass, Indices, WGPUIndexFormat_Uint16, IndexCursor, indexBytes);
    wgpuRenderPassEncoderSetScissorRect(Pass, run.get_ScissorX(), run.get_ScissorY(), run.get_ScissorWidth(), run.get_ScissorHeight());
    wgpuRenderPassEncoderDrawIndexed(Pass, buffer->get_IndexCount(), 1, 0, 0, 0);
    VertexCursor += vertexBytes; IndexCursor += (indexBytes + 3) & ~uint64_t(3);
}
void WebGpuRenderManager2D::RenderCamera(ICamera* camera) {
    auto viewport = camera->get_Viewport();
    Viewport = float4(viewport.X * Frame.Width(), viewport.Y * Frame.Height(), viewport.Z * Frame.Width(), viewport.W * Frame.Height());
    auto* commands = Commands->Build(camera->get_RenderQueue2D());
    if (!commands || !commands->get_Count()) return;
    WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    color.view = Frame.Color; color.loadOp = WGPULoadOp_Load; color.storeOp = WGPUStoreOp_Store;
    WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT; descriptor.colorAttachmentCount = 1; descriptor.colorAttachments = &color;
    Pass = wgpuCommandEncoderBeginRenderPass(Frame.Encoder, &descriptor);
    wgpuRenderPassEncoderSetViewport(Pass, Viewport.X, Viewport.Y, Viewport.Z, Viewport.W, 0, 1);
    Clips.clear(); Writer.ConfigureCamera(Batch2DCameraProjection::Create(Viewport), Viewport);
    for (int32_t i = 0; i < commands->get_Count(); ++i) {
        auto type = commands->GetCommandType(i);
        if (type == RenderCommand2DType::ClipPush) {
            auto rect = commands->GetClipPushRect(commands->GetClipPushPayloadIndex(i)); auto parent = Clips.empty() ? Viewport : Clips.back();
            float left = std::max(parent.X, rect.X), top = std::max(parent.Y, rect.Y);
            float right = std::min(parent.X + parent.Z, rect.X + rect.Z), bottom = std::min(parent.Y + parent.W, rect.Y + rect.W);
            Clips.push_back(float4(left, top, std::max(0.f, right - left), std::max(0.f, bottom - top))); continue;
        }
        if (type == RenderCommand2DType::ClipPop) { if (Clips.empty()) throw std::runtime_error("UI clip stack underflow"); Clips.pop_back(); continue; }
        Batch2DVertex a, b, c, d;
        if (type == RenderCommand2DType::RoundedRect) {
            int p = commands->GetRoundedRectPayloadIndex(i); auto bounds = commands->GetRoundedRectBounds(p); int corners = static_cast<int>(commands->GetRoundedRectCorners(p));
            Batch2DGeometry::CreateRoundedQuad__out11_out12_out13_out14(bounds.X, bounds.Y, bounds.Z, bounds.W, 0, 0, commands->GetRoundedRectRadius(p), commands->GetRoundedRectBorderThickness(p), float4(corners & 1 ? 1 : 0, corners & 2 ? 1 : 0, corners & 4 ? 1 : 0, corners & 8 ? 1 : 0), Batch2DGeometry::NormalizeColor(commands->GetRoundedRectFillColor(p)), Batch2DGeometry::NormalizeColor(commands->GetRoundedRectBorderColor(p)), a, b, c, d);
            Writer.AppendQuad(Run(Batch2DVariant::RoundedShape, nullptr), a, b, c, d);
        } else if (type == RenderCommand2DType::TexturedQuad || type == RenderCommand2DType::GlyphQuad) {
            bool glyph = type == RenderCommand2DType::GlyphQuad;
            int p = glyph ? commands->GetGlyphQuadPayloadIndex(i) : commands->GetTexturedQuadPayloadIndex(i);
            auto bounds = glyph ? commands->GetGlyphQuadBounds(p) : commands->GetTexturedQuadBounds(p);
            auto source = glyph ? commands->GetGlyphQuadSourceRect(p) : commands->GetTexturedQuadSourceRect(p);
            auto tint = glyph ? commands->GetGlyphQuadColor(p) : commands->GetTexturedQuadColor(p);
            auto* texture = glyph ? commands->GetGlyphQuadTexture(p) : commands->GetTexturedQuadTexture(p);
            Batch2DGeometry::CreateQuad__out8_out9_out10_out11(bounds.X, bounds.Y, bounds.Z, bounds.W, 0, glyph ? 0 : commands->GetTexturedQuadRotation(p), source, Batch2DGeometry::NormalizeColor(tint), a, b, c, d);
            Writer.AppendQuad(Run(Batch2DVariant::Textured, texture), a, b, c, d);
        } else throw std::runtime_error("Unknown engine UI command");
    }
    Writer.Flush(); wgpuRenderPassEncoderEnd(Pass); wgpuRenderPassEncoderRelease(Pass); Pass = nullptr;
}
}
