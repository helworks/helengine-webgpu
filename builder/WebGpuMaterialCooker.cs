using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;
using System.Globalization;

namespace helengine.webgpu.builder;

/// <summary>Translates authored basic materials to the engine's platform-owned asset format.</summary>
public sealed class WebGpuMaterialCooker {
    /// <summary>Serializes one basic WGSL material and rejects unsupported shader requirements.</summary>
    public PlatformMaterialCookResult Cook(PlatformMaterialCookRequest request) {
        ArgumentNullException.ThrowIfNull(request);
        if (request.TargetPlatformId != "webgpu" || request.SelectedGraphicsProfileId != "webgpu" || request.SchemaId != "standard-shader") {
            throw new NotSupportedException($"Material '{request.MaterialAssetId}' requires unsupported platform, graphics profile or schema.");
        }
        if (ReadBoolean(request, "use-custom-shader") || ReadBoolean(request, "casts-shadow") || ReadBoolean(request, "receives-shadow")) {
            throw new NotSupportedException($"Material '{request.MaterialAssetId}' requires custom shaders or shadows not supported by WebGPU's basic material.");
        }
        if (!request.FieldValues.TryGetValue("base-color", out string color) || color.Length != 7 && color.Length != 9 || color[0] != '#'
            || !uint.TryParse(color.AsSpan(1), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint rgba)) {
            throw new InvalidOperationException($"Material '{request.MaterialAssetId}' requires base-color in #RRGGBB or #RRGGBBAA format.");
        }
        if (color.Length == 7) {
            rgba = (rgba << 8) | 255;
        }
        request.FieldValues.TryGetValue("texture-relative-path", out string texture);
        if (!string.IsNullOrEmpty(texture)) { throw new NotSupportedException($"Material '{request.MaterialAssetId}' requires texture-relative-path; textured 3D materials are not supported by this WebGPU milestone."); }
        PlatformMaterialAsset material = new PlatformMaterialAsset {
            Id = request.MaterialAssetId, RendererFamilyId = "webgpu", TextureRelativePath = texture ?? string.Empty,
            Lit = true, DoubleSided = false, UseVertexColor = false,
            BaseColorR = (byte)(rgba >> 24), BaseColorG = (byte)(rgba >> 16), BaseColorB = (byte)(rgba >> 8), BaseColorA = (byte)rgba
        };
        return new PlatformMaterialCookResult(helengine.files.AssetSerializer.SerializeToBytes(material), []);
    }

    /// <summary>Validates a required authored boolean, identifying the asset and field on failure.</summary>
    static bool ReadBoolean(PlatformMaterialCookRequest request, string field) {
        if (!request.FieldValues.TryGetValue(field, out string value) || !bool.TryParse(value, out bool parsed)) {
            throw new InvalidOperationException($"Material '{request.MaterialAssetId}' requires boolean field '{field}'.");
        }
        return parsed;
    }
}
