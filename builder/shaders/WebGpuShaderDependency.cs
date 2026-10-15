namespace helengine.webgpu.builder;

/// <summary>Identifies the exact content returned by one active include lookup.</summary>
public sealed class WebGpuShaderDependency {
    /// <summary>Copies one resolved include identity.</summary>
    public WebGpuShaderDependency(string path, string hash) { Path = path; Hash = hash; }
    /// <summary>Gets the resolver-owned source identity.</summary>
    public string Path { get; }
    /// <summary>Gets the SHA-256 of the compiled UTF-8 include content.</summary>
    public string Hash { get; }
}
