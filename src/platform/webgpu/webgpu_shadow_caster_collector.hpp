#pragma once
#include "webgpu_shadow_caster.hpp"
#include "IRenderVisitor3D.hpp"
#include <vector>
namespace helengine::webgpu {
/// Collects light-pass submissions from camera registrations without applying main-camera frustum culling.
class WebGpuShadowCasterCollector:public IRenderVisitor3D {
public:
    explicit WebGpuShadowCasterCollector(uint16_t layerMask):LayerMask(layerMask) {}
    void Visit(IDrawable3D* drawable) override;
    std::vector<WebGpuShadowCaster> Casters;
private:
    uint16_t LayerMask;
};
}
