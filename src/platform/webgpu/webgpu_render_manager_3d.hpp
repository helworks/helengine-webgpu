#pragma once
#include "webgpu_frame.hpp"
#include "RenderManager3D.hpp"
#include "IRenderVisitor3D.hpp"
#include "float4x4.hpp"
#include "float4.hpp"
#include "webgpu_light_state.hpp"
#include "webgpu_shadow_plan.hpp"
#include "webgpu_pipeline_target.hpp"

namespace helengine::webgpu {
class WebGpuRenderManager2D;
class WebGpuShadowRenderer;
class WebGpuRuntimeMaterial;
/// Draws the engine's ordered camera queues using a basic lit WebGPU mesh pipeline.
class WebGpuRenderManager3D : public RenderManager3D, public IRenderVisitor3D {
public:
    /// Creates retained pipeline state on the frame device.
    explicit WebGpuRenderManager3D(WebGpuFrame& frame);
    /// Releases pipeline objects after scene resources have retired.
    ~WebGpuRenderManager3D() override;
    /// Converts raw mesh streams into retained GPU buffers.
    RuntimeModel* BuildModelFromRaw(ModelAsset* data) override;
    /// Deserializes a prepared engine model and uploads it.
    RuntimeModel* BuildModelFromCooked(std::string path, IContentStreamSource* source) override;
    /// Creates a normal engine render target whose output later cameras and sprites can sample.
    RenderTarget* CreateRenderTarget(int32_t width,int32_t height) override;
    /// Validates and loads the platform-owned basic material payload.
    RuntimeMaterial* BuildMaterialFromCooked(PlatformMaterialAsset* data) override;
    /// Reads and validates one prepared material from the content source.
    RuntimeMaterial* BuildMaterialFromCooked(std::string path, IContentStreamSource* source) override;
    /// Reports exactly the forward-light features implemented by this backend.
    RendererBackendCapabilityProfile* GetCapabilityProfile() override;
    /// Renders active cameras in engine order, interleaving their UI queues.
    void Draw() override;
    /// Draws one ordered mesh with the active camera and its authored material.
    void Visit(IDrawable3D* drawable) override;
    /// Attaches the same-device UI renderer before core initialization.
    WebGpuRenderManager2D* Ui = nullptr;
private:
    /// Borrows frame attachments and upload storage from the host.
    WebGpuFrame& Frame;
    /// Borrows the current render pass during queue traversal.
    WGPURenderPassEncoder Pass = nullptr;
    /// Stores current camera and light constants.
    float4x4 ViewProjection;
    float4 CameraPosition;
    WebGpuLightState Lights;
    WebGpuShadowPlan ShadowPlan;
    ArtifactJson ForwardValues;
    ArtifactJson ShadowValues;
    std::unique_ptr<WebGpuShadowRenderer> Shadows;
    WebGpuPipelineTarget ActiveTarget;
    /// Owns the original desktop missing-material shader used by unbound mesh slots.
    std::unique_ptr<WebGpuRuntimeMaterial> MissingMaterial;
    /// Loads the declared source-cooked built-in shader once for this renderer.
    WebGpuRuntimeMaterial* GetMissingMaterial();
};
}
