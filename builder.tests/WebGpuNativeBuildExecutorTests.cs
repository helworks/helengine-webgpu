using System.Diagnostics;

namespace helengine.webgpu.builder.tests;

/// <summary>Checks the real process argument builder and the pinned compiler version contract.</summary>
public sealed class WebGpuNativeBuildExecutorTests {
    /// <summary>Preserves source/cache paths as individual arguments without shell quoting or splitting.</summary>
    [Fact]
    public void Arguments_preserve_paths_with_spaces() {
        ProcessStartInfo start = WebGpuNativeBuildExecutor.CreateStartInfo("cmake", ["-S", "C:\\game sources\\webgpu", "-DHELENGINE_CONTENT_ROOT=C:\\prepared assets\\content"], "C:\\SDK tools\\emsdk");
        Assert.False(start.UseShellExecute);
        Assert.Equal(new[] { "-S", "C:\\game sources\\webgpu", "-DHELENGINE_CONTENT_ROOT=C:\\prepared assets\\content" }, start.ArgumentList);
        Assert.Equal("C:\\SDK tools\\emsdk\\.emscripten", start.Environment["EM_CONFIG"]);
    }

    /// <summary>Accepts the official compiler banner for the approved toolchain version.</summary>
    [Fact]
    public void Pinned_compiler_version_is_accepted() {
        WebGpuToolchainResolver.ValidateVersion("emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 6.0.12 (commit abc)");
    }

    /// <summary>Rejects another activated SDK instead of compiling against an unreviewed WebGPU ABI.</summary>
    [Theory]
    [InlineData("emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 6.0.11")]
    [InlineData("emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 16.0.120")]
    [InlineData("")]
    public void Other_compiler_versions_are_rejected(string banner) {
        Assert.Throws<InvalidOperationException>(() => WebGpuToolchainResolver.ValidateVersion(banner));
    }
}
