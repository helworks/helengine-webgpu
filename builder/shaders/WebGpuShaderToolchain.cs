using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace helengine.webgpu.builder;

/// <summary>Resolves the pinned native shader adapter and checks its installed content identities.</summary>
public sealed class WebGpuShaderToolchain {
    /// <summary>Identifies the Dawn revision shared with the installed emdawnwebgpu port.</summary>
    public const string DawnRevision = "31e25af254ab572c77054edec4946d2244e184dd";

    /// <summary>Initializes a verified compiler installation.</summary>
    WebGpuShaderToolchain(string compilerPath, string fingerprint) {
        CompilerPath = compilerPath;
        Fingerprint = fingerprint;
    }

    /// <summary>Gets the absolute native adapter executable path.</summary>
    public string CompilerPath { get; }

    /// <summary>Gets the installation identity used in shader compilation metadata.</summary>
    public string Fingerprint { get; }

    /// <summary>Checks the explicit tool root, lock schema and pinned compiler/header hashes.</summary>
    public static WebGpuShaderToolchain Resolve(string toolRoot) {
        if (string.IsNullOrWhiteSpace(toolRoot) || !Path.IsPathFullyQualified(toolRoot)) {
            throw new ArgumentException("WebGPU shader tool root must be an absolute path.", nameof(toolRoot));
        }
        string compiler = Path.Combine(toolRoot, "bin", "helengine-webgpu-shader.exe");
        string header = Path.Combine(toolRoot, "include", "nlohmann", "json.hpp");
        RequireFile(compiler);
        RequireFile(header);
        string lockPath = Path.Combine(toolRoot, "toolchain.lock.json");
        RequireFile(lockPath);
        byte[] lockBytes = File.ReadAllBytes(lockPath);
        using JsonDocument document = JsonDocument.Parse(lockBytes);
        JsonElement root = document.RootElement;
        if (root.GetProperty("schemaVersion").GetInt32() != 1
            || root.GetProperty("dawnRevision").GetString() != DawnRevision
            || root.GetProperty("adapterVersion").GetString() != "1") {
            throw new InvalidOperationException($"WebGPU shader toolchain lock has an unsupported revision/schema: {lockPath}");
        }
        string compilerHash = VerifyHash(compiler, root.GetProperty("compilerSha256").GetString());
        string headerHash = VerifyHash(header, root.GetProperty("jsonSha256").GetString());
        string fingerprint = Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(
            DawnRevision + "\n" + compilerHash + "\n" + headerHash + "\n" + Convert.ToHexString(SHA256.HashData(lockBytes))))).ToLowerInvariant();
        return new WebGpuShaderToolchain(Path.GetFullPath(compiler), fingerprint);
    }

    /// <summary>Rejects a missing or empty compiler input before launching any process.</summary>
    static void RequireFile(string path) {
        if (!File.Exists(path) || new FileInfo(path).Length == 0) {
            throw new FileNotFoundException($"Required WebGPU shader toolchain file is missing or empty: {path}", path);
        }
    }

    /// <summary>Computes the actual file identity and rejects an installation changed outside setup.</summary>
    static string VerifyHash(string path, string expected) {
        string actual = Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))).ToLowerInvariant();
        if (!string.Equals(expected, actual, StringComparison.Ordinal)) {
            throw new InvalidOperationException($"WebGPU shader toolchain hash mismatch: {path}");
        }
        return actual;
    }
}
