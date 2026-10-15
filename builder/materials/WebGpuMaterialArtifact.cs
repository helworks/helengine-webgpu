using System.Text.Json;

namespace helengine.webgpu.builder;

/// <summary>Owns a versioned WebGPU material selection and its original named values.</summary>
public sealed class WebGpuMaterialArtifact {
    /// <summary>Copies metadata independently of the temporary parser or source document.</summary>
    public WebGpuMaterialArtifact(JsonElement metadata) { Metadata = metadata.Clone(); }
    /// <summary>Gets explicit shader, render-state, constant and texture data.</summary>
    public JsonElement Metadata { get; }
}
