#pragma once
#include "RuntimeModel.hpp"
#include <webgpu/webgpu.h>

namespace helengine::webgpu {
/// Retains uploaded mesh buffers and authored draw ranges for the generated engine.
class WebGpuRuntimeModel : public RuntimeModel {
public:
    /// Releases only this runtime wrapper's GPU buffers.
    ~WebGpuRuntimeModel() override { if (Vertices) wgpuBufferRelease(Vertices); if (Indices) wgpuBufferRelease(Indices); }
    /// Holds the uploaded position/normal vertex stream.
    WGPUBuffer Vertices = nullptr;
    /// Holds a normalized 32-bit index stream, when present.
    WGPUBuffer Indices = nullptr;
    /// Records the available vertices and indices.
    uint32_t VertexCount = 0, IndexCount = 0;
};
}
