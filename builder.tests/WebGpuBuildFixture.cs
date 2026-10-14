using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Targets;

namespace helengine.webgpu.builder.tests;

/// <summary>Owns a visible workspace fixture with a staged scene and isolated output/cache directories.</summary>
public sealed class WebGpuBuildFixture {
    /// <summary>Creates the fixture without touching authored projects or other test runs.</summary>
    public WebGpuBuildFixture() {
        Root = Path.Combine("C:\\dev\\helworks\\builds\\helengine-webgpu\\managed-tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(Root, "staged", "cooked", "scenes"));
        File.WriteAllText(Path.Combine(Root, "staged", "cooked", "scenes", "cube.helen"), "scene fixture");
    }
    /// <summary>Gets the fixture's absolute root.</summary>
    public string Root { get; }
    /// <summary>Builds a request using the same manifest/request constructors as the editor.</summary>
    public PlatformBuildRequest CreateRequest(string scenePath = "cooked/scenes/cube.helen", PlatformBuildExecutionScope scope = PlatformBuildExecutionScope.Full) {
        PlatformBuildManifest manifest = new PlatformBuildManifest(1, "test", "1.0.0", "1.0.0", "webgpu", "1.0.0", "cube",
            [new PlatformBuildScene("cube", "Cube", scenePath, [], [])], [], [], [], [], new PlatformContainerWritePlan("", []), [], PlatformBuildRuntimeFeatureManifest.Empty);
        return new PlatformBuildRequest(manifest,
            [new PlatformBuildTargetVariant("webgpu", "webgpu", "webgpu", "default")],
            [new PlatformCookProfile("default", "Default", new PlatformCookProfileCapabilities("webgpu", "raw", "rgba", "scene-v1", PlatformSerializationEndianness.LittleEndian))],
            Path.Combine(Root, "output"), Path.Combine(Root, "work"), "release", "webgpu", "default", null, null, null, Path.Combine(Root, "core"),
            executionScope: scope);
    }
}
