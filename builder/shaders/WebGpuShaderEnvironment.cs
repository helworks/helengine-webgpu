using helengine;

namespace helengine.webgpu.builder;

/// <summary>Supplies explicit workspace tool/work roots at the plugin's compilation boundary.</summary>
internal static class WebGpuShaderEnvironment {
    /// <summary>Creates a compiler only when a cook or preview actually requests the browser target.</summary>
    internal static IShaderBackend CreateBackend(string workRoot) {
        string root = Environment.GetEnvironmentVariable("HELENGINE_WEBGPU_SHADER_TOOL_ROOT");
        if (string.IsNullOrWhiteSpace(root)) { root = "C:\\dev\\helworks\\tools\\webgpu-shaders"; }
        return new WebGpuShaderBackend(WebGpuShaderToolchain.Resolve(root), workRoot ?? Path.Combine(root, "jobs"));
    }
}
