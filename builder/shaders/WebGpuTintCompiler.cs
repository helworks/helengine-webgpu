using System.Diagnostics;
using System.Text;

namespace helengine.webgpu.builder;

/// <summary>Runs the pinned shader adapter with isolated files, bounded diagnostics and cancellation.</summary>
public sealed class WebGpuTintCompiler {
    /// <summary>Borrows an installation whose content was checked against its lock.</summary>
    readonly WebGpuShaderToolchain Toolchain;

    /// <summary>Requires a verified compiler installation rather than discovering executables through PATH.</summary>
    public WebGpuTintCompiler(WebGpuShaderToolchain toolchain) {
        Toolchain = toolchain ?? throw new ArgumentNullException(nameof(toolchain));
    }

    /// <summary>Lowers one compiled stage and preserves compiler inputs/diagnostics inside the supplied work root.</summary>
    public WebGpuTintResult Lower(byte[] spirv, string workRoot, CancellationToken cancellationToken) {
        cancellationToken.ThrowIfCancellationRequested();
        ArgumentNullException.ThrowIfNull(spirv);
        if (spirv.Length == 0 || spirv.Length % 4 != 0) {
            throw new ArgumentException("SPIR-V must contain complete nonempty words.", nameof(spirv));
        }
        if (string.IsNullOrWhiteSpace(workRoot) || !Path.IsPathFullyQualified(workRoot)) {
            throw new ArgumentException("Shader work root must be absolute.", nameof(workRoot));
        }
        return LowerAsync(spirv, workRoot, cancellationToken).GetAwaiter().GetResult();
    }

    /// <summary>Drains both process streams while enforcing a timeout and terminating cancelled work.</summary>
    async Task<WebGpuTintResult> LowerAsync(byte[] spirv, string workRoot, CancellationToken cancellationToken) {
        string job = Path.Combine(workRoot, "tint-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(job);
        string input = Path.Combine(job, "input.spv");
        string output = Path.Combine(job, "output.wgsl");
        string reflection = Path.Combine(job, "reflection.json");
        await File.WriteAllBytesAsync(input, spirv, cancellationToken);
        ProcessStartInfo start = new(Toolchain.CompilerPath) {
            UseShellExecute = false, CreateNoWindow = true, WindowStyle = ProcessWindowStyle.Hidden,
            RedirectStandardOutput = true, RedirectStandardError = true, WorkingDirectory = job
        };
        foreach (string value in new[] { "--input", input, "--wgsl", output, "--reflection", reflection }) {
            start.ArgumentList.Add(value);
        }
        using Process process = new() { StartInfo = start };
        using CancellationTokenSource timeout = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        timeout.CancelAfter(TimeSpan.FromMinutes(2));
        process.Start();
        Task<string> stdout = DrainAsync(process.StandardOutput);
        Task<string> stderr = DrainAsync(process.StandardError);
        try {
            await process.WaitForExitAsync(timeout.Token);
        } catch (OperationCanceledException) {
            if (!process.HasExited) { process.Kill(true); }
            await process.WaitForExitAsync();
            if (cancellationToken.IsCancellationRequested) { throw; }
            throw new TimeoutException($"WebGPU shader tool timed out: {Toolchain.CompilerPath}; job {job}");
        }
        string diagnostic = await stderr;
        await File.WriteAllTextAsync(Path.Combine(job, "stdout.log"), await stdout, CancellationToken.None);
        await File.WriteAllTextAsync(Path.Combine(job, "stderr.log"), diagnostic, CancellationToken.None);
        if (process.ExitCode != 0) {
            throw new InvalidOperationException($"WebGPU shader tool '{Toolchain.CompilerPath}' exited {process.ExitCode}: {diagnostic}; job {job}");
        }
        if (!File.Exists(output) || !File.Exists(reflection)
            || new FileInfo(output).Length > 64 * 1024 * 1024 || new FileInfo(reflection).Length > 16 * 1024 * 1024) {
            throw new InvalidDataException($"WebGPU shader tool omitted valid bounded outputs: {job}");
        }
        return WebGpuTintResult.Parse(await File.ReadAllTextAsync(output, cancellationToken),
            await File.ReadAllTextAsync(reflection, cancellationToken));
    }

    /// <summary>Consumes the entire stream while retaining only its last 64 KiB of diagnostic characters.</summary>
    static async Task<string> DrainAsync(StreamReader reader) {
        StringBuilder tail = new();
        char[] buffer = new char[4096];
        int count;
        while ((count = await reader.ReadAsync(buffer)) != 0) {
            tail.Append(buffer, 0, count);
            if (tail.Length > 65536) { tail.Remove(0, tail.Length - 65536); }
        }
        return tail.ToString();
    }
}
