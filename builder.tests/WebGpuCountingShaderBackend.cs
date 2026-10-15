using helengine;
using helengine.webgpu.builder;

namespace helengine.webgpu.builder.tests;

/// <summary>Counts real backend invocations so cook deduplication is exercised without substituting shader results.</summary>
internal sealed class WebGpuCountingShaderBackend : IShaderBackend, IShaderBackendCachePolicy {
    /// <summary>Owns the real compiler used for every counted request.</summary>
    readonly WebGpuShaderBackend Backend = new(WebGpuShaderToolchain.Resolve("C:\\dev\\helworks\\tools\\webgpu-shaders"),
        "C:\\dev\\helworks\\builds\\helengine-webgpu\\shared-shaders\\counted-compiler");
    /// <summary>Gets the number of actual compile calls.</summary>
    public int CompileCount { get; private set; }
    /// <summary>Gets the target forwarded from the real compiler.</summary>
    public ShaderCompileTarget Target => Backend.Target;
    /// <summary>Gets the source-language capabilities forwarded from the real compiler.</summary>
    public ShaderBackendCapabilities Capabilities => Backend.Capabilities;
    /// <summary>Ensures cache reuse cannot hide duplicate compiler calls.</summary>
    public bool UseSharedCache => false;
    /// <summary>Counts then compiles the original request through shaderc and Tint.</summary>
    public ShaderCompileResult Compile(ShaderCompileRequest request, IShaderIncludeResolver includeResolver) {
        CompileCount++;
        return Backend.Compile(request, includeResolver);
    }
}
