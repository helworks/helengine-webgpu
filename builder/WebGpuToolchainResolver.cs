using System.Text.RegularExpressions;

namespace helengine.webgpu.builder;

/// <summary>Locates the approved SDK and rejects a different activated Emscripten/WebGPU ABI.</summary>
internal static class WebGpuToolchainResolver {
    /// <summary>Identifies the compiler and bundled Emdawnwebgpu revision approved for this platform.</summary>
    internal const string RequiredVersion = "6.0.12";

    /// <summary>Finds an explicit or workspace SDK and requires its compiler and CMake integration.</summary>
    internal static string Resolve(string repositoryRoot) {
        string sdk = Environment.GetEnvironmentVariable("HELENGINE_EMSDK_ROOT") ?? Path.GetFullPath(Path.Combine(repositoryRoot, "..", "tools", "emsdk"));
        foreach (string relative in new[] { ".emscripten", "upstream/emscripten/emcc.exe", "upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" }) {
            string file = Path.Combine(sdk, relative.Replace('/', Path.DirectorySeparatorChar));
            if (!File.Exists(file)) { throw new FileNotFoundException($"Emscripten {RequiredVersion} is required. Run scripts/setup-toolchain.ps1.", file); }
        }
        return sdk;
    }

    /// <summary>Checks the compiler banner rather than accepting a directory that contains another SDK.</summary>
    internal static void ValidateVersion(string banner) {
        if (!Regex.IsMatch(banner, @"^emcc\s+\(Emscripten[^\r\n]*\)\s+6\.0\.12(?:\s|$)", RegexOptions.CultureInvariant)) {
            throw new InvalidOperationException($"WebGPU requires Emscripten {RequiredVersion}. Run scripts/setup-toolchain.ps1 to activate that version.");
        }
    }
}
