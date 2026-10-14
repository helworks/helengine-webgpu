namespace helengine.webgpu.builder.tests;

/// <summary>Substitutes only the external compiler, producing files that exercise real package validation.</summary>
public sealed class TestNativeBuildExecutor : IWebGpuNativeBuildExecutor {
    /// <summary>Controls an intentionally incomplete compiler result.</summary>
    public bool OmitWasm { get; set; }
    /// <summary>Counts calls to detect unwanted compilation after validation fails.</summary>
    public int Calls { get; private set; }
    /// <summary>Produces deterministic compiler outputs inside the request's native build root.</summary>
    public WebGpuNativeBuildResult Build(string repositoryRoot, string buildRoot, string generatedCoreRoot, string stagedCodeRoot, string contentRoot, string profileId, CancellationToken cancellationToken) {
        Calls++;
        Directory.CreateDirectory(buildRoot);
        File.WriteAllText(Path.Combine(buildRoot, "helengine_webgpu.js"), "compiler javascript fixture");
        File.WriteAllText(Path.Combine(buildRoot, "helengine_webgpu.data"), "compiler data fixture");
        if (!OmitWasm) { File.WriteAllBytes(Path.Combine(buildRoot, "helengine_webgpu.wasm"), [0, 97, 115, 109]); }
        return new WebGpuNativeBuildResult(Path.Combine(buildRoot, "helengine_webgpu.js"), Path.Combine(buildRoot, "helengine_webgpu.wasm"), Path.Combine(buildRoot, "helengine_webgpu.data"));
    }
}
