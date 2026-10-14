using System.Diagnostics;

namespace helengine.webgpu.builder;

/// <summary>Invokes CMake/Ninja with an explicit Emscripten toolchain and preserves failure diagnostics.</summary>
public sealed class WebGpuNativeBuildExecutor : IWebGpuNativeBuildExecutor {
    /// <summary>Compiles the generated engine, staged modules and maintained WebGPU host.</summary>
    public WebGpuNativeBuildResult Build(string repositoryRoot, string buildRoot, string generatedCoreRoot, string stagedCodeRoot, string contentRoot, string profileId, CancellationToken cancellationToken) {
        if (profileId != "debug" && profileId != "release") { throw new ArgumentException("WebGPU profile must be debug or release.", nameof(profileId)); }
        string sdk = Environment.GetEnvironmentVariable("HELENGINE_EMSDK_ROOT") ?? Path.GetFullPath(Path.Combine(repositoryRoot, "..", "tools", "emsdk"));
        string toolchain = Path.Combine(sdk, "upstream", "emscripten", "cmake", "Modules", "Platform", "Emscripten.cmake");
        if (!File.Exists(toolchain)) { throw new FileNotFoundException("Emscripten 6.0.12 is required. Run scripts/setup-toolchain.ps1.", toolchain); }
        Directory.CreateDirectory(buildRoot);
        Run("cmake", ["-S", repositoryRoot, "-B", buildRoot, "-G", "Ninja", "-DCMAKE_TOOLCHAIN_FILE=" + toolchain,
            "-DCMAKE_BUILD_TYPE=" + (profileId == "debug" ? "Debug" : "Release"), "-DHELENGINE_CORE_CPP_ROOT=" + generatedCoreRoot,
            "-DHELENGINE_CODE_ROOT=" + stagedCodeRoot, "-DHELENGINE_CONTENT_ROOT=" + contentRoot], sdk, Path.Combine(buildRoot, "configure.log"), cancellationToken);
        Run("cmake", ["--build", buildRoot, "--parallel", "4"], sdk, Path.Combine(buildRoot, "build.log"), cancellationToken);
        return new WebGpuNativeBuildResult(Path.Combine(buildRoot, "helengine_webgpu.js"), Path.Combine(buildRoot, "helengine_webgpu.wasm"), Path.Combine(buildRoot, "helengine_webgpu.data"));
    }

    /// <summary>Runs a child with structured arguments, SDK environment and cancellation of its process tree.</summary>
    static void Run(string command, string[] arguments, string sdk, string logPath, CancellationToken cancellationToken) {
        ProcessStartInfo start = new(command) { UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true, CreateNoWindow = true };
        foreach (string argument in arguments) { start.ArgumentList.Add(argument); }
        start.Environment["EM_CONFIG"] = Path.Combine(sdk, ".emscripten");
        start.Environment["PATH"] = Path.Combine(sdk, "upstream", "emscripten") + Path.PathSeparator + start.Environment["PATH"];
        using Process process = Process.Start(start) ?? throw new InvalidOperationException($"Could not start '{command}'.");
        using CancellationTokenRegistration registration = cancellationToken.Register(() => { if (!process.HasExited) { process.Kill(true); } });
        Task<string> stdout = process.StandardOutput.ReadToEndAsync(); Task<string> stderr = process.StandardError.ReadToEndAsync();
        process.WaitForExit(); Task.WaitAll(stdout, stderr); File.WriteAllText(logPath, stdout.Result + stderr.Result);
        cancellationToken.ThrowIfCancellationRequested();
        if (process.ExitCode != 0) { throw new InvalidOperationException($"{command} exited with {process.ExitCode}. Diagnostics: {logPath}\n{stderr.Result}"); }
    }
}
