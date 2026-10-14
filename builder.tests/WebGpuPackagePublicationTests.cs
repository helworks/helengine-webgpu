namespace helengine.webgpu.builder.tests;

/// <summary>Requires complete, self-contained packages without stale cached content or unsafe root overlap.</summary>
public sealed class WebGpuPackagePublicationTests {
    /// <summary>Requires all executable package artifacts to be published together after validation.</summary>
    [Fact]
    public async Task Publication_requires_all_four_nonempty_artifacts() {
        WebGpuBuildFixture fixture = new(); TestBuildReporter reporter = new(); var request = fixture.CreateRequest();
        DateTime started = DateTime.UtcNow;
        var report = await new WebGpuBuildWorkspace(new TestNativeBuildExecutor(), Path.Combine(fixture.Root, "staged"), fixture.Root).BuildAsync(request, reporter, reporter, CancellationToken.None);
        Assert.True(report.Succeeded);
        foreach (string artifact in new[] { "index.html", "helengine_webgpu.js", "helengine_webgpu.wasm", "helengine_webgpu.data" }) {
            FileInfo file = new(Path.Combine(request.OutputRoot, artifact)); Assert.True(file.Length > 0); Assert.True(file.LastWriteTimeUtc >= started, $"{artifact} must be freshly published for Build Waiter.");
        }
    }
    /// <summary>Rejects a missing page instead of returning success with only native files.</summary>
    [Fact]
    public async Task Missing_page_is_not_success() {
        WebGpuBuildFixture fixture = new(); TestNativeBuildExecutor executor = new(); TestBuildReporter reporter = new();
        File.Delete(Path.Combine(fixture.Root, "web", "index.html"));
        var report = await new WebGpuBuildWorkspace(executor, Path.Combine(fixture.Root, "staged"), fixture.Root).BuildAsync(fixture.CreateRequest(), reporter, reporter, CancellationToken.None);
        Assert.False(report.Succeeded); Assert.Equal(0, executor.Calls);
    }
    /// <summary>Ensures removed scene dependencies do not survive in the next preloaded package.</summary>
    [Fact]
    public async Task Removed_content_does_not_survive_in_reused_workspace() {
        WebGpuBuildFixture fixture = new(); TestBuildReporter reporter = new(); var request = fixture.CreateRequest();
        string stale = Path.Combine(request.WorkingRoot, "content", "cooked", "old.hasset");
        Directory.CreateDirectory(Path.GetDirectoryName(stale)); File.WriteAllText(stale, "removed asset");
        var report = await new WebGpuBuildWorkspace(new TestNativeBuildExecutor(), Path.Combine(fixture.Root, "staged"), fixture.Root).BuildAsync(request, reporter, reporter, CancellationToken.None);
        Assert.True(report.Succeeded); Assert.False(File.Exists(stale));
    }
}
