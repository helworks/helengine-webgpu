#pragma once
#include "RuntimeMaterial.hpp"
#include "float4.hpp"

namespace helengine::webgpu {
/// Stores a cooked basic material's values without allocating GPU state per frame.
class WebGpuRuntimeMaterial : public RuntimeMaterial {
public:
    /// Holds the straight-alpha material color.
    float4 Color;
    /// Enables the supported directional-light contribution.
    bool Lit = true;
};
}
