using helengine;
using helengine.webgpu.builder;

namespace helengine.webgpu.builder.tests;

/// <summary>Exercises original shadow source contracts through the real pinned shader compilers.</summary>
public sealed class WebGpuShadowShaderTests {
    /// <summary>Locates the original engine shader sources rather than a browser-owned replacement.</summary>
    const string Builtins = "C:\\dev\\helworks\\helengine\\engine\\helengine.editor\\shaders\\builtin";

    /// <summary>Both shadow dimensions retain depth types, comparison samplers and original register identities.</summary>
    [Fact]
    public void Depth2D_and_cube_keep_comparison_reflection() {
        string source = "Texture2D<float> atlas:register(t1); TextureCube<float> cube:register(t2); SamplerComparisonState atlasSampler:register(s1); SamplerComparisonState cubeSampler:register(s2); float4 PS(float3 uv:TEXCOORD0):SV_TARGET { return atlas.SampleCmpLevelZero(atlasSampler,uv.xy,0.5)+cube.SampleCmpLevelZero(cubeSampler,uv,0.5); }";
        WebGpuShaderStage stage = WebGpuShaderBackendTests.Compile(source, ShaderStage.Pixel);
        Assert.Contains(stage.Bindings, value => value.Name == "atlas" && value.Class == "depth-texture" && value.TextureDimension == 1 && value.RegisterIndex == 1 && value.Binding == 101);
        Assert.Contains(stage.Bindings, value => value.Name == "cube" && value.Class == "depth-texture" && value.TextureDimension == 4 && value.RegisterIndex == 2 && value.Binding == 102);
        Assert.All(stage.Bindings.Where(value => value.Class == "sampler"), value => Assert.Equal(0, value.SamplerType));
        Assert.All(stage.Bindings.Where(value => value.Class == "depth-texture"), value => Assert.Equal(0, value.SampleType));
        Assert.Contains("texture_depth_2d", stage.Wgsl);
        Assert.Contains("texture_depth_cube", stage.Wgsl);
        Assert.Contains("sampler_comparison", stage.Wgsl);
        Assert.Contains("textureSampleCompareLevel", stage.Wgsl);
    }

    /// <summary>Regular and comparison samplers cannot alias a shared cross-stage binding.</summary>
    [Fact]
    public void Comparison_binding_conflict_is_rejected() {
        WebGpuShaderStage vertex = WebGpuShaderBackendTests.Compile("Texture2D<float> tex:register(t1); SamplerState samp:register(s1); float4 VS(float3 p:POSITION):SV_POSITION { return float4(p,tex.SampleLevel(samp,p.xy,0)); }", ShaderStage.Vertex);
        WebGpuShaderStage fragment = WebGpuShaderBackendTests.Compile("Texture2D<float> tex:register(t1); SamplerComparisonState samp:register(s1); float4 PS():SV_TARGET { return tex.SampleCmpLevelZero(samp,float2(0.5,0.5),0.5); }", ShaderStage.Pixel);
        Assert.Throws<InvalidOperationException>(() => WebGpuShaderProgramPair.Link(vertex, fragment));
    }

    /// <summary>Point shadow rasterization explicitly stores normalized radial distance in the depth attachment.</summary>
    [Fact]
    public void Point_pass_preserves_radial_fragment_depth() {
        string path = Path.Combine(Builtins, "EditorPointShadowDepth.hlsl");
        WebGpuShaderStage stage = WebGpuShaderBackendTests.Compile(File.ReadAllText(path), ShaderStage.Pixel, path: path);
        Assert.Contains("@builtin(frag_depth)", stage.Wgsl);
        Assert.DoesNotContain("@location(0)", stage.Wgsl);
        Assert.Contains("length(", stage.Wgsl);
        Assert.Contains("clamp(", stage.Wgsl);
    }

    /// <summary>The original forward shader carries depth resources and preserves all four shadow slots.</summary>
    [Fact]
    public void Original_forward_shader_uses_native_shadow_comparisons() {
        string path = Path.Combine(Builtins, "ForwardStandardShader.hlsl");
        WebGpuShaderStage stage = WebGpuShaderBackendTests.Compile(File.ReadAllText(path), ShaderStage.Pixel, path: path);
        Assert.Equal(5, stage.Bindings.Count(value => value.Class == "depth-texture"));
        Assert.Equal(new[] { 1, 2, 3, 4, 5 }, stage.Bindings.Where(value => value.Class == "depth-texture").Select(value => value.RegisterIndex).Order().ToArray());
        Assert.Contains("textureSampleCompareLevel", stage.Wgsl);
    }

    /// <summary>Platforms without the WebGPU define retain ordinary shadow textures and radial color output.</summary>
    [Theory]
    [InlineData("ForwardStandardShader")]
    [InlineData("EditorPointShadowDepth")]
    public void Non_webgpu_shadow_source_keeps_existing_outputs(string asset) {
        string path = Path.Combine(Builtins, asset + ".hlsl");
        WebGpuShaderBackend backend = new(WebGpuShaderToolchain.Resolve("C:\\dev\\helworks\\tools\\webgpu-shaders"), "C:\\dev\\helworks\\builds\\helengine-webgpu\\renderer-parity\\desktop-source");
        ShaderCompileRequest request = new(new ShaderSourceInfo(path, File.ReadAllText(path)), asset + ".ps", "PS", ShaderStage.Pixel,
            ShaderCompileTarget.WebGpu, new ShaderModel(5, 0), "default", [], new ShaderCompileOptions(ShaderBindingPolicies.Default, false, true, false));
        WebGpuShaderStage stage = ((WebGpuShaderCompileResult)backend.Compile(request, new ShaderFilesystemIncludeResolver(Builtins))).Stage;
        Assert.DoesNotContain("texture_depth_", stage.Wgsl);
        Assert.DoesNotContain("@builtin(frag_depth)", stage.Wgsl);
        if (asset == "EditorPointShadowDepth") { Assert.Contains("@location(0u)", stage.Wgsl); }
    }
}
