using helengine.webgpu.builder;
using System.Security.Cryptography;
using System.Text.Json;

namespace helengine.webgpu.builder.tests;

/// <summary>Verifies that shader tools are explicit, pinned and fingerprinted before any compiler process starts.</summary>
public sealed class WebGpuShaderToolchainTests : IDisposable {
    /// <summary>Owns an isolated, visible fixture directory including spaces.</summary>
    readonly string Root = Path.Combine("C:\\dev\\helworks\\builds\\helengine-webgpu\\shared-shaders\\fixtures", "shader tools " + Guid.NewGuid().ToString("N"));

    /// <summary>Creates deterministic fake tool bytes for resolution tests; they are never executed.</summary>
    public WebGpuShaderToolchainTests() {
        Directory.CreateDirectory(Path.Combine(Root, "bin"));
        Directory.CreateDirectory(Path.Combine(Root, "include", "nlohmann"));
        File.WriteAllText(Path.Combine(Root, "bin", "helengine-webgpu-shader.exe"), "fixture tool");
        File.WriteAllText(Path.Combine(Root, "include", "nlohmann", "json.hpp"), "fixture header");
        WriteLock();
    }

    /// <summary>Explicit missing tool roots fail with an actionable diagnostic.</summary>
    [Fact]
    public void Resolve_rejects_missing_executable() {
        File.Delete(Path.Combine(Root, "bin", "helengine-webgpu-shader.exe"));
        Assert.Contains("helengine-webgpu-shader", Assert.Throws<FileNotFoundException>(() => WebGpuShaderToolchain.Resolve(Root)).Message);
    }

    /// <summary>Replacing a binary without updating the pinned identity is an error.</summary>
    [Fact]
    public void Resolve_rejects_changed_binary() {
        File.AppendAllText(Path.Combine(Root, "bin", "helengine-webgpu-shader.exe"), "changed");
        Assert.Contains("hash", Assert.Throws<InvalidOperationException>(() => WebGpuShaderToolchain.Resolve(Root)).Message, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>Tool roots containing spaces are represented by absolute paths rather than shell fragments.</summary>
    [Fact]
    public void Resolve_supports_spaces_and_changes_fingerprint_with_verified_tools() {
        WebGpuShaderToolchain first = WebGpuShaderToolchain.Resolve(Root);
        Assert.Equal(Path.Combine(Root, "bin", "helengine-webgpu-shader.exe"), first.CompilerPath);
        File.AppendAllText(first.CompilerPath, "new revision");
        WriteLock();
        Assert.NotEqual(first.Fingerprint, WebGpuShaderToolchain.Resolve(Root).Fingerprint);
    }

    /// <summary>Compilation cancellation takes precedence over launching the tool.</summary>
    [Fact]
    public void Lower_honors_cancellation_before_process_start() {
        WebGpuTintCompiler compiler = new(WebGpuShaderToolchain.Resolve(Root));
        using CancellationTokenSource cancellation = new();
        cancellation.Cancel();
        Assert.Throws<OperationCanceledException>(() => compiler.Lower([3, 2, 35, 7], Root, cancellation.Token));
    }

    /// <summary>Tool output must contain validated WGSL and a complete reflection record.</summary>
    [Fact]
    public void Parse_rejects_malformed_or_incomplete_tool_output() {
        Assert.Throws<InvalidDataException>(() => WebGpuTintResult.Parse("", "{}"));
        Assert.Throws<InvalidDataException>(() => WebGpuTintResult.Parse("invalid", "{"));
    }

    /// <summary>Records the actual fixture hashes using the same documented lock schema as setup.</summary>
    void WriteLock() {
        File.WriteAllText(Path.Combine(Root, "toolchain.lock.json"), JsonSerializer.Serialize(new {
            schemaVersion = 1, dawnRevision = "31e25af254ab572c77054edec4946d2244e184dd", adapterVersion = "1",
            compilerSha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(Root, "bin", "helengine-webgpu-shader.exe")))).ToLowerInvariant(),
            jsonSha256 = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(Path.Combine(Root, "include", "nlohmann", "json.hpp")))).ToLowerInvariant()
        }));
    }

    /// <summary>Releases only the fixture owned by this test instance.</summary>
    public void Dispose() {
        Directory.Delete(Root, true);
    }
}
