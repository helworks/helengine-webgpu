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
            [new PlatformGraphicsProfileDefinition("webgpu", "WebGPU", "Browser WebGPU forward lighting, depth shadows and desktop UI",
                [Setting("default-width", "Canvas Width", "1280"), Setting("default-height", "Canvas Height", "720")])],
            [new PlatformAssetRequirementDefinition("scene", "Scene", true, ["helen"]),
             new PlatformAssetRequirementDefinition("texture", "Texture", true, ["png", "tga", "jpg", "bmp"])],
            [new PlatformMaterialSchemaDefinition("standard-shader", "Standard WebGPU Material", ["webgpu"],
                [new PlatformMaterialFieldDefinition("base-color", "Base Color", PlatformMaterialFieldKind.Color, "#ffffffff", true, []),
                 new PlatformMaterialFieldDefinition("use-custom-shader", "Custom Shader", PlatformMaterialFieldKind.Boolean, "false", true, []),
                 MaterialField("shader-asset-id", "Shader", PlatformMaterialFieldKind.AssetReference, "ForwardStandardShader"),
                 MaterialField("vertex-program", "Vertex Program", PlatformMaterialFieldKind.Text, "ForwardStandardShader.vs"),
                 MaterialField("pixel-program", "Pixel Program", PlatformMaterialFieldKind.Text, "ForwardStandardShader.ps"),
                 MaterialField("variant", "Variant", PlatformMaterialFieldKind.Text, "default"),
                 MaterialField("roughness", "Roughness", PlatformMaterialFieldKind.Text, "0.4"),
                 MaterialField("metallic", "Metallic", PlatformMaterialFieldKind.Text, "0"),
                 MaterialField("specular", "Specular", PlatformMaterialFieldKind.Text, "0.5"),
                 MaterialField("emissive-color", "Emissive Color", PlatformMaterialFieldKind.Color, "#ffffff00"),
                 MaterialField("alpha-mode", "Alpha Mode", PlatformMaterialFieldKind.Text, "opaque"),
                 MaterialField("double-sided", "Double Sided", PlatformMaterialFieldKind.Boolean, "false"),
                 MaterialField("vertex-color-mode", "Vertex Color", PlatformMaterialFieldKind.Text, "ignore"),
                 MaterialField("lighting-mode", "Lighting", PlatformMaterialFieldKind.Text, "lit"),
                 MaterialField("depth-test", "Depth Test", PlatformMaterialFieldKind.Boolean, "true"),
                 MaterialField("depth-write", "Depth Write", PlatformMaterialFieldKind.Boolean, "true"),
                 MaterialField("texture-id", "Diffuse Texture", PlatformMaterialFieldKind.AssetReference, "", false),
                 MaterialField("roughness-texture-id", "Roughness Texture", PlatformMaterialFieldKind.AssetReference, "", false),
                 MaterialField("emissive-texture-id", "Emissive Texture", PlatformMaterialFieldKind.AssetReference, "", false),
                 MaterialField("constant-buffers", "Named Constant Buffers", PlatformMaterialFieldKind.Text, "", false),
                 new PlatformMaterialFieldDefinition("casts-shadow", "Casts Shadows", PlatformMaterialFieldKind.Boolean, "false", true, []),
                 new PlatformMaterialFieldDefinition("receives-shadow", "Receives Shadows", PlatformMaterialFieldKind.Boolean, "false", true, []),
                 new PlatformMaterialFieldDefinition("texture-relative-path", "Texture", PlatformMaterialFieldKind.AssetReference, "", false, [])])],
            [new PlatformComponentSupportRule("helengine.directionallightcomponent", PlatformComponentSupportKind.PassThrough, "Forward directional lighting and depth-atlas shadows.", ""),
             new PlatformComponentSupportRule("helengine.pointlightcomponent", PlatformComponentSupportKind.PassThrough, "Forward point lighting and radial depth-cubemap shadows.", ""),
             new PlatformComponentSupportRule("helengine.spotlightcomponent", PlatformComponentSupportKind.PassThrough, "Forward spot lighting and depth-atlas shadows.", "")],
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

    /// <summary>Declares the shared material field contract and its explicit editor default.</summary>
    static PlatformMaterialFieldDefinition MaterialField(string id, string name, PlatformMaterialFieldKind kind, string value, bool required = true) {
        return new PlatformMaterialFieldDefinition(id, name, kind, value, required, []);
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
