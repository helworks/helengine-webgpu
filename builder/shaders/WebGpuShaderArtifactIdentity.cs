using helengine.baseplatform.Results;
using System.Buffers.Binary;
using System.Security.Cryptography;

namespace helengine.webgpu.builder;

/// <summary>Derives stable opaque artifact filenames from explicit shared shader selections.</summary>
public static class WebGpuShaderArtifactIdentity {
    /// <summary>Hashes length-prefixed UTF-8 fields so selection boundaries cannot alias each other.</summary>
    public static string RelativePath(PlatformShaderDependency dependency) {
        ArgumentNullException.ThrowIfNull(dependency);
        if (!dependency.HasProgramPair) { throw new InvalidOperationException("WebGPU shaders require an explicit program pair and variant."); }
        using MemoryStream bytes = new();
        Span<byte> length = stackalloc byte[4];
        foreach (string value in new[] { dependency.ShaderAssetId, dependency.VertexProgramName, dependency.PixelProgramName, dependency.VariantName }) {
            if (string.IsNullOrWhiteSpace(value) || value.Length > 4096 || value.Contains('\0')) { throw new InvalidOperationException("WebGPU shader selection contains an invalid identity."); }
            byte[] encoded = WebGpuArtifactJson.Utf8.GetBytes(value);
            BinaryPrimitives.WriteUInt32LittleEndian(length, (uint)encoded.Length);
            bytes.Write(length); bytes.Write(encoded);
        }
        return "shaders/" + Convert.ToHexString(SHA256.HashData(bytes.ToArray())).ToLowerInvariant() + ".wgsp";
    }
}
