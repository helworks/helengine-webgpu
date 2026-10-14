using helengine.baseplatform.Builders;
using helengine.baseplatform.Reporting;

namespace helengine.webgpu.builder.tests;

/// <summary>Collects the real builder's emitted diagnostics while accepting progress updates.</summary>
public sealed class TestBuildReporter : IPlatformBuildProgressReporter, IPlatformBuildDiagnosticReporter {
    /// <summary>Stores diagnostics for assertions on failed builds.</summary>
    public List<PlatformBuildDiagnostic> Diagnostics { get; } = [];
    /// <summary>Accepts progress updates without replacing builder behavior.</summary>
    public void Report(PlatformBuildProgressUpdate update) { }
    /// <summary>Retains each diagnostic in the order produced by the builder.</summary>
    public void Report(PlatformBuildDiagnostic diagnostic) { Diagnostics.Add(diagnostic); }
}
