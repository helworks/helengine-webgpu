using helengine.baseplatform.Manifest;
using helengine.baseplatform.Profiles;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Targets;
using helengine.baseplatform.Results;
using System.Security.Cryptography;
using System.Text;

namespace helengine.webgpu.builder.tests;

/// <summary>Owns a visible workspace fixture with a staged scene and isolated output/cache directories.</summary>
public sealed class WebGpuBuildFixture {
    /// <summary>Creates the fixture without touching authored projects or other test runs.</summary>
    public WebGpuBuildFixture() {
        Root = Path.Combine("C:\\dev\\helworks\\builds\\helengine-webgpu\\managed-tests", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(Path.Combine(Root, "staged", "cooked", "scenes"));
        File.WriteAllText(Path.Combine(Root, "staged", "cooked", "scenes", "cube.helen"), "scene fixture");
        var dependencies = new WebGpuPlatformAssetBuilder().RendererShaderDependencies;
        List<PlatformShaderArtifactCookSource> sources = [];
        foreach (string asset in dependencies.Select(value => value.ShaderAssetId).Distinct(StringComparer.Ordinal)) {
            string path = asset == "MissingMaterial"
                ? "C:/dev/helworks/helengine/engine/helengine.directx11/shaders/MissingMaterial.fx"
                : "C:/dev/helworks/helengine/engine/helengine.editor/shaders/builtin/" + asset + ".hlsl";
            string text = File.ReadAllText(path);
            sources.Add(new PlatformShaderArtifactCookSource(asset, Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(text))), text, path));
        }
        new WebGpuShaderArtifactCooker().Cook(new PlatformShaderArtifactCookRequest(Path.Combine(Root, "staged", "cooked"), "webgpu", "release", "webgpu",
            dependencies, sources, helengine.NumericPrecisionPolicy.Default));
        Directory.CreateDirectory(Path.Combine(Root, "web"));
        File.WriteAllText(Path.Combine(Root, "web", "index.html"), "<!doctype html><title>fixture</title>");
        File.WriteAllText(Path.Combine(Root, "web", "bootstrap.js"), "fixture bootstrap");
        File.WriteAllText(Path.Combine(Root, "web", "audio.js"), "fixture audio");
    }
    /// <summary>Gets the fixture's absolute root.</summary>
    public string Root { get; }
    /// <summary>Builds a request using the same manifest/request constructors as the editor.</summary>
    public PlatformBuildRequest CreateRequest(string scenePath = "cooked/scenes/cube.helen", PlatformBuildExecutionScope scope = PlatformBuildExecutionScope.Full) {
        string cooked = Path.Combine(Root, "staged", "cooked");
        PlatformBuildArtifact[] shaders = Directory.GetFiles(cooked, "*.wgsp", SearchOption.AllDirectories).Select(path => new PlatformBuildArtifact(
            Path.GetRelativePath(cooked, path).Replace('\\', '/'), Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))), "shader", "test")).ToArray();
        PlatformBuildManifest manifest = new PlatformBuildManifest(1, "test", "1.0.0", "1.0.0", "webgpu", "1.0.0", "cube",
            [new PlatformBuildScene("cube", "Cube", scenePath, [], [])], [], shaders, [], [], new PlatformContainerWritePlan("", []), [], PlatformBuildRuntimeFeatureManifest.Empty);
        return new PlatformBuildRequest(manifest,
            [new PlatformBuildTargetVariant("webgpu", "webgpu", "webgpu", "default")],
            [new PlatformCookProfile("default", "Default", new PlatformCookProfileCapabilities("webgpu", "raw", "rgba", "scene-v1", PlatformSerializationEndianness.LittleEndian))],
            Path.Combine(Root, "output"), Path.Combine(Root, "work"), "release", "webgpu", "default", null, null, null, Path.Combine(Root, "core"),
            executionScope: scope);
    }
}
