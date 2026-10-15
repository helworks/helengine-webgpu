using helengine;
using Silk.NET.Shaderc;
using System.Runtime.InteropServices;
using System.Text;

namespace helengine.webgpu.builder;

/// <summary>Compiles authoritative HLSL through shaderc while preserving original reflection and include ownership.</summary>
public sealed class WebGpuShaderSourceCompiler {
    /// <summary>Produces SPIR-V with shared register policy shifts and records only active include content.</summary>
    public unsafe byte[] Compile(ShaderCompileRequest request, IShaderIncludeResolver resolver, out IReadOnlyList<WebGpuShaderDependency> dependencies) {
        using WebGpuShaderIncludeSession includes = new(resolver);
        using Shaderc api = Shaderc.GetApi();
        Compiler* compiler = api.CompilerInitialize();
        CompileOptions* options = api.CompileOptionsInitialize();
        if (compiler == null || options == null) {
            if (compiler != null) { api.CompilerRelease(compiler); }
            if (options != null) { api.CompileOptionsRelease(options); }
            throw new InvalidOperationException("WebGPU shaderc initialization failed.");
        }
        byte[] bytes = null;
        string diagnostic = null;
        try {
            api.CompileOptionsSetSourceLanguage(options, SourceLanguage.Hlsl);
            api.CompileOptionsSetTargetEnv(options, TargetEnv.Vulkan, (uint)EnvVersion.Vulkan10);
            api.CompileOptionsSetHlslIoMapping(options, true);
            api.CompileOptionsSetHlslFunctionality1(options, true);
            api.CompileOptionsSetAutoMapLocations(options, true);
            api.CompileOptionsSetAutoBindUniforms(options, true);
            api.CompileOptionsSetPreserveBindings(options, true);
            api.CompileOptionsSetGenerateDebugInfo(options);
            api.CompileOptionsSetOptimizationLevel(options, request.Options.Optimize ? OptimizationLevel.Performance : OptimizationLevel.Zero);
            if (request.Options.TreatWarningsAsErrors) { api.CompileOptionsSetWarningsAsErrors(options); }
            ShaderBindingPolicy policy = request.Options.BindingPolicy;
            api.CompileOptionsSetBindingBase(options, UniformKind.Buffer, (uint)policy.ConstantBufferShift);
            api.CompileOptionsSetBindingBase(options, UniformKind.Texture, (uint)policy.TextureShift);
            api.CompileOptionsSetBindingBase(options, UniformKind.Sampler, (uint)policy.SamplerShift);
            api.CompileOptionsSetBindingBase(options, UniformKind.StorageBuffer, (uint)policy.StorageShift);
            api.CompileOptionsSetBindingBase(options, UniformKind.Image, (uint)policy.StorageShift);
            api.CompileOptionsSetIncludeCallbacks(options, includes.ResolveCallback, includes.ReleaseCallback, null);
            foreach (ShaderDefine define in request.Defines) {
                string value = define.Value ?? string.Empty;
                api.CompileOptionsAddMacroDefinition(options, define.Name, (nuint)Encoding.UTF8.GetByteCount(define.Name), value, (nuint)Encoding.UTF8.GetByteCount(value));
            }
            CompilationResult* result = api.CompileIntoSpv(compiler, request.Source.Source, (nuint)Encoding.UTF8.GetByteCount(request.Source.Source),
                request.Stage == ShaderStage.Vertex ? ShaderKind.VertexShader : ShaderKind.FragmentShader, request.Source.Path, request.EntryPoint, options);
            if (result == null) { throw new InvalidOperationException("WebGPU shaderc returned no compilation result."); }
            try {
                if (api.ResultGetCompilationStatus(result) != CompilationStatus.Success) { diagnostic = api.ResultGetErrorMessageS(result); }
                else {
                    bytes = new byte[checked((int)api.ResultGetLength(result))];
                    Marshal.Copy((IntPtr)api.ResultGetBytes(result), bytes, 0, bytes.Length);
                }
            } finally { api.ResultRelease(result); }
        } finally {
            api.CompileOptionsRelease(options); api.CompilerRelease(compiler);
        }
        includes.ThrowIfFailed();
        if (diagnostic != null) { throw new InvalidOperationException("WebGPU HLSL compilation failed: " + diagnostic); }
        if (bytes == null || bytes.Length == 0) { throw new InvalidOperationException("WebGPU HLSL produced empty SPIR-V."); }
        dependencies = includes.Dependencies;
        return bytes;
    }
}
