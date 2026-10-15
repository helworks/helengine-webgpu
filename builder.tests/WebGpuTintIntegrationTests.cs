using helengine.webgpu.builder;
using Silk.NET.Shaderc;
using System.Runtime.InteropServices;
using System.Text;

namespace helengine.webgpu.builder.tests;

/// <summary>Exercises the real pinned native translator and its process failure boundary.</summary>
public sealed class WebGpuTintIntegrationTests {
    /// <summary>Uses the explicit installation built by the approved setup script.</summary>
    const string ToolRoot = "C:\\dev\\helworks\\tools\\webgpu-shaders";

    /// <summary>Retains compiler jobs and diagnostics in a visible workspace directory.</summary>
    const string WorkRoot = "C:\\dev\\helworks\\builds\\helengine-webgpu\\shared-shaders\\tint tests Ω";

    /// <summary>A rejected binary must retain stderr and the exact failing tool identity.</summary>
    [Fact]
    public void Lower_surfaces_native_exit_and_stderr() {
        WebGpuTintCompiler compiler = new(WebGpuShaderToolchain.Resolve(ToolRoot));
        InvalidOperationException error = Assert.Throws<InvalidOperationException>(() => compiler.Lower(new byte[20], WorkRoot, CancellationToken.None));
        Assert.Contains("helengine-webgpu-shader", error.Message);
        Assert.Contains("Invalid SPIR-V magic", error.Message);
        Assert.Contains("exited 1", error.Message);
    }

    /// <summary>Both real graphics stages lower to validated WGSL with final semantic reflection.</summary>
    [Theory]
    [InlineData(true)]
    [InlineData(false)]
    public void Lower_compiles_real_graphics_stage(bool vertex) {
        string source = vertex
            ? "cbuffer Camera : register(b0) { row_major float4x4 cameraMatrix; }; float4 VS(float3 p : POSITION) : SV_POSITION { return mul(float4(p, 1), cameraMatrix); }"
            : "float4 PS() : SV_TARGET { return float4(1, 0.5, 0.25, 1); }";
        WebGpuTintResult result = new WebGpuTintCompiler(WebGpuShaderToolchain.Resolve(ToolRoot))
            .Lower(CompileFixture(source, vertex), WorkRoot, CancellationToken.None);
        Assert.Contains(vertex ? "@vertex" : "@fragment", result.Wgsl);
        Assert.Equal(vertex ? "vertex" : "fragment", result.Reflection.GetProperty("stage").GetString());
        if (vertex) {
            var binding = Assert.Single(result.Reflection.GetProperty("bindings").EnumerateArray());
            Assert.Equal(0, binding.GetProperty("group").GetInt32());
            Assert.Equal(0, binding.GetProperty("binding").GetInt32());
            Assert.Equal(64, binding.GetProperty("size").GetInt32());
            Assert.Single(binding.GetProperty("layout").GetProperty("members").EnumerateArray());
        }
    }

    /// <summary>Creates a minimal SPIR-V fixture with the same pinned shaderc package as production.</summary>
    static unsafe byte[] CompileFixture(string source, bool vertex) {
        using Shaderc api = Shaderc.GetApi();
        Compiler* compiler = api.CompilerInitialize();
        CompileOptions* options = api.CompileOptionsInitialize();
        if (compiler == null || options == null) { throw new InvalidOperationException("Fixture shaderc initialization failed."); }
        try {
            api.CompileOptionsSetSourceLanguage(options, SourceLanguage.Hlsl);
            api.CompileOptionsSetTargetEnv(options, TargetEnv.Vulkan, (uint)EnvVersion.Vulkan10);
            api.CompileOptionsSetHlslIoMapping(options, true);
            api.CompileOptionsSetAutoMapLocations(options, true);
            api.CompileOptionsSetGenerateDebugInfo(options);
            CompilationResult* result = api.CompileIntoSpv(compiler, source, (nuint)Encoding.UTF8.GetByteCount(source),
                vertex ? ShaderKind.VertexShader : ShaderKind.FragmentShader, "fixture.hlsl", vertex ? "VS" : "PS", options);
            if (result == null) { throw new InvalidOperationException("Fixture shaderc returned no result."); }
            try {
                if (api.ResultGetCompilationStatus(result) != CompilationStatus.Success) { throw new InvalidOperationException(api.ResultGetErrorMessageS(result)); }
                byte[] bytes = new byte[checked((int)api.ResultGetLength(result))];
                Marshal.Copy((IntPtr)api.ResultGetBytes(result), bytes, 0, bytes.Length);
                return bytes;
            } finally { api.ResultRelease(result); }
        } finally {
            api.CompileOptionsRelease(options);
            api.CompilerRelease(compiler);
        }
    }
}
