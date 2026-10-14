using helengine.baseplatform.Definitions;
using helengine.baseplatform.Profiles;

namespace helengine.webgpu.builder.tests;

/// <summary>Protects the platform metadata consumed by editor cooking and generated gameplay.</summary>
public sealed class WebGpuPlatformDefinitionTests {
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
    }
}
