namespace helengine.webgpu.builder.tests;

/// <summary>Protects the native bridge's use of the engine's existing camera-relative transparency contract.</summary>
public sealed class WebGpuRendererContractTests {
    /// <summary>The published schema must not send users back to an implementation or source decision already completed.</summary>
    [Fact]
    public void Schema_status_does_not_claim_runtime_or_source_approval_is_pending() {
        string schema = File.ReadAllText("C:/dev/helworks/helengine-webgpu/docs/WebGpuArtifactSchema.md");
        Assert.DoesNotContain("not implemented yet", schema);
        Assert.DoesNotContain("awaiting Helena's decision", schema);
    }
    /// <summary>RenderList3D already owns transparent classification and camera ordering; the backend must call that entry point.</summary>
    [Fact]
    public void Native_renderer_uses_shared_camera_transparency_order() {
        string source = File.ReadAllText("C:/dev/helworks/helengine-webgpu/src/platform/webgpu/webgpu_render_manager_3d.cpp");
        Assert.Contains("VisitCameraOrdered(this, camera)", source);
    }
}
