namespace helengine.webgpu.builder;

/// <summary>Isolates the external Emscripten compilation from staging and manifest validation.</summary>
public interface IWebGpuNativeBuildExecutor {
    /// <summary>Compiles the maintained host together with the official generated core and authored modules.</summary>
    WebGpuNativeBuildResult Build(string repositoryRoot, string buildRoot, string generatedCoreRoot, string stagedCodeRoot, string contentRoot, string profileId, CancellationToken cancellationToken);
}
