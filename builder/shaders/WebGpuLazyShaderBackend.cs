using helengine;

namespace helengine.webgpu.builder;

/// <summary>Registers browser compilation without requiring its tools when another platform opens the editor.</summary>
internal sealed class WebGpuLazyShaderBackend : IShaderBackend, IShaderBackendCachePolicy {
    /// <summary>Stores the compiler after the first actual WebGPU request.</summary>
    readonly Lazy<IShaderBackend> Backend = new(() => WebGpuShaderEnvironment.CreateBackend(null));
    /// <summary>Gets the shared browser shader target without loading a native tool.</summary>
    public ShaderCompileTarget Target => ShaderCompileTarget.WebGpu;
    /// <summary>Advertises the same tested range as the concrete backend.</summary>
    public ShaderBackendCapabilities Capabilities { get; } = new(new ShaderModel(4, 0), new ShaderModel(5, 0), [ShaderStage.Vertex, ShaderStage.Pixel], false);
    /// <summary>Disables cache reads before tool/include fingerprints have been incorporated into shared cache keys.</summary>
    public bool UseSharedCache => false;
    /// <summary>Initializes tools lazily and forwards the unchanged shared request and include resolver.</summary>
    public ShaderCompileResult Compile(ShaderCompileRequest request, IShaderIncludeResolver includeResolver) => Backend.Value.Compile(request, includeResolver);
}
