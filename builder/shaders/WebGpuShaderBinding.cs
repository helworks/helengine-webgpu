using helengine;
using System.Text.Json;

namespace helengine.webgpu.builder;

/// <summary>Describes one linked original resource and its final WebGPU binding layout.</summary>
public sealed class WebGpuShaderBinding {
    /// <summary>Copies a compiler-verified resource mapping with both source and destination layout.</summary>
    public WebGpuShaderBinding(string name, string resourceClass, int registerIndex, int group, int binding,
        int size, JsonElement layout, JsonElement sourceLayout, int textureDimension, int sampleType, int samplerType,
        IReadOnlyList<ShaderStage> stages) {
        Name = name; Class = resourceClass; RegisterIndex = registerIndex; Group = group; Binding = binding;
        Size = size; Layout = layout; SourceLayout = sourceLayout; TextureDimension = textureDimension;
        SampleType = sampleType; SamplerType = samplerType; Stages = stages.ToArray();
    }
    /// <summary>Gets the original logical resource name used by engine/material binding.</summary>
    public string Name { get; }
    /// <summary>Gets the resource class in the WebGPU artifact schema.</summary>
    public string Class { get; }
    /// <summary>Gets the authored register index before the shared policy shift.</summary>
    public int RegisterIndex { get; }
    /// <summary>Gets the final bind group.</summary>
    public int Group { get; }
    /// <summary>Gets the final binding within its group.</summary>
    public int Binding { get; }
    /// <summary>Gets the final occupied buffer size, or zero for handle resources.</summary>
    public int Size { get; }
    /// <summary>Gets the final Tint semantic buffer layout.</summary>
    public JsonElement Layout { get; }
    /// <summary>Gets the original compiled buffer layout before WGSL lowering.</summary>
    public JsonElement SourceLayout { get; }
    /// <summary>Gets the pinned Tint texture-dimension value.</summary>
    public int TextureDimension { get; }
    /// <summary>Gets the pinned Tint sampled-kind value.</summary>
    public int SampleType { get; }
    /// <summary>Gets the pinned Tint sampler-kind value.</summary>
    public int SamplerType { get; }
    /// <summary>Gets the stages that actually access this binding.</summary>
    public IReadOnlyList<ShaderStage> Stages { get; }
    /// <summary>Returns the same mapping with linked visibility, leaving stage metadata unchanged.</summary>
    public WebGpuShaderBinding WithStages(IReadOnlyList<ShaderStage> stages) {
        return new(Name, Class, RegisterIndex, Group, Binding, Size, Layout, SourceLayout, TextureDimension, SampleType, SamplerType, stages);
    }
}
