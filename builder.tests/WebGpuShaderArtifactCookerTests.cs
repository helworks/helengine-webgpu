using helengine;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;
using helengine.webgpu.builder;
using System.Security.Cryptography;
using System.Text;

namespace helengine.webgpu.builder.tests;

/// <summary>Checks real shared-service cooking and publication boundaries without changing the blocked shadow shader.</summary>
public sealed class WebGpuShaderArtifactCookerTests : IDisposable {
    /// <summary>Owns the output directory for one test, separate from compiler work directories.</summary>
    readonly string Root = Path.Combine("C:\\dev\\helworks\\builds\\helengine-webgpu\\shared-shaders\\fixtures", "cook-" + Guid.NewGuid().ToString("N"));

    /// <summary>Creates the bounded output root before each cook.</summary>
    public WebGpuShaderArtifactCookerTests() { Directory.CreateDirectory(Root); }

    /// <summary>Both real shared UI variants compile once per distinct selection and declare every output.</summary>
    [Fact]
    public void Cook_deduplicates_and_declares_both_renderer_ui_variants() {
        string path = "C:\\dev\\helworks\\helengine\\engine\\helengine.editor\\shaders\\builtin\\SpriteBatchShader.hlsl";
        string text = File.ReadAllText(path);
        PlatformShaderDependency textured = new("SpriteBatchShader", "SpriteBatchShader.vs", "SpriteBatchShader.ps", "Textured");
        PlatformShaderDependency rounded = new("SpriteBatchShader", "SpriteBatchShader.vs", "SpriteBatchShader.ps", "RoundedShape");
        PlatformShaderArtifactCookRequest request = Request([textured, textured, rounded], "SpriteBatchShader", path, text);
        WebGpuCountingShaderBackend backend = new();
        PlatformShaderArtifactCookResult result = new WebGpuShaderArtifactCooker(_ => backend).Cook(request);
        Assert.Equal(4, backend.CompileCount);
        Assert.Equal(2, result.CookedArtifactDeclarations.Count);
        foreach (var declaration in result.CookedArtifactDeclarations) {
            Assert.Equal("shader", declaration.ArtifactKind);
            Assert.Equal("SpriteBatchShader", WebGpuShaderArtifactSerializer.Deserialize(File.ReadAllBytes(Path.Combine(Root, declaration.RelativePath))).Metadata.GetProperty("shaderAssetId").GetString());
        }
        var dependencies = new WebGpuPlatformAssetBuilder().RendererShaderDependencies;
        Assert.Contains(dependencies, value => value.VariantName == "Textured");
        Assert.Contains(dependencies, value => value.VariantName == "RoundedShape");
    }

    /// <summary>An error in the fragment stage publishes none of a newly prepared request.</summary>
    [Fact]
    public void Cook_does_not_publish_a_half_compiled_custom_pair() {
        PlatformShaderDependency dependency = new("custom", "custom.vs", "custom.ps", "default");
        var request = Request([dependency], "custom", Path.Combine(Root, "custom.hlsl"), "float4 VS(float3 p:POSITION):SV_POSITION { return float4(p,1); }");
        Assert.Throws<InvalidOperationException>(() => new WebGpuShaderArtifactCooker().Cook(request));
        Assert.Empty(Directory.GetFiles(Root, "*.wgsp", SearchOption.AllDirectories));
    }

    /// <summary>Missing source snapshots and unknown variants fail before publishing files.</summary>
    [Fact]
    public void Cook_rejects_missing_source_and_unknown_variant() {
        PlatformShaderDependency dependency = new("SpriteBatchShader", "SpriteBatchShader.vs", "SpriteBatchShader.ps", "unknown");
        WebGpuCountingShaderBackend backend = new();
        WebGpuShaderArtifactCooker cooker = new(_ => backend);
        Assert.Throws<InvalidOperationException>(() => cooker.Cook(new PlatformShaderArtifactCookRequest(Root, "webgpu", "release", "webgpu", [dependency])));
        Assert.Throws<InvalidOperationException>(() => cooker.Cook(Request([dependency], "SpriteBatchShader", Path.Combine(Root, "batch.hlsl"), "invalid")));
        Assert.Equal(0, backend.CompileCount);
    }

    /// <summary>Every original built-in variant produces an independently declared and checked artifact.</summary>
    [Fact]
    public void Cook_compiles_all_five_original_variants() {
        List<PlatformShaderDependency> dependencies = [];
        List<PlatformShaderArtifactCookSource> sources = [];
        foreach (string asset in new[] { "ForwardStandardShader", "SpriteBatchShader" }) {
            string path = Path.Combine("C:\\dev\\helworks\\helengine\\engine\\helengine.editor\\shaders\\builtin", asset + ".hlsl");
            string source = File.ReadAllText(path);
            sources.Add(new PlatformShaderArtifactCookSource(asset, Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(source))), source, path));
            foreach (StandardShaderVariant variant in asset == "SpriteBatchShader" ? SpriteBatchShaderVariants.All : StandardShaderVariants.All) {
                dependencies.Add(new PlatformShaderDependency(asset, asset + ".vs", asset + ".ps", variant.Name));
            }
        }
        var result = new WebGpuShaderArtifactCooker().Cook(new PlatformShaderArtifactCookRequest(Root, "webgpu", "release", "webgpu", dependencies, sources));
        Assert.Equal(5, result.CookedArtifactDeclarations.Count);
        foreach (var output in result.CookedArtifactDeclarations) {
            WebGpuShaderArtifact artifact = WebGpuShaderArtifactSerializer.Deserialize(File.ReadAllBytes(Path.Combine(Root, output.RelativePath)));
            Assert.Equal(output.VariantId, artifact.Metadata.GetProperty("variant").GetString());
        }
    }

    /// <summary>Changing an active include invalidates the cooked shader content without changing its logical path.</summary>
    [Fact]
    public void Cook_recompiles_after_include_change_and_preserves_last_valid_output_on_failure() {
        string source = "#include \"color.hlsli\"\nfloat4 VS(float3 p:POSITION):SV_POSITION { return float4(p,1); } float4 PS():SV_TARGET { return color(); }";
        string include = Path.Combine(Root, "color.hlsli");
        string path = Path.Combine(Root, "custom.hlsl");
        File.WriteAllText(include, "float4 color() { return float4(1,0,0,1); }");
        PlatformShaderDependency dependency = new("custom", "custom.vs", "custom.ps", "default");
        var request = Request([dependency], "custom", path, source);
        WebGpuShaderArtifactCooker cooker = new();
        string output = Path.Combine(Root, Assert.Single(cooker.Cook(request).CookedArtifactDeclarations).RelativePath);
        byte[] first = File.ReadAllBytes(output);
        File.WriteAllText(include, "float4 color() { return float4(0,1,0,1); }");
        cooker.Cook(request);
        byte[] second = File.ReadAllBytes(output);
        Assert.NotEqual(first, second);
        File.WriteAllText(include, "this is invalid HLSL");
        Assert.Throws<InvalidOperationException>(() => cooker.Cook(request));
        Assert.Equal(second, File.ReadAllBytes(output));
        Assert.Empty(Directory.GetFiles(Root, "*.prepared-*", SearchOption.AllDirectories));
    }

    /// <summary>Builds the same complete source snapshot contract the editor supplies.</summary>
    PlatformShaderArtifactCookRequest Request(IReadOnlyList<PlatformShaderDependency> dependencies, string asset, string path, string source) {
        return new PlatformShaderArtifactCookRequest(Root, "webgpu", "release", "webgpu", dependencies,
            [new PlatformShaderArtifactCookSource(asset, Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(source))), source, path)]);
    }

    /// <summary>Removes only the test's own output directory.</summary>
    public void Dispose() { Directory.Delete(Root, true); }
}
