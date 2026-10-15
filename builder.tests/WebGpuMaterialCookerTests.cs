using helengine.baseplatform.Requests;
using helengine.webgpu.builder;
using System.Buffers.Binary;

namespace helengine.webgpu.builder.tests;

/// <summary>Checks authored shader selection, PBR constants, state and imported texture preservation.</summary>
public sealed class WebGpuMaterialCookerTests {
    /// <summary>Colors and explicit shader identities survive the platform-owned material format.</summary>
    [Fact]
    public void Solid_material_round_trips_with_original_shader_selection() {
        var fields = Fields(); fields["base-color"] = "#12345678";
        var result = new WebGpuMaterialCooker().Cook(Request(fields));
        var artifact = WebGpuMaterialArtifactSerializer.Deserialize(result.CookedMaterialBytes);
        Assert.Equal("cube-solid", artifact.Metadata.GetProperty("materialAssetId").GetString());
        Assert.Equal("ForwardStandardShader", artifact.Metadata.GetProperty("shaderAssetId").GetString());
        byte[] color = Convert.FromBase64String(artifact.Metadata.GetProperty("constantBuffers").EnumerateArray().Single(value => value.GetProperty("name").GetString() == "BaseColorBuffer").GetProperty("data").GetString());
        Assert.Equal(0x12 / 255f, BinaryPrimitives.ReadSingleLittleEndian(color));
        Assert.Equal(0x78 / 255f, BinaryPrimitives.ReadSingleLittleEndian(color.AsSpan(12)));
        Assert.Equal("default", Assert.Single(result.ReferencedShaderDependencies).VariantName);
    }

    /// <summary>Custom selections reach dependencies without being replaced by the built-in material.</summary>
    [Fact]
    public void Custom_shader_preserves_program_pair() {
        var fields = Fields(); fields["use-custom-shader"] = "true"; fields["shader-asset-id"] = "custom";
        fields["vertex-program"] = "custom.vs"; fields["pixel-program"] = "custom.ps";
        var result = new WebGpuMaterialCooker().Cook(Request(fields));
        Assert.Equal("custom", Assert.Single(result.ReferencedShaderDependencies).ShaderAssetId);
        Assert.Equal("custom.vs", WebGpuMaterialArtifactSerializer.Deserialize(result.CookedMaterialBytes).Metadata.GetProperty("vertexProgram").GetString());
    }

    /// <summary>PBR values, shadow flags, culling, blending and all standard texture slots remain explicit.</summary>
    [Fact]
    public void Pbr_material_preserves_values_state_and_textures() {
        var fields = Fields(); fields["roughness"] = "0.25"; fields["metallic"] = "0.75"; fields["specular"] = "0.6";
        fields["emissive-color"] = "#80402080"; fields["casts-shadow"] = "true"; fields["receives-shadow"] = "true";
        fields["double-sided"] = "true"; fields["alpha-mode"] = "alpha-blend"; fields["vertex-color-mode"] = "multiply"; fields["lighting-mode"] = "unlit";
        fields["texture-id"] = "albedo"; fields["texture-relative-path"] = "cooked/imported/albedo";
        fields["roughness-texture-id"] = "rough"; fields["emissive-texture-id"] = "emissive";
        var artifact = WebGpuMaterialArtifactSerializer.Deserialize(new WebGpuMaterialCooker().Cook(Request(fields)).CookedMaterialBytes);
        Assert.True(artifact.Metadata.GetProperty("castsShadows").GetBoolean()); Assert.True(artifact.Metadata.GetProperty("receivesShadows").GetBoolean());
        var state = artifact.Metadata.GetProperty("renderState"); Assert.True(state.GetProperty("doubleSided").GetBoolean());
        Assert.True(state.GetProperty("useVertexColor").GetBoolean()); Assert.False(state.GetProperty("lit").GetBoolean());
        Assert.Equal("alpha-blend", state.GetProperty("alphaMode").GetString());
        var textures = artifact.Metadata.GetProperty("textures").EnumerateArray().ToArray(); Assert.Equal(3, textures.Length);
        Assert.Equal("cooked/imported/albedo", textures.Single(value => value.GetProperty("name").GetString() == "DiffuseTexture").GetProperty("path").GetString());
        Assert.Equal("cooked/imported/rough", textures.Single(value => value.GetProperty("name").GetString() == "RoughnessTexture").GetProperty("path").GetString());
        foreach (var buffer in artifact.Metadata.GetProperty("constantBuffers").EnumerateArray()) {
            string name = buffer.GetProperty("name").GetString();
            float first = BinaryPrimitives.ReadSingleLittleEndian(Convert.FromBase64String(buffer.GetProperty("data").GetString()));
            if (name == "RoughnessBuffer") Assert.Equal(0.25f, first);
            if (name == "MetallicBuffer") Assert.Equal(0.75f, first);
            if (name == "SpecularBuffer") Assert.Equal(0.6f, first);
        }
    }

    /// <summary>Required shader selection cannot be manufactured when the caller omits it.</summary>
    [Fact]
    public void Missing_shader_selection_is_not_filled_with_a_fallback() {
        var fields = Fields(); fields.Remove("shader-asset-id");
        Assert.Contains("cube-solid", Assert.Throws<InvalidOperationException>(() => new WebGpuMaterialCooker().Cook(Request(fields))).Message);
    }

    /// <summary>Malformed authored colors fail with a diagnostic before native compilation.</summary>
    [Theory]
    [InlineData("#gggggg")]
    [InlineData("#12345")]
    [InlineData("")]
    public void Invalid_color_is_rejected(string color) {
        var fields = Fields(); fields["base-color"] = color;
        Assert.Throws<InvalidOperationException>(() => new WebGpuMaterialCooker().Cook(Request(fields)));
    }

    /// <summary>Supplies independent literal values for every declared field.</summary>
    internal static Dictionary<string, string> Fields() => new(StringComparer.Ordinal) {
        ["shader-asset-id"] = "ForwardStandardShader", ["vertex-program"] = "ForwardStandardShader.vs", ["pixel-program"] = "ForwardStandardShader.ps", ["variant"] = "default",
        ["use-custom-shader"] = "false", ["base-color"] = "#ffffffff", ["roughness"] = "0.4", ["metallic"] = "0", ["specular"] = "0.5", ["emissive-color"] = "#00000000",
        ["casts-shadow"] = "false", ["receives-shadow"] = "false", ["double-sided"] = "false", ["alpha-mode"] = "opaque", ["lighting-mode"] = "lit", ["vertex-color-mode"] = "ignore",
        ["texture-id"] = "", ["texture-relative-path"] = "", ["roughness-texture-id"] = "", ["emissive-texture-id"] = "", ["constant-buffers"] = "", ["depth-test"] = "true", ["depth-write"] = "true"
    };
    /// <summary>Uses the existing unmodified editor-to-builder request signature.</summary>
    internal static PlatformMaterialCookRequest Request(IReadOnlyDictionary<string, string> fields) => new("cube-solid", "materials/cube.hasset", "webgpu", "release", "webgpu", "standard-shader", fields);
}
