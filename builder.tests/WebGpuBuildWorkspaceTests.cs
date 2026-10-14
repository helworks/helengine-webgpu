using helengine.baseplatform.Requests;

namespace helengine.webgpu.builder.tests;

/// <summary>Protects validation, cancellation and package publication across the compiler boundary.</summary>
public sealed class WebGpuBuildWorkspaceTests {
    /// <summary>Ensures a missing required scene cannot start native compilation.</summary>
    [Fact]
    public async Task Missing_payload_fails_before_native_compile() {
        WebGpuBuildFixture fixture = new(); TestNativeBuildExecutor executor = new(); TestBuildReporter reporter = new();
        var result = await new WebGpuBuildWorkspace(executor, Path.Combine(fixture.Root, "staged"), fixture.Root).BuildAsync(fixture.CreateRequest("cooked/missing.helen"), reporter, reporter, CancellationToken.None);
        Assert.False(result.Succeeded); Assert.Equal(0, executor.Calls); Assert.Single(reporter.Diagnostics);
    }
    /// <summary>Requires cancellation to propagate rather than become a successful package.</summary>
    [Fact]
    public async Task Cancellation_propagates() {
        WebGpuBuildFixture fixture = new(); TestBuildReporter reporter = new();
        await Assert.ThrowsAnyAsync<OperationCanceledException>(() => new WebGpuBuildWorkspace(new TestNativeBuildExecutor(), Path.Combine(fixture.Root, "staged"), fixture.Root).BuildAsync(fixture.CreateRequest(), reporter, reporter, new CancellationToken(true)));
    }
    /// <summary>Rejects a compiler result that omitted the executable WASM.</summary>
    [Fact]
    public async Task Missing_wasm_is_not_success() {
        WebGpuBuildFixture fixture = new(); TestNativeBuildExecutor executor = new() { OmitWasm = true }; TestBuildReporter reporter = new();
        var result = await new WebGpuBuildWorkspace(executor, Path.Combine(fixture.Root, "staged"), fixture.Root).BuildAsync(fixture.CreateRequest(), reporter, reporter, CancellationToken.None);
        Assert.False(result.Succeeded); Assert.Equal(1, executor.Calls); Assert.Contains(result.Diagnostics, diagnostic => diagnostic.Message.Contains("wasm"));
    }
    /// <summary>Prevents a scene source from escaping the staged package.</summary>
    [Fact]
    public async Task Staging_escape_is_rejected() {
        WebGpuBuildFixture fixture = new(); TestNativeBuildExecutor executor = new(); TestBuildReporter reporter = new();
        File.WriteAllText(Path.Combine(fixture.Root, "outside.helen"), "must not package");
        var result = await new WebGpuBuildWorkspace(executor, Path.Combine(fixture.Root, "staged"), fixture.Root).BuildAsync(fixture.CreateRequest("../outside.helen"), reporter, reporter, CancellationToken.None);
        Assert.False(result.Succeeded); Assert.Equal(0, executor.Calls);
    }
    /// <summary>Stages content for assets-only execution without invoking the compiler or publishing output.</summary>
    [Fact]
    public async Task AssetsOnly_stages_without_native_compile() {
        WebGpuBuildFixture fixture = new(); TestNativeBuildExecutor executor = new(); TestBuildReporter reporter = new();
        var request = fixture.CreateRequest(scope: PlatformBuildExecutionScope.AssetsOnly);
        var result = await new WebGpuBuildWorkspace(executor, Path.Combine(fixture.Root, "staged"), fixture.Root).BuildAsync(request, reporter, reporter, CancellationToken.None);
        Assert.True(result.Succeeded); Assert.Equal(0, executor.Calls);
        Assert.True(File.Exists(Path.Combine(request.WorkingRoot, "content", "cooked", "scenes", "cube.helen")));
        Assert.False(Directory.Exists(request.OutputRoot));
    }
}
