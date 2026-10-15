using helengine;
using helengine.webgpu.builder;

namespace helengine.webgpu.builder.tests;

/// <summary>Checks cross-stage interface and resource compatibility before publication.</summary>
public sealed class WebGpuShaderPairTests {
    /// <summary>Location type mismatches cannot be linked into a shader pair.</summary>
    [Fact]
    public void Link_rejects_nonmatching_varyings() {
        WebGpuShaderStage vertex = WebGpuShaderBackendTests.Compile("struct O { float4 p:SV_POSITION; float2 uv:TEXCOORD0; }; O VS(float3 p:POSITION) { O o; o.p=float4(p,1); o.uv=p.xy; return o; }", ShaderStage.Vertex);
        WebGpuShaderStage fragment = WebGpuShaderBackendTests.Compile("float4 PS(float3 uv:TEXCOORD0):SV_TARGET { return float4(uv,1); }", ShaderStage.Pixel);
        Assert.Throws<InvalidOperationException>(() => WebGpuShaderProgramPair.Link(vertex, fragment));
    }

    /// <summary>Identical constant bindings shared across stages retain both stage visibilities.</summary>
    [Fact]
    public void Link_merges_matching_shared_binding() {
        string source = "cbuffer Shared:register(b0) { float4 offset; }; float4 VS(float3 p:POSITION):SV_POSITION { return float4(p,1)+offset; } float4 PS():SV_TARGET { return offset; }";
        WebGpuShaderProgramPair pair = WebGpuShaderProgramPair.Link(WebGpuShaderBackendTests.Compile(source, ShaderStage.Vertex), WebGpuShaderBackendTests.Compile(source, ShaderStage.Pixel));
        Assert.Equal(2, Assert.Single(pair.Bindings).Stages.Count);
    }

    /// <summary>Resource classes cannot alias a slot across independently compiled stages.</summary>
    [Fact]
    public void Link_rejects_conflicting_resource_classes() {
        WebGpuShaderStage vertex = WebGpuShaderBackendTests.Compile("cbuffer Shared:register(b0) { float4 offset; }; float4 VS(float3 p:POSITION):SV_POSITION { return float4(p,1)+offset; }", ShaderStage.Vertex);
        WebGpuShaderStage fragment = WebGpuShaderBackendTests.Compile("Texture2D tex:register(t0); float4 PS():SV_TARGET { return tex.Load(int3(0,0,0)); }", ShaderStage.Pixel,
            new ShaderBindingPolicy(0, 0, 0, 200, 300));
        Assert.Throws<InvalidOperationException>(() => WebGpuShaderProgramPair.Link(vertex, fragment));
    }

    /// <summary>Malformed interface metadata is rejected even when it did not originate from the compiler.</summary>
    [Fact]
    public void Link_rejects_duplicate_locations() {
        WebGpuShaderStage vertex = WebGpuShaderBackendTests.Compile("struct O { float4 p:SV_POSITION; float2 uv:TEXCOORD0; }; O VS(float3 p:POSITION) { O o; o.p=float4(p,1); o.uv=p.xy; return o; }", ShaderStage.Vertex);
        WebGpuShaderStage fragment = WebGpuShaderBackendTests.Compile("float4 PS(float2 uv:TEXCOORD0):SV_TARGET { return float4(uv,0,1); }", ShaderStage.Pixel);
        WebGpuShaderStage malformed = new(vertex.Request, vertex.Translation, vertex.Bindings, vertex.Inputs,
            vertex.Outputs.Concat(vertex.Outputs).ToArray(), vertex.Dependencies, vertex.ToolchainFingerprint);
        Assert.Throws<InvalidOperationException>(() => WebGpuShaderProgramPair.Link(malformed, fragment));
    }
}
