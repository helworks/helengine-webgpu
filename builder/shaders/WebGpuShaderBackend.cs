using helengine;

namespace helengine.webgpu.builder;

/// <summary>Adapts shaderc and pinned Tint translation to the shared HelEngine shader service.</summary>
public sealed class WebGpuShaderBackend : IShaderBackend, IShaderBackendCachePolicy {
    /// <summary>Borrows the explicit fingerprinted native translator installation.</summary>
    readonly WebGpuShaderToolchain Toolchain;
    /// <summary>Stores the caller-owned absolute directory for compiler inputs and diagnostics.</summary>
    readonly string WorkRoot;

    /// <summary>Requires explicit tools and work roots rather than hidden temporary storage.</summary>
    public WebGpuShaderBackend(WebGpuShaderToolchain toolchain, string workRoot) {
        Toolchain = toolchain ?? throw new ArgumentNullException(nameof(toolchain));
        if (string.IsNullOrWhiteSpace(workRoot) || !Path.IsPathFullyQualified(workRoot)) { throw new ArgumentException("WebGPU shader work root must be absolute.", nameof(workRoot)); }
        WorkRoot = workRoot;
    }
    /// <summary>Gets the persisted browser shader target.</summary>
    public ShaderCompileTarget Target => ShaderCompileTarget.WebGpu;
    /// <summary>Gets the tested vertex/pixel source-language range; hardware ray tracing is not exposed.</summary>
    public ShaderBackendCapabilities Capabilities { get; } = new(new ShaderModel(4, 0), new ShaderModel(5, 0), [ShaderStage.Vertex, ShaderStage.Pixel], false);
    /// <summary>Disables shared reuse until all native tool and include identities participate in that cache.</summary>
    public bool UseSharedCache => false;

    /// <summary>Compiles the exact shared request and returns original identities with final WGSL layouts.</summary>
    public ShaderCompileResult Compile(ShaderCompileRequest request, IShaderIncludeResolver includeResolver) {
        ArgumentNullException.ThrowIfNull(request); ArgumentNullException.ThrowIfNull(includeResolver);
        if (request.Target != Target || request.Stage != ShaderStage.Vertex && request.Stage != ShaderStage.Pixel
            || request.ShaderModel.Major < 4 || request.ShaderModel.Major > 5 || request.ShaderModel.Minor != 0
            || request.Options.BindingPolicy.DefaultSpace != 0) {
            throw new NotSupportedException("WebGPU supports vertex/pixel HLSL 4.0–5.0 in register space zero only.");
        }
        try {
            byte[] spirv = new WebGpuShaderSourceCompiler().Compile(request, includeResolver, out IReadOnlyList<WebGpuShaderDependency> dependencies);
            WebGpuTintResult translation = new WebGpuTintCompiler(Toolchain).Lower(spirv, WorkRoot, CancellationToken.None);
            WebGpuShaderStage stage = WebGpuSpirvReflection.Read(request, spirv, translation, dependencies, Toolchain.Fingerprint);
            ShaderProgramDefinition definition = new(request.ProgramName, request.Stage, stage.EntryPoint,
                stage.Bindings.Select(BuildSharedBinding).ToArray(),
                stage.Inputs.Select(value => new ShaderVertexElement(value.Semantic, value.SemanticIndex, value.Type)).ToArray(),
                stage.Outputs.Select(value => new ShaderVertexElement(value.Semantic, value.SemanticIndex, value.Type)).ToArray(), []);
            return new WebGpuShaderCompileResult(stage, definition);
        } catch (Exception error) when (error is InvalidOperationException || error is InvalidDataException || error is NotSupportedException) {
            throw new InvalidOperationException($"WebGPU shader '{request.Source.Path}' program '{request.ProgramName}' entry '{request.EntryPoint}' stage '{request.Stage}' variant '{request.Variant}': {error.Message}", error);
        }
    }

    /// <summary>Publishes the common resource contract while preserving richer packing metadata in the typed stage.</summary>
    static ShaderBinding BuildSharedBinding(WebGpuShaderBinding binding) {
        ShaderResourceType type = binding.Class switch {
            "constant-buffer" => ShaderResourceType.ConstantBuffer,
            "texture" => binding.TextureDimension == 4 ? ShaderResourceType.TextureCube : ShaderResourceType.Texture2D,
            "sampler" => ShaderResourceType.Sampler,
            _ => throw new NotSupportedException("Unsupported WebGPU resource class: " + binding.Class)
        };
        ShaderConstantMember[] members = binding.Class == "constant-buffer"
            ? binding.SourceLayout.GetProperty("members").EnumerateArray().Select(value => new ShaderConstantMember(
                value.GetProperty("name").GetString(), value.GetProperty("type").GetString(), value.GetProperty("offset").GetInt32(), value.GetProperty("size").GetInt32())).ToArray()
            : [];
        return new ShaderBinding(binding.Name, type, binding.Group, binding.Binding, binding.Size, members);
    }
}
