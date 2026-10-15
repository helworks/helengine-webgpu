using helengine.baseplatform.Results;
using System.Buffers.Binary;
using System.Text.Json;

namespace helengine.webgpu.builder;

/// <summary>Enforces the WGPM v1 byte envelope and validates material references before runtime use.</summary>
public static class WebGpuMaterialArtifactSerializer {
    /// <summary>Bounds the UTF-8 JSON payload independently of its sixteen-byte header.</summary>
    public const int MaximumJsonBytes = 16 * 1024 * 1024;
    /// <summary>Defines the exact WGPM prefix size.</summary>
    public const int HeaderBytes = 16;

    /// <summary>Serializes canonical metadata with explicit version and exact byte lengths.</summary>
    public static byte[] Serialize(WebGpuMaterialArtifact artifact) {
        ArgumentNullException.ThrowIfNull(artifact); Validate(artifact.Metadata);
        byte[] json = WebGpuArtifactJson.Serialize(artifact.Metadata);
        if (json.Length > MaximumJsonBytes) { throw new InvalidDataException("WGPM JSON exceeds 16 MiB."); }
        byte[] bytes = new byte[HeaderBytes + json.Length];
        "WGPM"u8.CopyTo(bytes);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(4), 1);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(8), (uint)bytes.Length);
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(12), (uint)json.Length);
        json.CopyTo(bytes, HeaderBytes); return bytes;
    }

    /// <summary>Verifies the entire envelope before parsing bounded strict UTF-8 metadata.</summary>
    public static WebGpuMaterialArtifact Deserialize(byte[] bytes) {
        ArgumentNullException.ThrowIfNull(bytes);
        if (bytes.Length <= HeaderBytes || bytes.Length > HeaderBytes + MaximumJsonBytes
            || !bytes.AsSpan(0, 4).SequenceEqual("WGPM"u8) || BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(4)) != 1
            || BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(8)) != bytes.Length
            || HeaderBytes + (ulong)BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(12)) != (ulong)bytes.Length) {
            throw new InvalidDataException("WGPM header/version/length is invalid.");
        }
        JsonElement metadata = WebGpuArtifactJson.Parse(bytes.AsSpan(HeaderBytes)); Validate(metadata);
        return new WebGpuMaterialArtifact(metadata);
    }

    /// <summary>Requires a content-relative canonical path without source filesystem traversal.</summary>
    internal static string ContentPath(string path) {
        if (string.IsNullOrWhiteSpace(path) || path.Contains('\\') || path.Contains(':') || path.StartsWith('/') || path.Contains('\0')
            || path.Split('/').Any(value => value.Length == 0 || value == "." || value == "..")) { throw new InvalidDataException("WGPM has an invalid content-relative path: " + path); }
        return path.StartsWith("cooked/", StringComparison.Ordinal) ? path : "cooked/" + path;
    }

    /// <summary>Checks named constants, selection identity and every referenced runtime path.</summary>
    static void Validate(JsonElement metadata) {
        try {
            WebGpuArtifactJson.Text(metadata, "materialAssetId");
            PlatformShaderDependency dependency = new(WebGpuArtifactJson.Text(metadata, "shaderAssetId"),
                WebGpuArtifactJson.Text(metadata, "vertexProgram"), WebGpuArtifactJson.Text(metadata, "pixelProgram"), WebGpuArtifactJson.Text(metadata, "variant"));
            if (WebGpuArtifactJson.Text(metadata, "shaderPath") != "cooked/" + WebGpuShaderArtifactIdentity.RelativePath(dependency)) { throw new InvalidDataException("WGPM shader path does not match its explicit selection."); }
            metadata.GetProperty("castsShadows").GetBoolean(); metadata.GetProperty("receivesShadows").GetBoolean();
            JsonElement state = WebGpuArtifactJson.Container(metadata, "renderState", JsonValueKind.Object);
            string alpha = WebGpuArtifactJson.Text(state, "alphaMode");
            if (alpha != "opaque" && alpha != "alpha-blend" && alpha != "additive") { throw new InvalidDataException("WGPM alpha mode is unsupported: " + alpha); }
            foreach (string name in new[] { "doubleSided", "useVertexColor", "lit", "depthTest", "depthWrite" }) { state.GetProperty(name).GetBoolean(); }
            HashSet<string> buffers = new(StringComparer.Ordinal);
            foreach (JsonElement buffer in WebGpuArtifactJson.Container(metadata, "constantBuffers", JsonValueKind.Array).EnumerateArray()) {
                string name = WebGpuArtifactJson.Text(buffer, "name"); string data = WebGpuArtifactJson.Text(buffer, "data", MaximumJsonBytes);
                byte[] bytes = Convert.FromBase64String(data);
                if (!buffers.Add(name) || bytes.Length == 0 || bytes.Length > 65536 || bytes.Length % 4 != 0 || Convert.ToBase64String(bytes) != data) { throw new InvalidDataException("WGPM constant buffer has invalid name/size/base64: " + name); }
            }
            HashSet<string> textures = new(StringComparer.Ordinal);
            foreach (JsonElement texture in WebGpuArtifactJson.Container(metadata, "textures", JsonValueKind.Array).EnumerateArray()) {
                if (!textures.Add(WebGpuArtifactJson.Text(texture, "name"))) { throw new InvalidDataException("WGPM texture name is duplicated."); }
                WebGpuArtifactJson.Text(texture, "assetId"); string path = WebGpuArtifactJson.Text(texture, "path");
                if (ContentPath(path) != path) { throw new InvalidDataException("WGPM texture path is not canonical."); }
            }
        } catch (Exception error) when (error is KeyNotFoundException || error is InvalidOperationException || error is FormatException || error is ArgumentException) {
            throw new InvalidDataException("WGPM metadata does not match its v1 schema.", error);
        }
    }
}
