using helengine;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace helengine.webgpu.builder;

/// <summary>Owns versioned shader metadata and the two validated WGSL stage sections.</summary>
public sealed class WebGpuShaderArtifact {
    /// <summary>Initializes independently owned metadata and explicit stage text.</summary>
    public WebGpuShaderArtifact(JsonElement metadata, string vertexWgsl, string fragmentWgsl) {
        Metadata = metadata.Clone(); VertexWgsl = vertexWgsl; FragmentWgsl = fragmentWgsl;
    }
    /// <summary>Gets the original selection, identities and reconciled native layout.</summary>
    public JsonElement Metadata { get; }
    /// <summary>Gets the WGSL module used for the vertex stage.</summary>
    public string VertexWgsl { get; }
    /// <summary>Gets the WGSL module used for the fragment stage.</summary>
    public string FragmentWgsl { get; }

    /// <summary>Captures one linked pair without losing its shared program or include identities.</summary>
    public static WebGpuShaderArtifact FromPair(string shaderAssetId, WebGpuShaderProgramPair pair) {
        ArgumentNullException.ThrowIfNull(pair);
        List<WebGpuShaderDependency> dependencies = [];
        foreach (WebGpuShaderStage stage in new[] { pair.Vertex, pair.Fragment }) {
            dependencies.Add(new WebGpuShaderDependency(stage.Request.Source.Path, Hash(stage.Request.Source.Source)));
            dependencies.AddRange(stage.Dependencies);
        }
        foreach (var group in dependencies.GroupBy(value => value.Path, StringComparer.Ordinal)) {
            if (group.Select(value => value.Hash).Distinct(StringComparer.Ordinal).Count() != 1) { throw new InvalidOperationException("Shader pair compiled different contents for source: " + group.Key); }
        }
        JsonElement metadata = JsonSerializer.SerializeToElement(new {
            shaderAssetId, vertexProgram = pair.Vertex.Request.ProgramName, pixelProgram = pair.Fragment.Request.ProgramName,
            variant = pair.Vertex.Request.Variant, target = "webgpu",
            toolchain = new { fingerprint = pair.Vertex.ToolchainFingerprint, dawnRevision = WebGpuShaderToolchain.DawnRevision, adapterVersion = "1" },
            dependencies = dependencies.DistinctBy(value => value.Path, StringComparer.Ordinal).OrderBy(value => value.Path, StringComparer.Ordinal).ToArray(),
            vertexEntryPoint = pair.Vertex.EntryPoint, fragmentEntryPoint = pair.Fragment.EntryPoint,
            vertexWgslSha256 = Hash(pair.Vertex.Wgsl), fragmentWgslSha256 = Hash(pair.Fragment.Wgsl),
            vertexInputs = pair.Vertex.Inputs, vertexOutputs = pair.Vertex.Outputs, fragmentInputs = pair.Fragment.Inputs,
            bindings = pair.Bindings.Select(BindingMetadata).ToArray()
        }, WebGpuArtifactJson.Options);
        return new WebGpuShaderArtifact(metadata, pair.Vertex.Wgsl, pair.Fragment.Wgsl);
    }

    /// <summary>Computes the identity of exact UTF-8 section or include content.</summary>
    internal static string Hash(string value) => Convert.ToHexString(SHA256.HashData(WebGpuArtifactJson.Utf8.GetBytes(value))).ToLowerInvariant();

    /// <summary>Publishes stable resource names and final layouts with explicit graphics-stage visibility.</summary>
    static object BindingMetadata(WebGpuShaderBinding binding) {
        return new { binding.Name, binding.Class, binding.RegisterIndex, registerSpace = 0, binding.Group, binding.Binding,
            binding.Size, binding.Layout, binding.SourceLayout, binding.TextureDimension, binding.SampleType, binding.SamplerType,
            stages = binding.Stages.Select(value => value == ShaderStage.Vertex ? "vertex" : "fragment").ToArray() };
    }
}
