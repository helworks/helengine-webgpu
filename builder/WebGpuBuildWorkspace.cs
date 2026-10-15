using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;
using helengine.baseplatform.Requests;
using System.Text.Json;

namespace helengine.webgpu.builder;

/// <summary>Stages resolved game content and verifies native artifacts before publication.</summary>
public sealed class WebGpuBuildWorkspace {
    /// <summary>Stores the compiler boundary used for this build.</summary>
    readonly IWebGpuNativeBuildExecutor Executor;
    /// <summary>Stores the resolved package root supplied by editor orchestration.</summary>
    readonly string StagingRoot;
    /// <summary>Stores the maintained platform source root.</summary>
    readonly string RepositoryRoot;
    /// <summary>Initializes the compiler and explicit package/source roots.</summary>
    public WebGpuBuildWorkspace(IWebGpuNativeBuildExecutor executor, string stagingRoot, string repositoryRoot) {
        Executor = executor ?? throw new ArgumentNullException(nameof(executor));
        StagingRoot = Path.GetFullPath(stagingRoot);
        RepositoryRoot = Path.GetFullPath(repositoryRoot);
    }
    /// <summary>Executes the requested staging/compilation scope with structured progress and diagnostics.</summary>
    public Task<PlatformBuildReport> BuildAsync(PlatformBuildRequest request, IPlatformBuildProgressReporter progress, IPlatformBuildDiagnosticReporter diagnostics, CancellationToken cancellationToken) {
        ArgumentNullException.ThrowIfNull(request); ArgumentNullException.ThrowIfNull(progress); ArgumentNullException.ThrowIfNull(diagnostics);
        cancellationToken.ThrowIfCancellationRequested();
        try {
            if (request.Manifest.Scenes.Count(scene => scene.SceneId == request.Manifest.StartupSceneId) != 1) {
                throw new InvalidOperationException("Startup scene must identify exactly one packaged scene.");
            }
            foreach (var scene in request.Manifest.Scenes) { ValidatePayload(scene.SourceIdentity); }
            foreach (var asset in request.Manifest.LooseAssets) { ValidatePayload(asset.SourceIdentity); }
            string contentRoot = ContainedPath(request.WorkingRoot, "content");
            if (IsWithin(StagingRoot, contentRoot) || IsWithin(RepositoryRoot, contentRoot) || IsWithin(request.OutputRoot, contentRoot) || IsWithin(contentRoot, request.OutputRoot)) {
                throw new InvalidOperationException("Builder content must not overlap source or output roots.");
            }
            if (request.ExecutionScope == PlatformBuildExecutionScope.Full) {
                foreach (string file in new[] { "index.html", "bootstrap.js", "audio.js" }) {
                    string source = Path.Combine(RepositoryRoot, "web", file);
                    if (!File.Exists(source) || new FileInfo(source).Length == 0) { throw new InvalidOperationException($"Required browser host file is missing or empty: {source}"); }
                }
            }
            RejectLinks(StagingRoot);
            if (Directory.Exists(contentRoot)) { RejectLinks(contentRoot); Directory.Delete(contentRoot, true); }
            string[] sources = Directory.GetFiles(StagingRoot, "*", SearchOption.AllDirectories)
                .Where(path => !Path.GetRelativePath(StagingRoot, path).Replace('\\', '/').StartsWith("_builder/", StringComparison.OrdinalIgnoreCase))
                .Where(path => !Path.GetRelativePath(StagingRoot, path).Replace('\\', '/').StartsWith("code/", StringComparison.OrdinalIgnoreCase))
                .Where(path => !Path.GetFileName(path).StartsWith("runtime-", StringComparison.OrdinalIgnoreCase)).ToArray();
            Directory.CreateDirectory(contentRoot);
            foreach (string source in sources) {
                cancellationToken.ThrowIfCancellationRequested();
                if ((File.GetAttributes(source) & FileAttributes.ReparsePoint) != 0) { throw new InvalidOperationException($"Staged symbolic links are unsupported: {source}"); }
                string relative = Path.GetRelativePath(StagingRoot, source).Replace('\\', '/');
                string cooked = relative.StartsWith("cooked/", StringComparison.OrdinalIgnoreCase) ? relative : "cooked/" + (relative.StartsWith("generated/", StringComparison.OrdinalIgnoreCase) ? relative[10..] : relative);
                string destination = ContainedPath(contentRoot, cooked);
                Directory.CreateDirectory(Path.GetDirectoryName(destination)); File.Copy(source, destination, true);
            }
            foreach (var workItem in request.Manifest.PlatformCookWorkItems) {
                string source = ContainedPath(StagingRoot, workItem.SourceAssetPath);
                if (!File.Exists(source)) { throw new FileNotFoundException("Builder-owned payload is missing.", source); }
                string destination = ContainedPath(contentRoot, workItem.OutputRelativePath);
                Directory.CreateDirectory(Path.GetDirectoryName(destination)); File.Copy(source, destination, true);
            }
            if (request.ExecutionScope == PlatformBuildExecutionScope.Full) {
                ValidateShaderArtifacts(request, contentRoot);
                cancellationToken.ThrowIfCancellationRequested();
                progress.Report(new PlatformBuildProgressUpdate("WebAssembly", "webgpu", 0, 1, "Compiling generated engine and browser host."));
                string nativeRoot = string.IsNullOrWhiteSpace(request.NativeObjectCacheRoot) ? Path.Combine(request.WorkingRoot, "native") : request.NativeObjectCacheRoot;
                WebGpuNativeBuildResult result = Executor.Build(RepositoryRoot, nativeRoot, request.GeneratedCoreCppRootPath, Path.Combine(StagingRoot, "code"), contentRoot, request.SelectedBuildProfileId, cancellationToken);
                string[] artifacts = [result.JavaScriptPath, result.WebAssemblyPath, result.DataPath];
                foreach (string artifact in artifacts) {
                    if (!File.Exists(artifact) || new FileInfo(artifact).Length == 0) { throw new InvalidOperationException($"Native build omitted required artifact '{artifact}'."); }
                }
                Directory.CreateDirectory(request.OutputRoot);
                foreach (string artifact in artifacts) {
                    string destination = Path.Combine(request.OutputRoot, Path.GetFileName(artifact));
                    File.Copy(artifact, destination, true); File.SetLastWriteTimeUtc(destination, DateTime.UtcNow);
                }
                string page = Path.Combine(RepositoryRoot, "web", "index.html");
                File.Copy(page, Path.Combine(request.OutputRoot, "index.html"), true);
                File.SetLastWriteTimeUtc(Path.Combine(request.OutputRoot, "index.html"), DateTime.UtcNow);
                foreach (string script in new[] { "bootstrap.js", "audio.js" }) {
                    string source = Path.Combine(RepositoryRoot, "web", script);
                    File.Copy(source, Path.Combine(request.OutputRoot, script), true);
                    File.SetLastWriteTimeUtc(Path.Combine(request.OutputRoot, script), DateTime.UtcNow);
                }
                File.WriteAllText(Path.Combine(request.OutputRoot, "build-manifest.json"), JsonSerializer.Serialize(request.Manifest));
            }
            return Task.FromResult(new PlatformBuildReport(true, [],
                request.Manifest.Scenes.Select(scene => new PlatformBuildItemOutcome(scene.SceneId, PlatformBuildItemOutcomeKind.Succeeded)).ToArray(),
                request.Manifest.LooseAssets.Select(asset => new PlatformBuildItemOutcome(asset.AssetId, PlatformBuildItemOutcomeKind.Succeeded)).ToArray()));
        } catch (OperationCanceledException) { throw; }
        catch (Exception error) when (error is IOException || error is InvalidDataException || error is InvalidOperationException || error is ArgumentException || error is NotSupportedException) {
            PlatformBuildDiagnostic diagnostic = new(PlatformBuildDiagnosticSeverity.Error, "WEBGPU_BUILD", error.Message, "", "", StagingRoot);
            diagnostics.Report(diagnostic);
            return Task.FromResult(new PlatformBuildReport(false, [diagnostic],
                request.Manifest.Scenes.Select(scene => new PlatformBuildItemOutcome(scene.SceneId, PlatformBuildItemOutcomeKind.Failed)).ToArray(), []));
        }
    }

    /// <summary>Requires declared renderer dependencies and verifies every cooked shader before native compilation.</summary>
    static void ValidateShaderArtifacts(PlatformBuildRequest request, string contentRoot) {
        HashSet<string> declared = new(StringComparer.Ordinal);
        foreach (var artifact in request.Manifest.CookedArtifacts.Where(value => value.ArtifactKind == "shader")) {
            string relative = artifact.RelativePath.Replace('\\', '/');
            if (relative.StartsWith("cooked/", StringComparison.Ordinal)) { relative = relative[7..]; }
            string path = ContainedPath(contentRoot, "cooked/" + relative);
            if (!File.Exists(path)) { throw new FileNotFoundException("Declared WebGPU shader artifact is missing.", path); }
            WebGpuShaderArtifact shader = WebGpuShaderArtifactSerializer.Deserialize(File.ReadAllBytes(path));
            var metadata = shader.Metadata;
            var dependency = new helengine.baseplatform.Results.PlatformShaderDependency(metadata.GetProperty("shaderAssetId").GetString(),
                metadata.GetProperty("vertexProgram").GetString(), metadata.GetProperty("pixelProgram").GetString(), metadata.GetProperty("variant").GetString());
            if (WebGpuShaderArtifactIdentity.RelativePath(dependency) != relative) { throw new InvalidDataException("Declared WebGPU shader path does not match its selection."); }
            declared.Add(relative);
        }
        foreach (var dependency in new WebGpuPlatformAssetBuilder().RendererShaderDependencies) {
            if (!declared.Contains(WebGpuShaderArtifactIdentity.RelativePath(dependency))) { throw new InvalidDataException("Required renderer shader artifact is not declared: " + dependency.ShaderAssetId + "/" + dependency.VariantName); }
        }
    }

    /// <summary>Requires scene and asset identities to identify existing files inside the staged package.</summary>
    void ValidatePayload(string identity) {
        string path = ContainedPath(StagingRoot, identity);
        if (!File.Exists(path)) { throw new FileNotFoundException($"Required staged payload '{identity}' does not exist.", path); }
    }

    /// <summary>Checks absolute root relationships before removing builder-owned staged content.</summary>
    static bool IsWithin(string candidate, string root) {
        string canonicalRoot = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar);
        string canonicalCandidate = Path.GetFullPath(candidate).TrimEnd(Path.DirectorySeparatorChar);
        return canonicalCandidate.Equals(canonicalRoot, StringComparison.OrdinalIgnoreCase) || canonicalCandidate.StartsWith(canonicalRoot + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase);
    }

    /// <summary>Rejects directory junctions and file links without walking through their targets.</summary>
    static void RejectLinks(string root) {
        if ((File.GetAttributes(root) & FileAttributes.ReparsePoint) != 0) { throw new InvalidOperationException($"Staged symbolic links are unsupported: {root}"); }
        foreach (string entry in Directory.EnumerateFileSystemEntries(root)) {
            FileAttributes attributes = File.GetAttributes(entry);
            if ((attributes & FileAttributes.ReparsePoint) != 0) { throw new InvalidOperationException($"Staged symbolic links are unsupported: {entry}"); }
            if ((attributes & FileAttributes.Directory) != 0) { RejectLinks(entry); }
        }
    }

    /// <summary>Rejects traversal and absolute paths escaping the intended filesystem root.</summary>
    public static string ContainedPath(string root, string path) {
        string canonicalRoot = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
        string candidate = Path.GetFullPath(Path.Combine(canonicalRoot, path.Replace('/', Path.DirectorySeparatorChar)));
        if (!candidate.StartsWith(canonicalRoot, StringComparison.OrdinalIgnoreCase)) { throw new InvalidOperationException($"Path '{path}' escapes root '{root}'."); }
        return candidate;
    }
}
