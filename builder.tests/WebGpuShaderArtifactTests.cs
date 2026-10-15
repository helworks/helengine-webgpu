using helengine;
using helengine.baseplatform.Results;
using helengine.webgpu.builder;
using System.Buffers.Binary;
using System.Text;

namespace helengine.webgpu.builder.tests;

/// <summary>Verifies deterministic WGSP bytes and rejects malformed or inconsistent envelope metadata.</summary>
public sealed class WebGpuShaderArtifactTests {
    /// <summary>Header offsets, UTF-8 sections and deterministic serialization form the native reader contract.</summary>
    [Fact]
    public void Serialize_round_trips_the_exact_versioned_header() {
        WebGpuShaderArtifact artifact = Fixture();
        byte[] bytes = WebGpuShaderArtifactSerializer.Serialize(artifact);
        Assert.Equal("WGSP", Encoding.ASCII.GetString(bytes, 0, 4));
        Assert.Equal(1u, BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(4)));
        Assert.Equal(28u, BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(8)));
        Assert.Equal((uint)bytes.Length, BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(12)));
        Assert.Equal(bytes, WebGpuShaderArtifactSerializer.Serialize(artifact));
        WebGpuShaderArtifact decoded = WebGpuShaderArtifactSerializer.Deserialize(bytes);
        Assert.Equal("fixture", decoded.Metadata.GetProperty("shaderAssetId").GetString());
        Assert.Equal(artifact.VertexWgsl, decoded.VertexWgsl);
        Assert.Equal(artifact.FragmentWgsl, decoded.FragmentWgsl);
        Assert.Equal(bytes, WebGpuShaderArtifactSerializer.Serialize(decoded));
    }

    /// <summary>Corrupt envelopes cannot reach metadata interpretation or GPU initialization.</summary>
    [Theory]
    [InlineData(4, 2u)]
    [InlineData(8, 24u)]
    [InlineData(12, uint.MaxValue)]
    [InlineData(16, uint.MaxValue)]
    [InlineData(20, uint.MaxValue)]
    [InlineData(24, uint.MaxValue)]
    public void Deserialize_rejects_bad_version_lengths_and_overflow(int offset, uint value) {
        byte[] bytes = WebGpuShaderArtifactSerializer.Serialize(Fixture());
        BinaryPrimitives.WriteUInt32LittleEndian(bytes.AsSpan(offset), value);
        Assert.Throws<InvalidDataException>(() => WebGpuShaderArtifactSerializer.Deserialize(bytes));
    }

    /// <summary>Every section uses strict UTF-8 and exact byte boundaries, without trailing data.</summary>
    [Fact]
    public void Deserialize_rejects_truncation_trailing_bytes_and_invalid_utf8() {
        byte[] bytes = WebGpuShaderArtifactSerializer.Serialize(Fixture());
        Assert.Throws<InvalidDataException>(() => WebGpuShaderArtifactSerializer.Deserialize(bytes[..^1]));
        Assert.Throws<InvalidDataException>(() => WebGpuShaderArtifactSerializer.Deserialize(bytes.Concat(new byte[1]).ToArray()));
        bytes[28] = 255;
        Assert.Throws<InvalidDataException>(() => WebGpuShaderArtifactSerializer.Deserialize(bytes));
        Assert.Throws<InvalidDataException>(() => WebGpuShaderArtifactSerializer.Deserialize(new byte[64 * 1024 * 1024 + 1]));
    }

    /// <summary>Duplicate properties, missing identities and changed shader sections invalidate the artifact.</summary>
    [Fact]
    public void Deserialize_rejects_duplicate_missing_and_inconsistent_metadata() {
        byte[] bytes = WebGpuShaderArtifactSerializer.Serialize(Fixture());
        string metadata = Encoding.UTF8.GetString(bytes, 28, (int)BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(16)));
        byte[] duplicate = Repack(bytes, metadata.Replace("\"target\":\"webgpu\"", "\"target\":\"webgpu\",\"target\":\"webgpu\"", StringComparison.Ordinal));
        Assert.Throws<InvalidDataException>(() => WebGpuShaderArtifactSerializer.Deserialize(duplicate));
        Assert.Throws<InvalidDataException>(() => WebGpuShaderArtifactSerializer.Deserialize(Repack(bytes, "{}")));
        bytes[^1] ^= 1;
        Assert.Throws<InvalidDataException>(() => WebGpuShaderArtifactSerializer.Deserialize(bytes));
    }

    /// <summary>Stable logical paths use collision-resistant framed selections rather than authored filenames.</summary>
    [Fact]
    public void Identity_is_stable_and_distinguishes_selection_boundaries() {
        string first = WebGpuShaderArtifactIdentity.RelativePath(new PlatformShaderDependency("a", "bc", "d", "e"));
        Assert.Equal(first, WebGpuShaderArtifactIdentity.RelativePath(new PlatformShaderDependency("a", "bc", "d", "e")));
        Assert.NotEqual(first, WebGpuShaderArtifactIdentity.RelativePath(new PlatformShaderDependency("ab", "c", "d", "e")));
        Assert.Matches("^shaders/[a-f0-9]{64}\\.wgsp$", first);
    }

    /// <summary>Creates real compiled stage data without the pending ForwardStandard shadow source decision.</summary>
    internal static WebGpuShaderArtifact Fixture() {
        string source = "float4 VS(float3 p:POSITION):SV_POSITION { return float4(p,1); } float4 PS():SV_TARGET { return float4(1,0,0,1); }";
        WebGpuShaderProgramPair pair = WebGpuShaderProgramPair.Link(WebGpuShaderBackendTests.Compile(source, ShaderStage.Vertex),
            WebGpuShaderBackendTests.Compile(source, ShaderStage.Pixel));
        return WebGpuShaderArtifact.FromPair("fixture", pair);
    }

    /// <summary>Reframes a test metadata section while retaining complete shader sections.</summary>
    static byte[] Repack(byte[] original, string metadata) {
        int oldLength = (int)BinaryPrimitives.ReadUInt32LittleEndian(original.AsSpan(16));
        byte[] replacement = Encoding.UTF8.GetBytes(metadata);
        byte[] result = new byte[original.Length - oldLength + replacement.Length];
        original.AsSpan(0, 28).CopyTo(result);
        replacement.CopyTo(result, 28);
        original.AsSpan(28 + oldLength).CopyTo(result.AsSpan(28 + replacement.Length));
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(12), (uint)result.Length);
        BinaryPrimitives.WriteUInt32LittleEndian(result.AsSpan(16), (uint)replacement.Length);
        return result;
    }
}
