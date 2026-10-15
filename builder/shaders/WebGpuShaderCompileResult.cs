using helengine;
using System.Text;

namespace helengine.webgpu.builder;

/// <summary>Publishes the shared compile result while preserving richer WebGPU reflection.</summary>
public sealed class WebGpuShaderCompileResult : ShaderCompileResult {
    /// <summary>Initializes a successful shared result with UTF-8 WGSL binary data.</summary>
    public WebGpuShaderCompileResult(WebGpuShaderStage stage, ShaderProgramDefinition definition)
        : base(stage.Request, definition, new ShaderCompiledBinary(ShaderCompileTarget.WebGpu,
            stage.Request.Stage, stage.EntryPoint, stage.Request.Variant, Encoding.UTF8.GetBytes(stage.Wgsl)), [], true) {
        Stage = stage;
    }
    /// <summary>Gets reconciled stage data required by the WebGPU artifact writer.</summary>
    public WebGpuShaderStage Stage { get; }
}
