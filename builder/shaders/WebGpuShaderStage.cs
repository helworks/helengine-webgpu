using helengine;

namespace helengine.webgpu.builder;

/// <summary>Owns one compiled stage and its reconciled original/final reflection.</summary>
public sealed class WebGpuShaderStage {
    /// <summary>Initializes a self-contained stage after frontend and Tint validation.</summary>
    public WebGpuShaderStage(ShaderCompileRequest request, WebGpuTintResult translation,
        IReadOnlyList<WebGpuShaderBinding> bindings, IReadOnlyList<WebGpuShaderInterfaceElement> inputs,
        IReadOnlyList<WebGpuShaderInterfaceElement> outputs, IReadOnlyList<WebGpuShaderDependency> dependencies, string toolchainFingerprint) {
        Request = request; Translation = translation; Bindings = bindings.ToArray(); Inputs = inputs.ToArray();
        Outputs = outputs.ToArray(); Dependencies = dependencies.ToArray(); ToolchainFingerprint = toolchainFingerprint;
    }
    /// <summary>Gets the original shared compile request and its explicit selection.</summary>
    public ShaderCompileRequest Request { get; }
    /// <summary>Gets the validated native translation and semantic metadata.</summary>
    public WebGpuTintResult Translation { get; }
    /// <summary>Gets the WGSL text produced by Tint.</summary>
    public string Wgsl => Translation.Wgsl;
    /// <summary>Gets the final renamed entry point.</summary>
    public string EntryPoint => Translation.Reflection.GetProperty("entryPoint").GetString();
    /// <summary>Gets all active resources in deterministic final slot order.</summary>
    public IReadOnlyList<WebGpuShaderBinding> Bindings { get; }
    /// <summary>Gets original semantics reconciled with final input locations.</summary>
    public IReadOnlyList<WebGpuShaderInterfaceElement> Inputs { get; }
    /// <summary>Gets original semantics reconciled with final output locations.</summary>
    public IReadOnlyList<WebGpuShaderInterfaceElement> Outputs { get; }
    /// <summary>Gets the exact active include content identities.</summary>
    public IReadOnlyList<WebGpuShaderDependency> Dependencies { get; }
    /// <summary>Gets the verified translator installation identity.</summary>
    public string ToolchainFingerprint { get; }
}
