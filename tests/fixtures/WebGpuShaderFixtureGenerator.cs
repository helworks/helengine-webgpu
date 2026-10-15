using helengine.baseplatform.Requests;
using helengine.baseplatform.Results;
using helengine.webgpu.builder;
using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

namespace helengine.webgpu.fixtures;

/// <summary>Generates reproducible native/browser fixtures through the production cookers and shared compiler.</summary>
public static class WebGpuShaderFixtureGenerator {
    /// <summary>Writes original built-ins plus one explicit custom matrix/array/sampling selection to the requested root.</summary>
    public static void Main(string[] arguments) {
        if (arguments.Length != 1 || !Path.IsPathFullyQualified(arguments[0])) { throw new ArgumentException("An absolute fixture output root is required."); }
        string root = Path.GetFullPath(arguments[0]); Directory.CreateDirectory(root);
        List<PlatformShaderDependency> dependencies = [];
        List<PlatformShaderArtifactCookSource> sources = [];
        foreach (string asset in new[] { "ForwardStandardShader", "SpriteBatchShader" }) {
            string path = Path.Combine(FindRepository(), "..", "helengine", "engine", "helengine.editor", "shaders", "builtin", asset + ".hlsl");
            string text = File.ReadAllText(path);
            sources.Add(Source(asset, path, text));
            foreach (string variant in asset == "SpriteBatchShader" ? new[] { "Textured", "RoundedShape" } : new[] { "default", "ForwardStandard", "ForwardStandardShadowed", "ShadowDepth" }) {
                dependencies.Add(new PlatformShaderDependency(asset, asset + ".vs", asset + ".ps", variant));
            }
        }
        string customPath = Path.Combine(root, "matrix_fixture.hlsl");
        string custom = """
            cbuffer FixtureBuffer : register(b0) { row_major float4x4 cameraMatrix; float4 tints[2]; float weights[2]; float2 offsets[2]; };
            Texture2D FixtureTexture : register(t0); SamplerState FixtureSampler : register(s0);
            struct O { float4 p:SV_POSITION; float2 uv:TEXCOORD0; };
            O VS(float3 p:POSITION) { O o; o.p=mul(float4(p,1),cameraMatrix); o.uv=float2(0.5,0.5); return o; }
            float4 PS(O o):SV_TARGET { return FixtureTexture.Sample(FixtureSampler,o.uv)*tints[1]*weights[1]*float4(offsets[1],1,1); }
            """;
        File.WriteAllText(customPath, custom);
        dependencies.Add(new PlatformShaderDependency("matrix_fixture", "matrix_fixture.vs", "matrix_fixture.ps", "Mesh"));
        sources.Add(Source("matrix_fixture", customPath, custom));
        foreach (string fixture in new[] { "shadow_probe", "shadow_depth" }) {
            string path = Path.Combine(root, fixture + ".hlsl");
            string source = fixture == "shadow_depth"
                ? "float4 VS(float3 p:POSITION):SV_POSITION{return float4(p,1);} float PS():SV_DEPTH{return 0.25;}"
                : """
                  Texture2D<float> DepthAtlas:register(t1); TextureCube<float> DepthCube:register(t2);
                  SamplerComparisonState CompareAtlas:register(s1); SamplerComparisonState CompareCube:register(s2);
                  float4 VS(float3 p:POSITION):SV_POSITION{return float4(p,1);}
                  float4 PS(float4 p:SV_POSITION):SV_TARGET {
                      int stripe=min((int)(p.x*18.0/640.0),17); int face=stripe/3; int sampleIndex=stripe%3;
                      float reference=sampleIndex==0?0.1:sampleIndex==1?0.5:0.9;
                      float3 direction=face==0?float3(1,0,0):face==1?float3(-1,0,0):face==2?float3(0,1,0):face==3?float3(0,-1,0):face==4?float3(0,0,1):float3(0,0,-1);
                      return float4(DepthAtlas.SampleCmpLevelZero(CompareAtlas,float2(0.5,0.5),reference),DepthCube.SampleCmpLevelZero(CompareCube,direction,reference),0,1);
                  }
                  """;
            File.WriteAllText(path, source); sources.Add(Source(fixture, path, source));
            dependencies.Add(new PlatformShaderDependency(fixture, fixture + ".vs", fixture + ".ps", "default"));
        }
        new WebGpuShaderArtifactCooker().Cook(new PlatformShaderArtifactCookRequest(Path.Combine(root, "shaders-content"), "webgpu", "release", "webgpu", dependencies, sources));
        var fields = WebGpuPlatformDefinitionFactory.Create().MaterialSchemas.Single().Fields.ToDictionary(field => field.FieldId, field => field.DefaultValue);
        fields["shader-asset-id"] = "matrix_fixture"; fields["vertex-program"] = "matrix_fixture.vs"; fields["pixel-program"] = "matrix_fixture.ps"; fields["use-custom-shader"] = "true";
        fields["variant"] = "Mesh";
        fields["lighting-mode"] = "unlit"; fields["double-sided"] = "true"; fields["depth-test"] = "false"; fields["depth-write"] = "false";
        var compiled = WebGpuShaderArtifactSerializer.Deserialize(File.ReadAllBytes(Path.Combine(root, "shaders-content", WebGpuShaderArtifactIdentity.RelativePath(dependencies.Single(value => value.ShaderAssetId == "matrix_fixture")))));
        var layout = compiled.Metadata.GetProperty("bindings")[0].GetProperty("sourceLayout");
        byte[] data = new byte[layout.GetProperty("size").GetInt32()];
        for (int index = 0; index < 8; ++index) { BinaryPrimitives.WriteSingleLittleEndian(data.AsSpan(64 + index * 4), index < 4 ? 0.9f : index == 7 ? 1 : 0.5f); }
        WriteArray(data, layout, "weights", [0.25f, 1], 1);
        WriteArray(data, layout, "offsets", [0.1f, 0.2f, 1, 1], 2);
        fields["constant-buffers"] = JsonSerializer.Serialize(new[] { new { name = "FixtureBuffer", data = Convert.ToBase64String(data) } });
        File.WriteAllBytes(Path.Combine(root, "shaders-content", "matrix_fixture.wgpm"), new WebGpuMaterialCooker().Cook(new PlatformMaterialCookRequest("matrix_fixture", "matrix_fixture.wgpm", "webgpu", "release", "webgpu", "standard-shader", fields)).CookedMaterialBytes);
        Console.WriteLine("Generated 7 original/custom shader selections and matrix/array WGPM fixture in " + root);
    }
    /// <summary>Writes logical array elements using original compiler offsets and strides instead of assuming WGSL packing.</summary>
    static void WriteArray(byte[] data, JsonElement layout, string name, float[] values, int components) {
        JsonElement member = layout.GetProperty("members").EnumerateArray().Single(value => value.GetProperty("name").GetString() == name);
        int offset = member.GetProperty("offset").GetInt32(); int stride = member.GetProperty("arrayStride").GetInt32();
        for (int index = 0; index < values.Length; ++index) { BinaryPrimitives.WriteSingleLittleEndian(data.AsSpan(offset + index / components * stride + index % components * 4), values[index]); }
    }
    /// <summary>Locates the maintained backend from the invocation directory without relying on a compiled binary's output folder.</summary>
    static string FindRepository() {
        DirectoryInfo directory = new(Directory.GetCurrentDirectory());
        while (directory != null) { if (File.Exists(Path.Combine(directory.FullName, "builder", "WebGpuPlatformDefinitionFactory.cs"))) { return directory.FullName; } directory = directory.Parent; }
        throw new DirectoryNotFoundException("Run the fixture generator from the backend repository.");
    }
    /// <summary>Uses the exact authored source snapshot and its hash at the shared cooker boundary.</summary>
    static PlatformShaderArtifactCookSource Source(string asset, string path, string text) => new(asset, Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(text))), text, Path.GetFullPath(path));
}
