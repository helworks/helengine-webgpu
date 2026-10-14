namespace helengine.webgpu.builder;

/// <summary>Identifies the three linked artifacts produced by Emscripten.</summary>
public sealed class WebGpuNativeBuildResult {
    /// <summary>Initializes the artifact paths returned by the native compiler.</summary>
    public WebGpuNativeBuildResult(string javaScriptPath, string webAssemblyPath, string dataPath) { JavaScriptPath = javaScriptPath; WebAssemblyPath = webAssemblyPath; DataPath = dataPath; }
    /// <summary>Gets the linked JavaScript loader.</summary>
    public string JavaScriptPath { get; }
    /// <summary>Gets the WebAssembly binary.</summary>
    public string WebAssemblyPath { get; }
    /// <summary>Gets the preloaded game data package.</summary>
    public string DataPath { get; }
}
