using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;

namespace helengine.webgpu.builder.tests;

/// <summary>Exercises actual material serialization and rejects incompatible authored shader requests.</summary>
public sealed class WebGpuMaterialCookerTests {
    /// <summary>Detects incorrect channel ordering, renderer selection, or external shader dependencies.</summary>
    [Fact]
    public void Solid_material_round_trips_with_webgpu_family() {
        PlatformMaterialCookResult result = new WebGpuMaterialCooker().Cook(CreateRequest("#12345678", false));
        using MemoryStream stream = new MemoryStream(result.CookedMaterialBytes);
        PlatformMaterialAsset material = Assert.IsType<PlatformMaterialAsset>(helengine.files.AssetSerializer.Deserialize(stream));
        Assert.Equal("cube-solid", material.Id);
        Assert.Equal("webgpu", material.RendererFamilyId);
        Assert.Equal(0x12, material.BaseColorR);
        Assert.Equal(0x34, material.BaseColorG);
        Assert.Equal(0x56, material.BaseColorB);
        Assert.Equal(0x78, material.BaseColorA);
        Assert.True(material.Lit);
        Assert.Empty(result.ReferencedShaderDependencies);
    }

    /// <summary>Rejects a custom shader instead of silently substituting the stock WGSL material.</summary>
    [Fact]
    public void Custom_shader_is_rejected_with_asset_identity() {
        NotSupportedException error = Assert.Throws<NotSupportedException>(() => new WebGpuMaterialCooker().Cook(CreateRequest("#ffffffff", true)));
        Assert.Contains("cube-solid", error.Message);
    }

    /// <summary>Fails during cooking when a material requires the later textured-3D milestone.</summary>
    [Fact]
    public void Texture_requirement_is_rejected_before_native_build() {
        PlatformMaterialCookRequest request = new("textured-cube", "materials/textured.hasset", "webgpu", "release", "webgpu", "standard-shader",
            new Dictionary<string, string> { ["base-color"] = "#ffffffff", ["use-custom-shader"] = "false", ["casts-shadow"] = "false", ["receives-shadow"] = "false", ["texture-relative-path"] = "textures/checker.hasset" });
        NotSupportedException error = Assert.Throws<NotSupportedException>(() => new WebGpuMaterialCooker().Cook(request));
        Assert.Contains("textured-cube", error.Message); Assert.Contains("texture", error.Message);
    }

    /// <summary>Rejects malformed authored colors rather than manufacturing a fallback color.</summary>
    [Theory]
    [InlineData("#gggggg")]
    [InlineData("#12345")]
    [InlineData("")]
    public void Invalid_color_is_rejected(string color) {
        Assert.Throws<InvalidOperationException>(() => new WebGpuMaterialCooker().Cook(CreateRequest(color, false)));
    }

    /// <summary>Creates a complete material input with literal, independently selected fields.</summary>
    static PlatformMaterialCookRequest CreateRequest(string color, bool custom) {
        return new PlatformMaterialCookRequest("cube-solid", "materials/rendering/cube_test/CubeTestSolid.hasset", "webgpu", "release", "webgpu", "standard-shader",
            new Dictionary<string, string> {
                ["base-color"] = color, ["use-custom-shader"] = custom.ToString(),
                ["casts-shadow"] = "false", ["receives-shadow"] = "false"
            });
    }
}
