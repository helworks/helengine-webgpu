using helengine.baseplatform.Definitions;
using helengine.baseplatform.Profiles;

namespace helengine.webgpu.builder;

/// <summary>Builds platform metadata used by the editor and native runtime generation.</summary>
public static class WebGpuPlatformDefinitionFactory {
    /// <summary>Returns the WebGPU build, graphics, material and codegen definitions.</summary>
    public static PlatformDefinition Create() {
        return new PlatformDefinition("webgpu", "WebGPU",
            [new PlatformBuildProfileDefinition("debug", "Debug", "Browser player with runtime diagnostics", "webgpu", "default", []),
             new PlatformBuildProfileDefinition("release", "Release", "Optimized browser player", "webgpu", "default", [])],
            [new PlatformGraphicsProfileDefinition("webgpu", "WebGPU", "Browser WebGPU with a directional light and desktop UI",
                [Setting("default-width", "Canvas Width", "1280"), Setting("default-height", "Canvas Height", "720")])],
            [new PlatformAssetRequirementDefinition("scene", "Scene", true, ["helen"]),
             new PlatformAssetRequirementDefinition("texture", "Texture", true, ["png", "tga", "jpg", "bmp"])],
            [new PlatformMaterialSchemaDefinition("standard-shader", "Basic WebGPU Material", ["webgpu"],
                [new PlatformMaterialFieldDefinition("base-color", "Base Color", PlatformMaterialFieldKind.Color, "#ffffffff", true, []),
                 new PlatformMaterialFieldDefinition("use-custom-shader", "Custom Shader", PlatformMaterialFieldKind.Boolean, "false", true, []),
                 new PlatformMaterialFieldDefinition("casts-shadow", "Casts Shadows", PlatformMaterialFieldKind.Boolean, "false", true, []),
                 new PlatformMaterialFieldDefinition("receives-shadow", "Receives Shadows", PlatformMaterialFieldKind.Boolean, "false", true, []),
                 new PlatformMaterialFieldDefinition("texture-relative-path", "Texture", PlatformMaterialFieldKind.AssetReference, "", false, [])])],
            [new PlatformComponentSupportRule("helengine.directionallightcomponent", PlatformComponentSupportKind.PassThrough, "One directional light is supported without shadows.", ""),
             new PlatformComponentSupportRule("helengine.pointlightcomponent", PlatformComponentSupportKind.Unsupported, "Point lights are not supported by the initial WebGPU renderer.", ""),
             new PlatformComponentSupportRule("helengine.spotlightcomponent", PlatformComponentSupportKind.Unsupported, "Spot lights are not supported by the initial WebGPU renderer.", "")],
            [new PlatformCodegenProfileDefinition("default", "C++", "C# gameplay compiled to WebAssembly", PlatformCodegenLanguage.Cpp, PlatformSerializationEndianness.LittleEndian,
                [Setting("additional-preprocessor-symbols", "Gameplay Symbols", "DESKTOP_PLATFORM"),
                 Setting("generated-math-convention", "Engine Matrix Convention", "engine-row-vector"),
                 Setting("pointer-size-bytes", "WebAssembly Pointer Size", "4"),
                 Setting(PlatformCodegenSettingIds.EnabledFeatures, "Runtime Features", "host_file_system"),
                 BooleanSetting("load-native-runtime-metadata", "Load Runtime Metadata", "true"),
                 BooleanSetting("write-conversion-report", "Conversion Report", "true"),
                 BooleanSetting("codegen-use-rtti", "RTTI", "true"),
                 BooleanSetting("codegen-use-exceptions", "Exceptions", "true")])],
            [new PlatformStorageProfileDefinition("loose-files", "Preloaded Files", PlatformStorageProfileKind.LooseFiles, "webgpu-preload", false)],
            [new PlatformMediaProfileDefinition("web-package", "Browser Package", PlatformMediaLayoutKind.InstallTree, false, false)],
            new RuntimeGenerationContract(RuntimeMaterialResolutionMode.CookedPlatformOwned, true, PackagedPathPolicy.ContentRelativeOnly, []));
    }

    /// <summary>Creates a required textual setting with an explicit default supplied by the platform.</summary>
    static PlatformSettingDefinition Setting(string id, string name, string value) {
        return new PlatformSettingDefinition(id, name, PlatformSettingKind.Text, value, true, []);
    }

    /// <summary>Creates a boolean codegen setting interpreted by the shared converter.</summary>
    static PlatformSettingDefinition BooleanSetting(string id, string name, string value) {
        return new PlatformSettingDefinition(id, name, PlatformSettingKind.Boolean, value, true, []);
    }
}
