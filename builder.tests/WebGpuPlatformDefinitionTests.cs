using helengine.baseplatform.Definitions;
using helengine.baseplatform.Profiles;

namespace helengine.webgpu.builder.tests;

/// <summary>Protects the platform metadata consumed by editor cooking and generated gameplay.</summary>
public sealed class WebGpuPlatformDefinitionTests {
    /// <summary>The editor's declared defaults form a complete cook request with explicit shader selection.</summary>
    [Fact]
    public void Schema_defaults_supply_the_complete_shared_material_contract() {
        var schema = Assert.Single(WebGpuPlatformDefinitionFactory.Create().MaterialSchemas);
        var fields = schema.Fields.ToDictionary(field => field.FieldId, field => field.DefaultValue);
        var material = WebGpuMaterialArtifactSerializer.Deserialize(new WebGpuMaterialCooker().Cook(WebGpuMaterialCookerTests.Request(fields)).CookedMaterialBytes);
        Assert.Equal("ForwardStandardShader", material.Metadata.GetProperty("shaderAssetId").GetString());
        Assert.Equal("default", material.Metadata.GetProperty("variant").GetString());
        Assert.Equal(5, material.Metadata.GetProperty("constantBuffers").GetArrayLength());
    }
    /// <summary>Prevents a web build from losing desktop keyboard branches or selecting another output language.</summary>
    [Fact]
    public void Profiles_expose_webgpu_cpp_and_desktop_controls() {
        PlatformDefinition definition = WebGpuPlatformDefinitionFactory.Create();
        Assert.Equal("webgpu", definition.PlatformId);
        Assert.Equal(RuntimeMaterialResolutionMode.CookedPlatformOwned, definition.RuntimeGenerationContract.MaterialResolutionMode);
        Assert.Equal(new[] { "debug", "release" }, definition.BuildProfiles.Select(profile => profile.ProfileId));
        PlatformCodegenProfileDefinition codegen = Assert.Single(definition.CodegenProfiles);
        Assert.Equal(PlatformCodegenLanguage.Cpp, codegen.OutputLanguage);
        Assert.Equal(PlatformSerializationEndianness.LittleEndian, codegen.Endianness);
        Assert.Contains(codegen.Settings, setting => setting.SettingId == "additional-preprocessor-symbols" && setting.DefaultValue == "DESKTOP_PLATFORM");
        Assert.Contains(codegen.Settings, setting => setting.SettingId == PlatformCodegenSettingIds.EnabledFeatures && setting.DefaultValue.Contains("host_file_system"));
        Assert.Contains(codegen.Settings, setting => setting.SettingId == "generated-math-convention" && setting.DefaultValue == "engine-row-vector");
        Assert.Contains(codegen.Settings, setting => setting.SettingId == "pointer-size-bytes" && setting.DefaultValue == "4");
    }
}
