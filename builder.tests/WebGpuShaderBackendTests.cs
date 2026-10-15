using helengine;
using helengine.webgpu.builder;

namespace helengine.webgpu.builder.tests;

/// <summary>Checks real HLSL compilation through the shared request and include contracts.</summary>
public sealed class WebGpuShaderBackendTests {
    /// <summary>Exercises colliding HLSL register numbers in all three supported resource classes.</summary>
    const string SampleSource = "cbuffer ColorBuffer : register(b0) { float4 color; }; Texture2D tex : register(t0); SamplerState samp : register(s0); float4 PS(float2 uv : TEXCOORD0) : SV_TARGET { return tex.Sample(samp, uv) * color; }";

    /// <summary>Default binding policy separates buffers, textures and samplers in group zero.</summary>
    [Fact]
    public void Compile_preserves_resource_classes_with_shared_binding_policy() {
        WebGpuShaderStage result = Compile(SampleSource, ShaderStage.Pixel);
        Assert.Equal(new[] { 0, 100, 200 }, result.Bindings.Select(value => value.Binding).Order().ToArray());
        Assert.All(result.Bindings, value => Assert.Equal(0, value.Group));
        Assert.Contains(result.Bindings, value => value.Name == "ColorBuffer" && value.Class == "constant-buffer");
        Assert.Equal("TEXCOORD", Assert.Single(result.Inputs).Semantic);
    }

    /// <summary>Policy-induced collisions are errors rather than silently reassigned slots.</summary>
    [Fact]
    public void Compile_rejects_binding_policy_collision() {
        Assert.Throws<InvalidOperationException>(() => Compile(SampleSource, ShaderStage.Pixel, new ShaderBindingPolicy(0, 0, 0, 0, 0)));
    }

    /// <summary>Unsupported spaces and storage resources do not produce nominally usable shaders.</summary>
    [Theory]
    [InlineData("Texture2D tex : register(t0, space1); float4 PS(float2 uv:TEXCOORD0):SV_TARGET { return tex.Load(int3(uv,0)); }")]
    [InlineData("RWTexture2D<float4> tex : register(u0); float4 PS(float2 uv:TEXCOORD0):SV_TARGET { return tex[int2(uv)]; }")]
    public void Compile_rejects_unsupported_resources(string source) {
        Assert.ThrowsAny<Exception>(() => Compile(source, ShaderStage.Pixel));
    }

    /// <summary>Inactive includes never resolve, while nested Unicode paths record the bytes actually compiled.</summary>
    [Fact]
    public void Compile_uses_resolver_and_fingerprints_active_nested_includes() {
        string source = "#if 0\n#include \"missing.hlsl\"\n#endif\n#include \"coração.hlsl\"\nfloat4 PS():SV_TARGET { return value(); }";
        WebGpuTestIncludeResolver resolver = new();
        resolver.Sources["coração.hlsl"] = "#include \"nested.hlsl\"\nfloat4 value() { return nested(); }";
        resolver.Sources["nested.hlsl"] = "float4 nested() { return float4(1,0,0,1); }";
        WebGpuShaderStage first = Compile(source, ShaderStage.Pixel, null, resolver);
        Assert.DoesNotContain("missing.hlsl", resolver.Requests);
        Assert.Equal(2, first.Dependencies.Count);
        resolver.Sources["nested.hlsl"] = "float4 nested() { return float4(0,1,0,1); }";
        WebGpuShaderStage second = Compile(source, ShaderStage.Pixel, null, resolver);
        Assert.NotEqual(first.Dependencies.Single(value => value.Path.EndsWith("nested.hlsl")).Hash,
            second.Dependencies.Single(value => value.Path.EndsWith("nested.hlsl")).Hash);
    }

    /// <summary>Resolver failures retain the include identity in the compiler diagnostic.</summary>
    [Fact]
    public void Compile_reports_missing_include() {
        Exception error = Assert.ThrowsAny<Exception>(() => Compile("#include \"missing.hlsl\"\nfloat4 PS():SV_TARGET{return 1;}", ShaderStage.Pixel, null, new WebGpuTestIncludeResolver()));
        Assert.Contains("missing.hlsl", error.Message);
    }

    /// <summary>Matrix and array packing is recorded from both compiled representations.</summary>
    [Fact]
    public void Compile_preserves_matrix_and_array_layout_metadata() {
        WebGpuShaderStage stage = Compile("cbuffer Camera:register(b0) { row_major float4x4 cameraMatrix; float4 offsets[2]; }; float4 VS(float3 p:POSITION):SV_POSITION { return mul(float4(p,1),cameraMatrix)+offsets[1]; }", ShaderStage.Vertex);
        WebGpuShaderBinding binding = Assert.Single(stage.Bindings);
        var originalMembers = binding.SourceLayout.GetProperty("members").EnumerateArray().ToArray();
        Assert.Equal(0, originalMembers[0].GetProperty("offset").GetInt32());
        Assert.Equal(16, originalMembers[0].GetProperty("matrixStride").GetInt32());
        Assert.Equal(64, originalMembers[1].GetProperty("offset").GetInt32());
        Assert.Equal(16, originalMembers[1].GetProperty("arrayStride").GetInt32());
        Assert.Equal(96, binding.Size);
        var finalMembers = binding.Layout.GetProperty("members").EnumerateArray().ToArray();
        Assert.Equal(16, finalMembers[0].GetProperty("matrixStride").GetInt32());
        Assert.Equal("column-major", finalMembers[0].GetProperty("matrixOrientation").GetString());
        Assert.Equal(16, finalMembers[1].GetProperty("arrayStride").GetInt32());
    }

    /// <summary>Unsupported execution stages fail at the shared boundary before any compiler work.</summary>
    [Fact]
    public void Compile_rejects_compute_stage_and_disables_unkeyed_shared_cache() {
        WebGpuShaderBackend backend = new(WebGpuShaderToolchain.Resolve("C:\\dev\\helworks\\tools\\webgpu-shaders"),
            "C:\\dev\\helworks\\builds\\helengine-webgpu\\shared-shaders\\backend-tests");
        Assert.False(backend.UseSharedCache);
        ShaderCompileRequest request = new(new ShaderSourceInfo("fixture.hlsl", "[numthreads(1,1,1)] void CS() {}"), "compute", "CS", ShaderStage.Compute,
            ShaderCompileTarget.WebGpu, new ShaderModel(5, 0), "test", [], new ShaderCompileOptions(ShaderBindingPolicies.Default, false, true, false));
        Assert.Throws<NotSupportedException>(() => backend.Compile(request, new WebGpuTestIncludeResolver()));
    }

    /// <summary>Every actual built-in variant lowers without substituting a platform-owned shader.</summary>
    [Theory]
    [InlineData("ForwardStandardShader", "ForwardStandard")]
    [InlineData("ForwardStandardShader", "ForwardStandardShadowed")]
    [InlineData("ForwardStandardShader", "ShadowDepth")]
    [InlineData("SpriteBatchShader", "Textured")]
    [InlineData("SpriteBatchShader", "RoundedShape")]
    public void Compile_links_original_builtin_variant(string asset, string variantName) {
        string path = Path.Combine("C:\\dev\\helworks\\helengine\\engine\\helengine.editor\\shaders\\builtin", asset + ".hlsl");
        StandardShaderVariant variant = (asset == "SpriteBatchShader" ? SpriteBatchShaderVariants.All : StandardShaderVariants.All)
            .Single(value => value.Name == variantName);
        ShaderDefine[] defines = variant.Defines.Select(value => value.Split('=', 2))
            .Select(value => new ShaderDefine(value[0], value.Length == 2 ? value[1] : "1")).ToArray();
        WebGpuShaderStage vertex = Compile(File.ReadAllText(path), ShaderStage.Vertex, null, null, variant.VertexEntryPoint, defines, path, variant.Name);
        WebGpuShaderStage fragment = Compile(File.ReadAllText(path), ShaderStage.Pixel, null, null, variant.PixelEntryPoint, defines, path, variant.Name);
        WebGpuShaderProgramPair pair = WebGpuShaderProgramPair.Link(vertex, fragment);
        Assert.Equal(variantName, pair.Vertex.Request.Variant);
        Assert.NotEmpty(pair.Vertex.Wgsl);
        Assert.NotEmpty(pair.Fragment.Wgsl);
    }

    /// <summary>Builds a real shared request while keeping all compiler jobs in the workspace.</summary>
    internal static WebGpuShaderStage Compile(string source, ShaderStage stage, ShaderBindingPolicy policy = null,
        IShaderIncludeResolver resolver = null, string entry = null, IReadOnlyList<ShaderDefine> defines = null,
        string path = "C:\\dev\\helworks\\builds\\helengine-webgpu\\shared-shaders\\fixture.hlsl", string variant = "test") {
        WebGpuShaderBackend backend = new(WebGpuShaderToolchain.Resolve("C:\\dev\\helworks\\tools\\webgpu-shaders"),
            "C:\\dev\\helworks\\builds\\helengine-webgpu\\shared-shaders\\backend-tests");
        ShaderModel model = new(5, 0);
        ShaderCompileRequest request = new(new ShaderSourceInfo(path, source), "fixture." + stage,
            entry ?? (stage == ShaderStage.Vertex ? "VS" : "PS"), stage, ShaderCompileTarget.WebGpu, model, variant,
            ShaderPlatformDefines.BuildDefines(ShaderCompileTarget.WebGpu, model, defines ?? []),
            new ShaderCompileOptions(policy ?? ShaderBindingPolicies.Default, false, true, false));
        return ((WebGpuShaderCompileResult)backend.Compile(request, resolver ?? new ShaderFilesystemIncludeResolver(Path.GetDirectoryName(path)))).Stage;
    }
}
