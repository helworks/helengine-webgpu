using helengine.baseplatform.Requests;
using helengine.webgpu.builder;
using System.Security.Cryptography;
using System.Text;

namespace helengine.webgpu.builder.tests;

/// <summary>Requires backend shadow passes to travel through the same source-snapshot shader cooker as UI and materials.</summary>
public sealed class WebGpuShadowDependencyTests {
    /// <summary>The atlas dependency must identify the original VS/PS source and produce a validated depth-only pair.</summary>
    [Fact]
    public void Renderer_declares_and_cooks_original_atlas_depth_shader() {
        var dependency = Assert.Single(new WebGpuPlatformAssetBuilder().RendererShaderDependencies.Where(value => value.ShaderAssetId == "EditorShadowDepth"));
        Assert.Equal("EditorShadowDepth.vs", dependency.VertexProgramName); Assert.Equal("EditorShadowDepth.ps", dependency.PixelProgramName); Assert.Equal("default", dependency.VariantName);
        string path="C:/dev/helworks/helengine/engine/helengine.editor/shaders/builtin/EditorShadowDepth.hlsl"; string source=File.ReadAllText(path);
        string root="C:/dev/helworks/builds/helengine-webgpu/renderer-parity/shadow-dependencies/"+Guid.NewGuid().ToString("N");
        var result=new WebGpuShaderArtifactCooker().Cook(new PlatformShaderArtifactCookRequest(root,"webgpu","release","webgpu",[dependency],
            [new PlatformShaderArtifactCookSource(dependency.ShaderAssetId,Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(source))),source,path)]));
        var declaration=Assert.Single(result.CookedArtifactDeclarations);
        var artifact=WebGpuShaderArtifactSerializer.Deserialize(File.ReadAllBytes(Path.Combine(root,declaration.RelativePath)));
        Assert.DoesNotContain("@location(0u)",artifact.FragmentWgsl); Assert.Contains("worldViewProj",artifact.Metadata.GetProperty("bindings")[0].GetProperty("sourceLayout").GetRawText());
    }
}
