#include "webgpu_render_manager_2d.hpp"
#include "webgpu_runtime_texture.hpp"
#include "webgpu_material_bindings.hpp"
#include "webgpu_render_target.hpp"
#include "webgpu_camera_target.hpp"
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
    ArtifactJson selection = {{"shaderAssetId","SpriteBatchShader"},{"vertexProgram","SpriteBatchShader.vs"},{"pixelProgram","SpriteBatchShader.ps"},{"variant","Textured"}};
    TexturedProgram = Frame.Shaders->Load(ShaderArtifactPath(selection));
    selection["variant"] = "RoundedShape"; RoundedProgram = Frame.Shaders->Load(ShaderArtifactPath(selection));
    ArtifactJson state = {{"alphaMode","alpha-blend"},{"doubleSided",true},{"useVertexColor",true},{"lit",false},{"depthTest",false},{"depthWrite",false}};
    Textured = Frame.Shaders->Pipeline(*TexturedProgram,true,state); Rounded = Frame.Shaders->Pipeline(*RoundedProgram,true,state);
    RoundedBinding = Frame.Shaders->CreateBinding(*RoundedProgram,{}); UniformGeneration=Frame.UniformGeneration;
    Vertices = Frame.Buffer(nullptr, 4 * 1024 * 1024, WGPUBufferUsage_Vertex); Indices = Frame.Buffer(nullptr, 1024 * 1024, WGPUBufferUsage_Index);
    TextureAsset white; white.Width = 1; white.Height = 1; white.ColorFormat = TextureAssetColorFormat::Rgba32; white.Colors = new Array<uint8_t>(4);
    for (int i = 0; i < 4; ++i) (*white.Colors)[i] = 255;
    White.reset(static_cast<WebGpuRuntimeTexture*>(BuildTextureFromRaw(&white))); white.Dispose();
}
WebGpuRenderManager2D::~WebGpuRenderManager2D() {
    Writer.Dispose(); White.reset();
    if (Vertices) wgpuBufferRelease(Vertices); if (Indices) wgpuBufferRelease(Indices);
    if (RoundedBinding) wgpuBindGroupRelease(RoundedBinding);
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
    texture->Binding = Frame.Shaders->CreateBinding(*TexturedProgram,{{"BatchTexture",texture->View}}); texture->UniformGeneration=Frame.UniformGeneration;
    return texture.release();
}
RuntimeTexture* WebGpuRenderManager2D::BuildTextureFromCooked(HeCppString path, IContentStreamSource* source) {
    std::unique_ptr<Stream> stream(source->OpenRead(path)); std::unique_ptr<Asset> asset(AssetSerializer::Deserialize(stream.get()));
    auto* texture = dynamic_cast<TextureAsset*>(asset.get()); if (!texture) throw std::runtime_error("Prepared texture has the wrong asset type: " + path.raw());
    return BuildTextureFromRaw(texture);
}
void WebGpuRenderManager2D::UpdateTextureRegionCore(RuntimeTexture* runtime, int32_t x, int32_t y, int32_t width, int32_t height, Array<uint8_t>* rgba8, int32_t sourceRowPitch) {
    auto* texture = dynamic_cast<WebGpuRuntimeTexture*>(runtime);
    auto* target = dynamic_cast<WebGpuRenderTarget*>(runtime);
    if(!texture && !target) throw std::runtime_error("Texture update requires a WebGPU runtime texture or target");
    WGPUTexelCopyTextureInfo destination = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    destination.texture = texture?texture->Texture:target->Texture; destination.origin = {static_cast<uint32_t>(x), static_cast<uint32_t>(y), 0};
    WGPUTexelCopyBufferLayout layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
    layout.bytesPerRow = sourceRowPitch; layout.rowsPerImage = height;
    WGPUExtent3D extent {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    wgpuQueueWriteTexture(Frame.Queue, &destination, rgba8->Data, rgba8->Length, &layout, &extent);
}
void WebGpuRenderManager2D::BeginFrame() {
    VertexCursor = 0; IndexCursor = 0;
    if(UniformGeneration!=Frame.UniformGeneration) {
        auto binding=Frame.Shaders->CreateBinding(*RoundedProgram,{});
        wgpuBindGroupRelease(RoundedBinding); RoundedBinding=binding; UniformGeneration=Frame.UniformGeneration;
    }
}
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
    RuntimeTexture* texture = run.get_Variant() == Batch2DVariant::RoundedShape ? White.get() : run.get_Texture();
    if (!texture || texture->get_IsDisposed()) throw std::runtime_error("UI batch references a missing or disposed texture");
    auto& program = run.get_Variant() == Batch2DVariant::RoundedShape ? *RoundedProgram : *TexturedProgram;
    auto& reflected = program.Artifact.Metadata.at("bindings").at(0);
    std::vector<std::byte> camera(reflected.at("sourceLayout").at("size").get<size_t>());
    WriteEngineMatrix(reflected.at("sourceLayout").at("members").at(0),reinterpret_cast<const float*>(&Projection),camera);
    WGPUBindGroup binding=RoundedBinding;
    if(run.get_Variant()!=Batch2DVariant::RoundedShape) {
        if(auto* ordinary=dynamic_cast<WebGpuRuntimeTexture*>(texture)) {
            if(ordinary->UniformGeneration!=Frame.UniformGeneration) {
                auto replacement=Frame.Shaders->CreateBinding(program,{{"BatchTexture",ordinary->View}});
                if(ordinary->Binding) wgpuBindGroupRelease(ordinary->Binding);
                ordinary->Binding=replacement; ordinary->UniformGeneration=Frame.UniformGeneration;
            }
            binding=ordinary->Binding;
        }
        else if(auto* target=dynamic_cast<WebGpuRenderTarget*>(texture)) {
            if(!target->get_CanSampleAsTexture() || !target->View) throw std::runtime_error("UI batch references an unsampleable camera target");
            if(!target->Binding || target->UniformGeneration!=Frame.UniformGeneration) {
                auto replacement=Frame.Shaders->CreateBinding(program,{{"BatchTexture",target->View}});
                if(target->Binding) wgpuBindGroupRelease(target->Binding);
                target->Binding=replacement; target->UniformGeneration=Frame.UniformGeneration;
            }
            binding=target->Binding;
        } else throw std::runtime_error("UI batch requires a WebGPU runtime texture or target");
    }
    auto state=ArtifactJson({{"alphaMode","alpha-blend"},{"doubleSided",true},{"depthTest",false},{"depthWrite",false},{"useVertexColor",true},{"lit",false}});
    wgpuRenderPassEncoderSetPipeline(Pass,Frame.Shaders->Pipeline(program,true,state,ActiveTarget));
    Frame.Shaders->Bind(Pass,program,binding,{{"BatchCameraBuffer",std::move(camera)}});
    wgpuRenderPassEncoderSetVertexBuffer(Pass, 0, Vertices, VertexCursor, vertexBytes);
    wgpuRenderPassEncoderSetIndexBuffer(Pass, Indices, WGPUIndexFormat_Uint16, IndexCursor, indexBytes);
    wgpuRenderPassEncoderSetScissorRect(Pass, run.get_ScissorX(), run.get_ScissorY(), run.get_ScissorWidth(), run.get_ScissorHeight());
    wgpuRenderPassEncoderDrawIndexed(Pass, buffer->get_IndexCount(), 1, 0, 0, 0);
    VertexCursor += vertexBytes; IndexCursor += (indexBytes + 3) & ~uint64_t(3);
}
void WebGpuRenderManager2D::RenderCamera(ICamera* camera) {
    auto viewport = camera->get_Viewport();
    auto output=ResolveCameraTarget(Frame,camera);ActiveTarget=output.Pipeline;ActiveTarget.DepthFormat=WGPUTextureFormat_Undefined;
    Viewport = float4(viewport.X * output.Width, viewport.Y * output.Height, viewport.Z * output.Width, viewport.W * output.Height);
    auto* commands = Commands->Build(camera->get_RenderQueue2D());
    if (!commands || !commands->get_Count()) return;
    WGPURenderPassColorAttachment color = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    color.view = output.Color; color.loadOp = WGPULoadOp_Load; color.storeOp = WGPUStoreOp_Store;
    WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT; descriptor.colorAttachmentCount = 1; descriptor.colorAttachments = &color;
    Pass = wgpuCommandEncoderBeginRenderPass(Frame.Encoder, &descriptor);
    // WebGPU viewport arguments are fixed float32 values at the native API boundary.
    wgpuRenderPassEncoderSetViewport(Pass, static_cast<float>(Viewport.X), static_cast<float>(Viewport.Y), static_cast<float>(Viewport.Z), static_cast<float>(Viewport.W), 0, 1);
    Clips.clear(); Writer.ConfigureCamera(Batch2DCameraProjection::Create(Viewport), Viewport);
    for (int32_t i = 0; i < commands->get_Count(); ++i) {
        auto type = commands->GetCommandType(i);
        if (type == RenderCommand2DType::ClipPush) {
            auto rect = commands->GetClipPushRect(commands->GetClipPushPayloadIndex(i)); auto parent = Clips.empty() ? Viewport : Clips.back();
            auto left = std::max(parent.X, rect.X), top = std::max(parent.Y, rect.Y);
            Real right = std::min(parent.X + parent.Z, rect.X + rect.Z), bottom = std::min(parent.Y + parent.W, rect.Y + rect.W);
            Real clipWidth = right > left ? Real(right - left) : Real(0), clipHeight = bottom > top ? Real(bottom - top) : Real(0);
            Clips.push_back(float4(left, top, clipWidth, clipHeight)); continue;
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
            auto source = glyph ? commands->GetGlyphQuadSourceRect(p) : commands->GetTexturedQuadSourceRect(p);
            auto tint = glyph ? commands->GetGlyphQuadColor(p) : commands->GetTexturedQuadColor(p);
            auto* texture = glyph ? commands->GetGlyphQuadTexture(p) : commands->GetTexturedQuadTexture(p);
            if (glyph) {
                auto bounds = commands->GetGlyphQuadBounds(p);
                Batch2DGeometry::CreateQuad__out8_out9_out10_out11(bounds.X, bounds.Y, bounds.Z, bounds.W, 0, 0, source, Batch2DGeometry::NormalizeColor(tint), a, b, c, d);
            } else {
                Batch2DGeometry::CreateSpriteQuad__out3_out4_out5_out6(commands->GetTexturedQuadGeometry(p), source, Batch2DGeometry::NormalizeColor(tint), a, b, c, d);
            }
            Writer.AppendQuad(Run(Batch2DVariant::Textured, texture), a, b, c, d);
        } else throw std::runtime_error("Unknown engine UI command");
    }
    Writer.Flush(); wgpuRenderPassEncoderEnd(Pass); wgpuRenderPassEncoderRelease(Pass); Pass = nullptr;
}
}
