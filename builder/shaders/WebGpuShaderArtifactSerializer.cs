using System.Buffers.Binary;
using System.Text;
using System.Text.Json;

namespace helengine.webgpu.builder;

/// <summary>Reads and writes the checked WGSP v1 envelope shared with the native browser player.</summary>
public static class WebGpuShaderArtifactSerializer {
    /// <summary>Bounds complete shader artifacts before allocating section copies.</summary>
    public const int MaximumBytes = 64 * 1024 * 1024;
    /// <summary>Defines the exact fixed-width v1 prefix length.</summary>
    public const int HeaderBytes = 28;

    /// <summary>Publishes deterministic metadata and two validated UTF-8 shader sections.</summary>
    public static byte[] Serialize(WebGpuShaderArtifact artifact) {
        ArgumentNullException.ThrowIfNull(artifact);
        Validate(artifact);
        try {
            byte[] metadata = WebGpuArtifactJson.Serialize(artifact.Metadata);
            byte[] vertex = WebGpuArtifactJson.Utf8.GetBytes(artifact.VertexWgsl);
            byte[] fragment = WebGpuArtifactJson.Utf8.GetBytes(artifact.FragmentWgsl);
            long total = HeaderBytes + (long)metadata.Length + vertex.Length + fragment.Length;
            if (total > MaximumBytes) { throw new InvalidDataException("WGSP exceeds the 64 MiB limit."); }
            byte[] bytes = new byte[(int)total];
            "WGSP"u8.CopyTo(bytes);
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(4), 1);
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(8), HeaderBytes);
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(12), (uint)total);
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(16), (uint)metadata.Length);
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(20), (uint)vertex.Length);
            BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(24), (uint)fragment.Length);
            metadata.CopyTo(bytes, HeaderBytes); vertex.CopyTo(bytes, HeaderBytes + metadata.Length);
            fragment.CopyTo(bytes, HeaderBytes + metadata.Length + vertex.Length);
            return bytes;
        } catch (EncoderFallbackException error) { throw new InvalidDataException("WGSP contains invalid Unicode.", error); }
    }

    /// <summary>Verifies all section boundaries before decoding or trusting any metadata.</summary>
    public static WebGpuShaderArtifact Deserialize(byte[] bytes) {
        ArgumentNullException.ThrowIfNull(bytes);
        if (bytes.Length < HeaderBytes || bytes.Length > MaximumBytes || !bytes.AsSpan(0, 4).SequenceEqual("WGSP"u8)
            || BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(4)) != 1
            || BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(8)) != HeaderBytes
            || BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(12)) != bytes.Length) {
            throw new InvalidDataException("WGSP header/version/length is invalid.");
        }
        uint metadata = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(16));
        uint vertex = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(20));
        uint fragment = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(24));
        if (metadata == 0 || vertex == 0 || fragment == 0 || HeaderBytes + (ulong)metadata + vertex + fragment != (ulong)bytes.Length) {
            throw new InvalidDataException("WGSP section lengths do not match the complete artifact.");
        }
        try {
            WebGpuShaderArtifact artifact = new(WebGpuArtifactJson.Parse(bytes.AsSpan(HeaderBytes, (int)metadata)),
                WebGpuArtifactJson.Utf8.GetString(bytes, HeaderBytes + (int)metadata, (int)vertex),
                WebGpuArtifactJson.Utf8.GetString(bytes, HeaderBytes + (int)metadata + (int)vertex, (int)fragment));
            Validate(artifact);
            return artifact;
        } catch (DecoderFallbackException error) { throw new InvalidDataException("WGSP shader text contains invalid UTF-8.", error); }
    }

    /// <summary>Checks required identities, stage integrity and bounded structural metadata.</summary>
    static void Validate(WebGpuShaderArtifact artifact) {
        try {
            JsonElement metadata = artifact.Metadata;
            foreach (string name in new[] { "shaderAssetId", "vertexProgram", "pixelProgram", "variant", "vertexEntryPoint", "fragmentEntryPoint" }) { WebGpuArtifactJson.Text(metadata, name); }
            if (WebGpuArtifactJson.Text(metadata, "target") != "webgpu" || string.IsNullOrWhiteSpace(artifact.VertexWgsl) || string.IsNullOrWhiteSpace(artifact.FragmentWgsl)
                || WebGpuArtifactJson.Text(metadata, "vertexWgslSha256") != WebGpuShaderArtifact.Hash(artifact.VertexWgsl)
                || WebGpuArtifactJson.Text(metadata, "fragmentWgslSha256") != WebGpuShaderArtifact.Hash(artifact.FragmentWgsl)) {
                throw new InvalidDataException("WGSP shader identity or stage hash is inconsistent.");
            }
            JsonElement tools = WebGpuArtifactJson.Container(metadata, "toolchain", JsonValueKind.Object);
            WebGpuArtifactJson.Text(tools, "fingerprint"); WebGpuArtifactJson.Text(tools, "dawnRevision"); WebGpuArtifactJson.Text(tools, "adapterVersion");
            foreach (JsonElement dependency in WebGpuArtifactJson.Container(metadata, "dependencies", JsonValueKind.Array).EnumerateArray()) {
                WebGpuArtifactJson.Text(dependency, "path"); WebGpuArtifactJson.Text(dependency, "hash");
            }
            foreach (string name in new[] { "vertexInputs", "vertexOutputs", "fragmentInputs" }) {
                HashSet<int> locations = [];
                foreach (JsonElement item in WebGpuArtifactJson.Container(metadata, name, JsonValueKind.Array).EnumerateArray()) {
                    WebGpuArtifactJson.Text(item, "semantic"); WebGpuArtifactJson.Text(item, "type"); WebGpuArtifactJson.Text(item, "interpolation");
                    if (item.GetProperty("semanticIndex").GetInt32() < 0 || item.GetProperty("location").GetInt32() < 0 || !locations.Add(item.GetProperty("location").GetInt32())) {
                        throw new InvalidDataException("WGSP interface location is invalid or duplicated.");
                    }
                }
            }
            HashSet<int> slots = [];
            foreach (JsonElement binding in WebGpuArtifactJson.Container(metadata, "bindings", JsonValueKind.Array).EnumerateArray()) {
                WebGpuArtifactJson.Text(binding, "name");
                string resourceClass = WebGpuArtifactJson.Text(binding, "class");
                int slot = binding.GetProperty("binding").GetInt32();
                if (resourceClass != "constant-buffer" && resourceClass != "texture" && resourceClass != "sampler"
                    || binding.GetProperty("group").GetInt32() != 0 || binding.GetProperty("registerSpace").GetInt32() != 0
                    || binding.GetProperty("registerIndex").GetInt32() < 0 || slot < 0 || !slots.Add(slot)
                    || binding.GetProperty("size").GetInt32() < 0) { throw new InvalidDataException("WGSP resource binding is invalid."); }
                JsonElement stages = WebGpuArtifactJson.Container(binding, "stages", JsonValueKind.Array);
                if (stages.GetArrayLength() == 0 || stages.GetArrayLength() > 2
                    || stages.EnumerateArray().Any(value => value.GetString() != "vertex" && value.GetString() != "fragment")
                    || stages.EnumerateArray().Select(value => value.GetString()).Distinct().Count() != stages.GetArrayLength()) { throw new InvalidDataException("WGSP stage visibility is invalid."); }
                WebGpuArtifactJson.Container(binding, "layout", JsonValueKind.Object);
                WebGpuArtifactJson.Container(binding, "sourceLayout", JsonValueKind.Object);
            }
        } catch (Exception error) when (error is KeyNotFoundException || error is InvalidOperationException || error is FormatException || error is EncoderFallbackException) {
            throw new InvalidDataException("WGSP metadata does not match its v1 schema.", error);
        }
    }
}
