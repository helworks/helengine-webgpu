using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;
using System.Buffers.Binary;
using System.Globalization;
using System.Text.Json;

namespace helengine.webgpu.builder;

/// <summary>Preserves explicit shader selections, PBR values and texture references in WGPM materials.</summary>
public sealed class WebGpuMaterialCooker {
    /// <summary>Cooks the existing field contract without replacing missing required selections.</summary>
    public PlatformMaterialCookResult Cook(PlatformMaterialCookRequest request) {
        ArgumentNullException.ThrowIfNull(request);
        if (request.TargetPlatformId != "webgpu" || request.SelectedGraphicsProfileId != "webgpu" || request.SchemaId != "standard-shader") {
            throw new NotSupportedException($"Material '{request.MaterialAssetId}' has an unsupported platform/profile/schema.");
        }
        try {
            PlatformShaderDependency dependency = new(Text(request, "shader-asset-id"), Text(request, "vertex-program"), Text(request, "pixel-program"), Text(request, "variant"));
            Boolean(request, "use-custom-shader");
            Dictionary<string, string> constants = new(StringComparer.Ordinal) {
                ["BaseColorBuffer"] = Color(request, "base-color"),
                ["RoughnessBuffer"] = Convert.ToBase64String(StandardMaterialScalarDefaults.CreateConstantBufferData(Scalar(request, "roughness"))),
                ["MetallicBuffer"] = Convert.ToBase64String(StandardMaterialScalarDefaults.CreateConstantBufferData(Scalar(request, "metallic"))),
                ["SpecularBuffer"] = Convert.ToBase64String(StandardMaterialScalarDefaults.CreateConstantBufferData(Scalar(request, "specular"))),
                ["EmissiveColorBuffer"] = Color(request, "emissive-color")
            };
            if (request.FieldValues.TryGetValue("constant-buffers", out string raw) && !string.IsNullOrWhiteSpace(raw)) {
                JsonElement buffers = WebGpuArtifactJson.Parse(WebGpuArtifactJson.Utf8.GetBytes(raw));
                if (buffers.ValueKind != JsonValueKind.Array) { throw new InvalidDataException("constant-buffers must be an array."); }
                HashSet<string> names = new(StringComparer.Ordinal);
                foreach (JsonElement buffer in buffers.EnumerateArray()) {
                    string name = WebGpuArtifactJson.Text(buffer, "name"); string data = WebGpuArtifactJson.Text(buffer, "data", WebGpuMaterialArtifactSerializer.MaximumJsonBytes);
                    if (!names.Add(name)) { throw new InvalidDataException("Raw material constant buffer name is duplicated: " + name); }
                    constants[name] = Convert.ToBase64String(Convert.FromBase64String(data));
                }
            }
            List<object> textures = [];
            AddTexture(request, textures, "DiffuseTexture", "texture-id", "texture-relative-path");
            AddTexture(request, textures, "RoughnessTexture", "roughness-texture-id", "roughness-texture-relative-path");
            AddTexture(request, textures, "EmissiveTexture", "emissive-texture-id", "emissive-texture-relative-path");
            string vertexColor = Choice(request, "vertex-color-mode", ["ignore", "multiply"]);
            string lighting = Choice(request, "lighting-mode", ["lit", "unlit"]);
            JsonElement metadata = JsonSerializer.SerializeToElement(new {
                materialAssetId = request.MaterialAssetId, shaderAssetId = dependency.ShaderAssetId,
                vertexProgram = dependency.VertexProgramName, pixelProgram = dependency.PixelProgramName, variant = dependency.VariantName,
                shaderPath = "cooked/" + WebGpuShaderArtifactIdentity.RelativePath(dependency),
                castsShadows = Boolean(request, "casts-shadow"), receivesShadows = Boolean(request, "receives-shadow"),
                renderState = new { alphaMode = Choice(request, "alpha-mode", ["opaque", "alpha-blend", "additive"]),
                    doubleSided = Boolean(request, "double-sided"), useVertexColor = vertexColor == "multiply", lit = lighting == "lit",
                    depthTest = Boolean(request, "depth-test"), depthWrite = Boolean(request, "depth-write") },
                constantBuffers = constants.OrderBy(value => value.Key, StringComparer.Ordinal).Select(value => new { name = value.Key, data = value.Value }).ToArray(), textures
            }, WebGpuArtifactJson.Options);
            return new PlatformMaterialCookResult(WebGpuMaterialArtifactSerializer.Serialize(new WebGpuMaterialArtifact(metadata)), [dependency]);
        } catch (Exception error) when (error is InvalidDataException || error is FormatException || error is ArgumentException) {
            throw new InvalidOperationException($"Material '{request.MaterialAssetId}' could not be cooked: {error.Message}", error);
        }
    }

    /// <summary>Reads required values with an actionable asset/field diagnostic.</summary>
    static string Text(PlatformMaterialCookRequest request, string field) {
        if (!request.FieldValues.TryGetValue(field, out string value) || string.IsNullOrWhiteSpace(value)) { throw new InvalidOperationException($"Material '{request.MaterialAssetId}' requires field '{field}'."); }
        return value;
    }
    /// <summary>Parses required booleans without generating fallback state.</summary>
    static bool Boolean(PlatformMaterialCookRequest request, string field) {
        if (!bool.TryParse(Text(request, field), out bool value)) { throw new InvalidOperationException($"Material '{request.MaterialAssetId}' has invalid boolean '{field}'."); }
        return value;
    }
    /// <summary>Validates authored enumerated modes rather than silently reducing them.</summary>
    static string Choice(PlatformMaterialCookRequest request, string field, IReadOnlyList<string> choices) {
        string value = Text(request, field);
        if (!choices.Contains(value, StringComparer.Ordinal)) { throw new InvalidOperationException($"Material '{request.MaterialAssetId}' has unsupported '{field}' value '{value}'."); }
        return value;
    }
    /// <summary>Accepts only finite normalized scalar material values.</summary>
    static float Scalar(PlatformMaterialCookRequest request, string field) {
        if (!float.TryParse(Text(request, field), NumberStyles.Float, CultureInfo.InvariantCulture, out float value) || !float.IsFinite(value) || value < 0 || value > 1) { throw new InvalidOperationException($"Material '{request.MaterialAssetId}' has invalid scalar '{field}'."); }
        return value;
    }
    /// <summary>Packs the exact authored RGBA channels into the shared float4 constant layout.</summary>
    static string Color(PlatformMaterialCookRequest request, string field) {
        string value = Text(request, field);
        if (value.Length != 7 && value.Length != 9 || value[0] != '#' || !uint.TryParse(value.AsSpan(1), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out uint rgba)) { throw new InvalidOperationException($"Material '{request.MaterialAssetId}' requires '{field}' in #RRGGBB or #RRGGBBAA format."); }
        if (value.Length == 7) { rgba = (rgba << 8) | 255; }
        byte[] data = new byte[16];
        for (int index = 0; index < 4; index++) { BinaryPrimitives.WriteSingleLittleEndian(data.AsSpan(index * 4), ((rgba >> ((3 - index) * 8)) & 255) / 255f); }
        return Convert.ToBase64String(data);
    }
    /// <summary>Preserves explicit imported paths or derives the engine's existing extensionless imported-asset path.</summary>
    static void AddTexture(PlatformMaterialCookRequest request, List<object> textures, string name, string idField, string pathField) {
        request.FieldValues.TryGetValue(idField, out string assetId); request.FieldValues.TryGetValue(pathField, out string path);
        if (string.IsNullOrWhiteSpace(assetId) && string.IsNullOrWhiteSpace(path)) { return; }
        if (string.IsNullOrWhiteSpace(assetId)) { throw new InvalidOperationException($"Material '{request.MaterialAssetId}' has a texture path without '{idField}'."); }
        if (string.IsNullOrWhiteSpace(path)) { path = "cooked/imported/" + assetId; }
        textures.Add(new { name, assetId, path = WebGpuMaterialArtifactSerializer.ContentPath(path) });
    }
}
