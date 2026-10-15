using helengine;

namespace helengine.webgpu.builder.tests;

/// <summary>Provides named in-memory include contents while exposing every compiler lookup to assertions.</summary>
internal sealed class WebGpuTestIncludeResolver : IShaderIncludeResolver {
    /// <summary>Stores caller-authored include text by its requested Unicode name.</summary>
    public Dictionary<string, string> Sources { get; } = new(StringComparer.Ordinal);

    /// <summary>Records active lookups in order, including compiler retry requests.</summary>
    public List<string> Requests { get; } = [];

    /// <summary>Resolves fixture includes or reports the exact missing requested name.</summary>
    public ShaderIncludeResult Resolve(string requestingFile, string includePath) {
        Requests.Add(includePath);
        if (!Sources.TryGetValue(includePath, out string source)) { throw new FileNotFoundException("Missing include: " + includePath); }
        return new ShaderIncludeResult(Path.Combine(Path.GetDirectoryName(requestingFile), includePath), source);
    }
}
