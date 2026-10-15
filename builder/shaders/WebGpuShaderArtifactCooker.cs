using helengine;
using helengine.baseplatform.Manifest;
using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.webgpu.builder;

/// <summary>Compiles explicit shared selections and publishes only completely prepared WebGPU shader pairs.</summary>
public sealed class WebGpuShaderArtifactCooker {
    /// <summary>Defers native tool loading until a validated nonempty request needs compilation.</summary>
    readonly Func<string, IShaderBackend> BackendFactory;

    /// <summary>Creates the production cooker using the explicit workspace tool configuration.</summary>
    public WebGpuShaderArtifactCooker() : this(WebGpuShaderEnvironment.CreateBackend) { }
    /// <summary>Allows invocation counting or controlled compiler failures without replacing the cook contract.</summary>
    public WebGpuShaderArtifactCooker(Func<string, IShaderBackend> backendFactory) {
        BackendFactory = backendFactory ?? throw new ArgumentNullException(nameof(backendFactory));
    }

    /// <summary>Validates all selections, compiles through the shared service and declares every published shader.</summary>
    public PlatformShaderArtifactCookResult Cook(PlatformShaderArtifactCookRequest request) {
        ArgumentNullException.ThrowIfNull(request);
        if (request.PlatformId != "webgpu" || request.GraphicsProfileId != "webgpu") { throw new InvalidOperationException("WebGPU shader cook requires platform/profile webgpu."); }
        if (request.ShaderDependencies.Count == 0) { return new PlatformShaderArtifactCookResult([]); }
        Dictionary<string, PlatformShaderArtifactCookSource> sources = request.ShaderSources.ToDictionary(value => value.ShaderAssetId, StringComparer.Ordinal);
        SortedDictionary<string, PlatformShaderDependency> selections = new(StringComparer.Ordinal);
        foreach (PlatformShaderDependency dependency in request.ShaderDependencies) {
            string path = WebGpuShaderArtifactIdentity.RelativePath(dependency);
            if (!sources.TryGetValue(dependency.ShaderAssetId, out PlatformShaderArtifactCookSource source) || !source.HasSourcePath || string.IsNullOrWhiteSpace(source.SourceText)) {
                throw new InvalidOperationException("WebGPU shader selection has no complete source snapshot: " + dependency.ShaderAssetId);
            }
            ResolveVariant(dependency, source);
            selections.TryAdd(path, dependency);
        }
        // Keep native inputs and logs outside the scanned cooked-content tree.
        string workRoot = Path.Combine(Path.GetDirectoryName(request.CookRootPath), ".webgpu-shader-work", Guid.NewGuid().ToString("N"));
        IShaderBackend backend = BackendFactory(workRoot);
        SortedDictionary<string, byte[]> prepared = new(StringComparer.Ordinal);
        foreach (var selection in selections) {
            PlatformShaderDependency dependency = selection.Value;
            PlatformShaderArtifactCookSource source = sources[dependency.ShaderAssetId];
            StandardShaderVariant variant = ResolveVariant(dependency, source);
            ShaderDefine[] defines = variant.Defines.Select(value => value.Split('=', 2))
                .Select(value => new ShaderDefine(value[0], value.Length == 2 ? value[1] : "1")).ToArray();
            ShaderCompileService service = new(new ShaderFilesystemIncludeResolver(Path.GetDirectoryName(source.SourcePath)), new ShaderMemoryCompileCache(), new ShaderSourceHasher());
            service.RegisterBackend(backend);
            WebGpuShaderStage vertex = Compile(service, source, dependency.VertexProgramName, variant.VertexEntryPoint, ShaderStage.Vertex, variant.Name, defines);
            WebGpuShaderStage fragment = Compile(service, source, dependency.PixelProgramName, variant.PixelEntryPoint, ShaderStage.Pixel, variant.Name, defines);
            WebGpuShaderArtifact artifact = WebGpuShaderArtifact.FromPair(dependency.ShaderAssetId, WebGpuShaderProgramPair.Link(vertex, fragment));
            prepared.Add(selection.Key, WebGpuShaderArtifactSerializer.Serialize(artifact));
        }
        Dictionary<string, string> temporary = new(StringComparer.Ordinal);
        try {
            foreach (var output in prepared) {
                string final = WebGpuBuildWorkspace.ContainedPath(request.CookRootPath, output.Key);
                Directory.CreateDirectory(Path.GetDirectoryName(final));
                string path = final + ".prepared-" + Guid.NewGuid().ToString("N");
                temporary.Add(final, path);
                File.WriteAllBytes(path, output.Value);
            }
            foreach (var output in temporary) { File.Move(output.Value, output.Key, true); }
        } finally {
            foreach (string path in temporary.Values) { if (File.Exists(path)) { File.Delete(path); } }
        }
        return new PlatformShaderArtifactCookResult(selections.Select(value => new PlatformCookedArtifactDeclaration(
            value.Key, value.Key, "shader", value.Value.VariantName)).ToArray());
    }

    /// <summary>Uses actual shared built-in catalogs and the existing custom-package VS/PS/default convention.</summary>
    static StandardShaderVariant ResolveVariant(PlatformShaderDependency dependency, PlatformShaderArtifactCookSource source) {
        IReadOnlyList<StandardShaderVariant> variants;
        string programPrefix;
        if (dependency.ShaderAssetId == "ForwardStandardShader") {
            // The editor's shared standard material packaging also emits its existing default/Mesh aliases.
            variants = StandardShaderVariants.All.Concat([new StandardShaderVariant("default", "VS", "PS", []), new StandardShaderVariant("Mesh", "VS", "PS", [])]).ToArray();
            programPrefix = dependency.ShaderAssetId;
        }
        else if (dependency.ShaderAssetId == "SpriteBatchShader") { variants = SpriteBatchShaderVariants.All; programPrefix = dependency.ShaderAssetId; }
        else {
            // ShaderPackageBuilder defines custom source packages with file-name programs, VS/PS entries and default variant.
            programPrefix = Path.GetFileNameWithoutExtension(source.SourcePath);
            variants = [new StandardShaderVariant("default", "VS", "PS", [])];
        }
        StandardShaderVariant variant = variants.SingleOrDefault(value => value.Name == dependency.VariantName);
        if (variant == null || dependency.VertexProgramName != programPrefix + ".vs" || dependency.PixelProgramName != programPrefix + ".ps") {
            throw new InvalidOperationException("WebGPU shader selection does not match its shared program/variant definition: " + dependency.ShaderAssetId + "/" + dependency.VariantName);
        }
        return variant;
    }

    /// <summary>Compiles an explicit stage through the shared service and requires its exact typed result.</summary>
    static WebGpuShaderStage Compile(ShaderCompileService service, PlatformShaderArtifactCookSource source,
        string program, string entry, ShaderStage stage, string variant, IReadOnlyList<ShaderDefine> defines) {
        ShaderModel model = new(5, 0);
        ShaderCompileRequest request = new(new ShaderSourceInfo(source.SourcePath, source.SourceText), program, entry,
            stage, ShaderCompileTarget.WebGpu, model, variant, ShaderPlatformDefines.BuildDefines(ShaderCompileTarget.WebGpu, model, defines),
            new ShaderCompileOptions(ShaderBindingPolicies.Default, false, true, false));
        if (service.Compile(request) is not WebGpuShaderCompileResult result || !result.Success || !ReferenceEquals(result.Request, request)) {
            throw new InvalidOperationException("WebGPU shader service returned an unsuccessful or mismatched typed result.");
        }
        return result.Stage;
    }
}
