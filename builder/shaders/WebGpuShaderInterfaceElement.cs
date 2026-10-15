namespace helengine.webgpu.builder;

/// <summary>Connects original HLSL semantics to a final compiled stage location.</summary>
public sealed class WebGpuShaderInterfaceElement {
    /// <summary>Initializes one original semantic and its verified final shape.</summary>
    public WebGpuShaderInterfaceElement(string name, string semantic, int semanticIndex, int location, string type, string interpolation) {
        Name = name; Semantic = semantic; SemanticIndex = semanticIndex; Location = location; Type = type; Interpolation = interpolation;
    }
    /// <summary>Gets the original compiler variable name.</summary>
    public string Name { get; }
    /// <summary>Gets the HLSL semantic without its numeric index.</summary>
    public string Semantic { get; }
    /// <summary>Gets the numeric semantic suffix.</summary>
    public int SemanticIndex { get; }
    /// <summary>Gets the final WGSL location.</summary>
    public int Location { get; }
    /// <summary>Gets the reflected scalar/vector format.</summary>
    public string Type { get; }
    /// <summary>Gets the final interpolation type and sampling mode.</summary>
    public string Interpolation { get; }
}
