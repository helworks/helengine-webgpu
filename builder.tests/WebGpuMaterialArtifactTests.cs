using helengine.webgpu.builder;
using System.Buffers.Binary;
using System.Text;

namespace helengine.webgpu.builder.tests;

/// <summary>Checks WGPM v1 byte boundaries and exact preservation of named constant buffers.</summary>
public sealed class WebGpuMaterialArtifactTests {
    /// <summary>The approved prefix is exact and serialization is deterministic.</summary>
    [Fact]
    public void Material_round_trips_exact_header_and_named_buffers() {
        byte[] bytes = Fixture(); Assert.Equal("WGPM", Encoding.ASCII.GetString(bytes, 0, 4));
        Assert.Equal(1u, BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(4)));
        Assert.Equal((uint)bytes.Length, BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(8)));
        Assert.Equal((uint)bytes.Length - 16, BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(12)));
        Assert.Equal(bytes, WebGpuMaterialArtifactSerializer.Serialize(WebGpuMaterialArtifactSerializer.Deserialize(bytes)));
    }
    /// <summary>Malformed lengths, UTF-8, versions and trailing bytes never reach runtime material creation.</summary>
    [Fact]
    public void Material_rejects_invalid_envelope_and_utf8() {
        byte[] bytes = Fixture();
        Assert.Throws<InvalidDataException>(() => WebGpuMaterialArtifactSerializer.Deserialize(bytes[..^1]));
        Assert.Throws<InvalidDataException>(() => WebGpuMaterialArtifactSerializer.Deserialize(bytes.Concat(new byte[1]).ToArray()));
        byte[] changed = (byte[])bytes.Clone(); changed[4] = 2;
        Assert.Throws<InvalidDataException>(() => WebGpuMaterialArtifactSerializer.Deserialize(changed));
        changed = (byte[])bytes.Clone(); BinaryPrimitives.WriteUInt32LittleEndian(changed.AsSpan(12), uint.MaxValue);
        Assert.Throws<InvalidDataException>(() => WebGpuMaterialArtifactSerializer.Deserialize(changed));
        changed = (byte[])bytes.Clone(); changed[16] = 255;
        Assert.Throws<InvalidDataException>(() => WebGpuMaterialArtifactSerializer.Deserialize(changed));
        Assert.Throws<InvalidDataException>(() => WebGpuMaterialArtifactSerializer.Deserialize(new byte[16 * 1024 * 1024 + 17]));
    }
    /// <summary>Raw caller-provided constants stay byte-exact and malformed base64 fails cooking.</summary>
    [Fact]
    public void Material_preserves_raw_constants_and_rejects_malformed_base64() {
        var fields = WebGpuMaterialCookerTests.Fields(); fields["constant-buffers"] = "[{\"name\":\"CustomBuffer\",\"data\":\"AQIDBA==\"}]";
        var material = WebGpuMaterialArtifactSerializer.Deserialize(new WebGpuMaterialCooker().Cook(WebGpuMaterialCookerTests.Request(fields)).CookedMaterialBytes);
        Assert.Equal("AQIDBA==", material.Metadata.GetProperty("constantBuffers").EnumerateArray().Single(value => value.GetProperty("name").GetString() == "CustomBuffer").GetProperty("data").GetString());
        fields["constant-buffers"] = "[{\"name\":\"CustomBuffer\",\"data\":\"!!\"}]";
        Assert.Throws<InvalidOperationException>(() => new WebGpuMaterialCooker().Cook(WebGpuMaterialCookerTests.Request(fields)));
    }
    /// <summary>Named uniform data may reach the documented 64 KiB limit rather than the shorter identifier limit.</summary>
    [Fact]
    public void Material_accepts_a_complete_64_kib_constant_buffer() {
        var fields = WebGpuMaterialCookerTests.Fields();
        string data = Convert.ToBase64String(new byte[65536]);
        fields["constant-buffers"] = "[{\"name\":\"CustomBuffer\",\"data\":\"" + data + "\"}]";
        var artifact = WebGpuMaterialArtifactSerializer.Deserialize(new WebGpuMaterialCooker().Cook(WebGpuMaterialCookerTests.Request(fields)).CookedMaterialBytes);
        Assert.Equal(data, artifact.Metadata.GetProperty("constantBuffers").EnumerateArray().Single(value => value.GetProperty("name").GetString() == "CustomBuffer").GetProperty("data").GetString());
    }
    /// <summary>Builds a real cooked material rather than hand-authoring the expected envelope.</summary>
    static byte[] Fixture() => new WebGpuMaterialCooker().Cook(WebGpuMaterialCookerTests.Request(WebGpuMaterialCookerTests.Fields())).CookedMaterialBytes;
}
