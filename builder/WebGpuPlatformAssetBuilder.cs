using helengine.baseplatform.Builders;
using helengine.baseplatform.Definitions;
using helengine.baseplatform.Descriptors;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;
using helengine.baseplatform.Reporting;

namespace helengine.webgpu.builder;

/// <summary>Exposes the WebGPU plugin through the existing editor/CLI platform build contract.</summary>
public sealed class WebGpuPlatformAssetBuilder : IPlatformAssetBuilder {
    /// <summary>Identifies supported engine/manifest ranges and build profiles.</summary>
    public PlatformBuilderDescriptor Descriptor { get; } = new("helengine.webgpu.builder", "1.0.0", "webgpu", new EngineCompatibilityRange("1.0.0", "999.0.0"), new ManifestCompatibilityRange(1, 1), ["webgpu"], ["debug", "release"]);
    /// <summary>Describes browser build and cooking capabilities.</summary>
    public PlatformDefinition Definition { get; } = WebGpuPlatformDefinitionFactory.Create();
    /// <summary>Translates the authored schema into the engine's existing material serialization.</summary>
    public PlatformMaterialCookResult CookMaterial(PlatformMaterialCookRequest request) { return new WebGpuMaterialCooker().Cook(request); }
    /// <summary>Runs content staging and native compilation for a resolved project request.</summary>
    public Task<PlatformBuildReport> BuildAsync(PlatformBuildRequest request, IPlatformBuildProgressReporter progressReporter, IPlatformBuildDiagnosticReporter diagnosticReporter, CancellationToken cancellationToken) {
        string source = Environment.GetEnvironmentVariable("HELENGINE_WEBGPU_REPOSITORY_ROOT");
        if (string.IsNullOrWhiteSpace(source)) {
            DirectoryInfo directory = new(Path.GetDirectoryName(typeof(WebGpuPlatformAssetBuilder).Assembly.Location));
            while (directory != null && !File.Exists(Path.Combine(directory.FullName, "CMakeLists.txt"))) { directory = directory.Parent; }
            source = directory?.FullName ?? throw new InvalidOperationException("WebGPU platform source root is not installed.");
        }
        return new WebGpuBuildWorkspace(new WebGpuNativeBuildExecutor(), Directory.GetCurrentDirectory(), source).BuildAsync(request, progressReporter, diagnosticReporter, cancellationToken);
    }
}
