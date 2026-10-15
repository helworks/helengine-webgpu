using System.Text.Json;

namespace helengine.webgpu.builder;

/// <summary>Preserves original constant-member byte layout for reconciliation with WGSL.</summary>
public sealed class WebGpuShaderBufferMember {
    /// <summary>Initializes a compiled original member layout including nested aggregate metadata.</summary>
    public WebGpuShaderBufferMember(string name, string type, int offset, int size, int arrayStride, int matrixStride, string matrixOrientation, JsonElement aggregate) {
        Name = name; Type = type; Offset = offset; Size = size; ArrayStride = arrayStride;
        MatrixStride = matrixStride; MatrixOrientation = matrixOrientation; Aggregate = aggregate;
    }
    /// <summary>Gets the original member name.</summary>
    public string Name { get; }
    /// <summary>Gets the original scalar, vector, matrix or aggregate type.</summary>
    public string Type { get; }
    /// <summary>Gets the original byte offset within its enclosing struct.</summary>
    public int Offset { get; }
    /// <summary>Gets the original occupied byte size.</summary>
    public int Size { get; }
    /// <summary>Gets the original array stride, or zero for non-arrays.</summary>
    public int ArrayStride { get; }
    /// <summary>Gets the original matrix stride, or zero for non-matrices.</summary>
    public int MatrixStride { get; }
    /// <summary>Gets the original matrix orientation, or an empty string for non-matrices.</summary>
    public string MatrixOrientation { get; }
    /// <summary>Gets the recursive original type description for arrays and structures.</summary>
    public JsonElement Aggregate { get; }
}
