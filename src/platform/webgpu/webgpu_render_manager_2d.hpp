#pragma once
#include "webgpu_frame.hpp"
#include "webgpu_shader_library.hpp"
#include "RenderManager2D.hpp"
#include "Batch2DSink.hpp"
#include "Batch2DWriter.hpp"
#include "Batch2DDrawableEmitter.hpp"
#include "float4x4.hpp"
#include "float4.hpp"
#include <vector>
class ICamera;
class RenderCommandListBuilder2D;
namespace helengine::webgpu {
class WebGpuRuntimeTexture;
/// Consumes shared engine quad batches for text, sprites and rounded UI panels.
class WebGpuRenderManager2D : public RenderManager2D, public Batch2DSink {
public:
    /// Creates reusable UI GPU state and shared CPU emitters.
    explicit WebGpuRenderManager2D(WebGpuFrame& frame);
    /// Releases pipelines and upload arenas after the core retires scene assets.
    ~WebGpuRenderManager2D() override;
    /// Uploads prepared RGBA texture bytes for sprites or font atlases.
    RuntimeTexture* BuildTextureFromRaw(TextureAsset* data) override;
    /// Deserializes one prepared texture using the generated engine serializer.
    RuntimeTexture* BuildTextureFromCooked(HeCppString path, IContentStreamSource* source) override;
    /// Emits a sprite through shared geometry rules.
    void DrawSprite(ISpriteDrawable2D* sprite) override;
    /// Emits text with shared font layout and glyph expansion.
    void DrawText(ITextDrawable2D* text) override;
    /// Emits a rounded panel using the shared corner/border representation.
    void DrawRoundedRect(IRoundedRectDrawable2D* shape) override;
    /// Sets the shared batch sink's camera projection.
    void SetCamera(float4x4 projection, float4 viewport) override;
    /// Uploads and submits one borrowed CPU chunk before returning.
    void Submit(Batch2DBuffer* buffer, Batch2DRun run) override;
    /// Resets frame arena cursors once before all camera passes.
    void BeginFrame();
    /// Draws the camera's actual ordered command list including clipping.
    void RenderCamera(ICamera* camera);
protected:
    /// Copies validated atlas/texture subregions without retaining WASM heap views.
    void UpdateTextureRegionCore(RuntimeTexture* texture, int32_t x, int32_t y, int32_t width, int32_t height, Array<uint8_t>* rgba8, int32_t sourceRowPitch) override;
private:
    /// Borrows the host's shared frame resources.
    WebGpuFrame& Frame;
    /// Expands CPU quads with the engine's existing batching implementation.
    Batch2DWriter Writer;
    /// Shares sprite, glyph and rounded-quad expansion with other backends.
    Batch2DDrawableEmitter Emitter;
    /// Owns ordered command-list construction and its retained glyph metadata.
    std::unique_ptr<RenderCommandListBuilder2D> Commands;
    /// Retains original shared shader variants and borrows their cached pipelines.
    std::shared_ptr<WebGpuShaderProgram> TexturedProgram, RoundedProgram;
    WGPUBindGroup RoundedBinding = nullptr;
    uint64_t UniformGeneration = 0;
    WGPURenderPipeline Textured = nullptr, Rounded = nullptr;
    WebGpuPipelineTarget ActiveTarget;
    /// Owns stable vertex/index arenas rather than allocating per drawable.
    WGPUBuffer Vertices = nullptr, Indices = nullptr;
    /// Holds the texture required by textureless rounded-shape batches.
    std::unique_ptr<WebGpuRuntimeTexture> White;
    /// Borrows the camera's active UI pass.
    WGPURenderPassEncoder Pass = nullptr;
    /// Retains the current projection, viewport and clip intersection stack.
    float4x4 Projection;
    float4 Viewport;
    std::vector<float4> Clips;
    /// Advances through retained frame upload arenas.
    uint64_t VertexCursor = 0, IndexCursor = 0;
    /// Returns the current intersected camera scissor for a shared writer run.
    Batch2DRun Run(Batch2DVariant variant, RuntimeTexture* texture);
};
}
