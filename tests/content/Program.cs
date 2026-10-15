namespace helengine.webgpu.content;

/// <summary>Runs the isolated editor-backed content preparation without loading an editor window.</summary>
public static class Program {
    /// <summary>Requires explicit project/report paths and applies only the requested WebGPU content changes.</summary>
    public static int Main(string[] arguments) {
        DemoDiscWebGpuContentVerifier.Prepare(Value(arguments,"--project-root"),Value(arguments,"--report"),arguments.Contains("--apply",StringComparer.Ordinal));
        return 0;
    }

    /// <summary>Reads one required command option rather than guessing a project or output directory.</summary>
    static string Value(string[] arguments,string option) {
        int index=Array.IndexOf(arguments,option);
        if(index<0 || index+1>=arguments.Length || arguments[index+1].StartsWith("--",StringComparison.Ordinal)) throw new ArgumentException("Missing required option "+option);
        return arguments[index+1];
    }
}
