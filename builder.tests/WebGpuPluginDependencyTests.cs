using System.Xml.Linq;

namespace helengine.webgpu.builder.tests;

/// <summary>Protects dependencies needed when the editor loads the builder outside its own dependency graph.</summary>
public sealed class WebGpuPluginDependencyTests {
    /// <summary>NuGet runtime assemblies must be copied beside the standalone dynamically loaded plugin.</summary>
    [Fact]
    public void Builder_copies_its_private_runtime_dependencies() {
        string project = "C:/dev/helworks/helengine-webgpu/builder/helengine.webgpu.builder.csproj";
        XDocument document = XDocument.Load(project);
        Assert.Contains(document.Descendants("CopyLocalLockFileAssemblies"), value => value.Value == "true");
    }
}
