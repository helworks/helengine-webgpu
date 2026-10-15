#include "webgpu_shadow_caster_collector.hpp"
#include "webgpu_shadow_renderer.hpp"
#include "webgpu_runtime_model.hpp"
#include "webgpu_runtime_material.hpp"
#include "Entity.hpp"
#include "ICamera.hpp"
#include "IDrawable3D.hpp"
#include "IRenderQueue3D.hpp"
#include "RuntimeSubmesh.hpp"
#include "ModelPrimitiveTopology.hpp"
#include <stdexcept>
namespace helengine::webgpu {
void WebGpuShadowCasterCollector::Visit(IDrawable3D* drawable) {
    if(!drawable || !drawable->get_Parent() || !drawable->get_Parent()->get_IsHierarchyEnabled() || !(drawable->get_Parent()->get_LayerMask()&LayerMask)) return;
    auto* model=dynamic_cast<WebGpuRuntimeModel*>(drawable->get_Model()); if(!model) throw std::runtime_error("Shadow caster requires a WebGPU runtime model");
    auto* ranges=model->get_Submeshes(); auto* materials=drawable->get_Materials(); int count=ranges && ranges->Length?ranges->Length:1;
    for(int index=0;index<count;++index) {
        if(!materials || index>=materials->Length || !(*materials)[index]) throw std::runtime_error("Shadow caster submesh has no material");
        auto* material=dynamic_cast<WebGpuRuntimeMaterial*>((*materials)[index]->ResolveRootMaterial());
        if(!material) throw std::runtime_error("Shadow caster requires a WebGPU runtime material");
        if(!material->get_CastsShadows() || material->Artifact.Metadata.at("renderState").at("alphaMode")!="opaque") continue;
        auto* range=ranges && ranges->Length?(*ranges)[index]:nullptr; if(range && range->PrimitiveTopology!=ModelPrimitiveTopology::TriangleList) continue;
        WebGpuShadowCaster caster; caster.Vertices=model->Vertices; caster.Indices=model->Indices; caster.First=range?range->IndexStart:0;
        caster.Count=range?range->IndexCount:model->Indices?model->IndexCount:model->VertexCount; caster.World=drawable->get_Parent()->get_WorldTransformMatrix(); Casters.push_back(caster);
    }
}
void WebGpuShadowRenderer::Render(ICamera* camera,const WebGpuShadowPlan& plan) {
    WebGpuShadowCasterCollector collector(camera->get_LayerMask());
    if(!plan.Entries.empty()) camera->get_RenderQueue3D()->VisitOrdered(&collector);
    RenderCasters(collector.Casters,plan);
}
}
