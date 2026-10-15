using helengine;
using Silk.NET.SPIRV;
using Silk.NET.SPIRV.Cross;
using System.Runtime.InteropServices;
using System.Text.Json;

namespace helengine.webgpu.builder;

/// <summary>Reconciles original compiled HLSL identities with final Tint semantic reflection.</summary>
internal sealed unsafe class WebGpuSpirvReflection {
    /// <summary>Uses the artifact's camel-case member schema for original layout descriptions.</summary>
    static readonly JsonSerializerOptions JsonOptions = new() { PropertyNamingPolicy = JsonNamingPolicy.CamelCase };
    /// <summary>Borrows the reflection API during a single native context lifetime.</summary>
    readonly Cross Api;
    /// <summary>Borrows the native error/IR owner.</summary>
    readonly Context* Context;
    /// <summary>Borrows the selected original SPIR-V stage compiler.</summary>
    readonly Silk.NET.SPIRV.Cross.Compiler* Compiler;

    /// <summary>Initializes a context-scoped reflection reader.</summary>
    WebGpuSpirvReflection(Cross api, Context* context, Silk.NET.SPIRV.Cross.Compiler* compiler) {
        Api = api; Context = context; Compiler = compiler;
    }

    /// <summary>Copies source metadata while the SPIR-V context is alive and checks final slots/locations.</summary>
    public static WebGpuShaderStage Read(ShaderCompileRequest request, byte[] spirv, WebGpuTintResult translation,
        IReadOnlyList<WebGpuShaderDependency> dependencies, string fingerprint) {
        string library = Path.Combine(Path.GetDirectoryName(typeof(WebGpuSpirvReflection).Assembly.Location), "runtimes", "win-x64", "native", "spirv-cross.dll");
        if (!File.Exists(library)) { throw new FileNotFoundException("WebGPU SPIRV-Cross library is missing.", library); }
        using Cross api = new(Cross.CreateDefaultContext([library]));
        Context* context = null;
        Check(api, context, api.ContextCreate(&context));
        try {
            ParsedIr* parsed = null;
            fixed (byte* data = spirv) { Check(api, context, api.ContextParseSpirv(context, (uint*)data, (nuint)(spirv.Length / 4), &parsed)); }
            Silk.NET.SPIRV.Cross.Compiler* compiler = null;
            Check(api, context, api.ContextCreateCompiler(context, Backend.None, parsed, CaptureMode.TakeOwnership, &compiler));
            Check(api, context, api.CompilerSetEntryPoint(compiler, request.EntryPoint, request.Stage == ShaderStage.Vertex ? ExecutionModel.Vertex : ExecutionModel.Fragment));
            Resources* resources = null;
            Check(api, context, api.CompilerCreateShaderResources(compiler, &resources));
            WebGpuSpirvReflection reader = new(api, context, compiler);
            List<WebGpuShaderBinding> bindings = reader.ReadBindings(resources, translation.Reflection, request);
            return new WebGpuShaderStage(request, translation, bindings,
                reader.ReadInterface(resources, ResourceType.StageInput, translation.Reflection.GetProperty("inputs")),
                reader.ReadInterface(resources, ResourceType.StageOutput, translation.Reflection.GetProperty("outputs")), dependencies, fingerprint);
        } finally { if (context != null) { api.ContextDestroy(context); } }
    }

    /// <summary>Copies one native resource list before its owning context is released.</summary>
    ReflectedResource[] ResourcesOf(Resources* resources, ResourceType type) {
        ReflectedResource* values = null; nuint count = 0;
        Check(Api, Context, Api.ResourcesGetResourceListForType(resources, type, ref values, ref count));
        ReflectedResource[] result = new ReflectedResource[checked((int)count)];
        for (int index = 0; index < result.Length; index++) { result[index] = values[index]; }
        return result;
    }

    /// <summary>Matches active final bindings by slot and preserves original uniform member names and layout.</summary>
    List<WebGpuShaderBinding> ReadBindings(Resources* resources, JsonElement final, ShaderCompileRequest request) {
        Dictionary<int, JsonElement> originals = [];
        foreach (ResourceType resourceType in new[] { ResourceType.UniformBuffer, ResourceType.SeparateImage, ResourceType.SeparateSamplers }) {
            string resourceClass = resourceType == ResourceType.UniformBuffer ? "constant-buffer" : resourceType == ResourceType.SeparateImage ? "texture" : "sampler";
            int shift = resourceType == ResourceType.UniformBuffer ? request.Options.BindingPolicy.ConstantBufferShift
                : resourceType == ResourceType.SeparateImage ? request.Options.BindingPolicy.TextureShift : request.Options.BindingPolicy.SamplerShift;
            foreach (ReflectedResource resource in ResourcesOf(resources, resourceType)) {
                int group = checked((int)Api.CompilerGetDecoration(Compiler, resource.Id, Decoration.DescriptorSet));
                int slot = checked((int)Api.CompilerGetDecoration(Compiler, resource.Id, Decoration.Binding));
                if (group != 0) { throw new NotSupportedException("WebGPU supports register space zero only."); }
                string name = resourceType == ResourceType.UniformBuffer ? Api.CompilerGetNameS(Compiler, resource.BaseTypeId) : Api.CompilerGetNameS(Compiler, resource.Id);
                if (string.IsNullOrWhiteSpace(name)) { throw new InvalidOperationException("Compiled resource has no logical name."); }
                CrossType* type = Api.CompilerGetTypeHandle(Compiler, resource.TypeId);
                if (resourceType != ResourceType.UniformBuffer && Api.TypeGetNumArrayDimensions(type) != 0) { throw new NotSupportedException("WebGPU resource binding arrays are not supported by this profile."); }
                JsonElement layout = resourceType == ResourceType.UniformBuffer ? ReadStructure(resource.BaseTypeId, 0) : JsonSerializer.SerializeToElement(new { });
                JsonElement original = JsonSerializer.SerializeToElement(new { name, resourceClass, registerIndex = checked(slot - shift), layout });
                if (!originals.TryAdd(slot, original)) { throw new InvalidOperationException("WebGPU binding policy collision at group 0 binding " + slot); }
            }
        }
        foreach (ResourceType type in new[] { ResourceType.StorageBuffer, ResourceType.StorageImage, ResourceType.SampledImage }) {
            if (ResourcesOf(resources, type).Length != 0) { throw new NotSupportedException("WebGPU shader requires unsupported storage/combined resources: " + type); }
        }
        List<WebGpuShaderBinding> bindings = [];
        foreach (JsonElement binding in final.GetProperty("bindings").EnumerateArray()) {
            int slot = binding.GetProperty("binding").GetInt32();
            if (!originals.TryGetValue(slot, out JsonElement original)) { throw new InvalidOperationException("Tint introduced an unrecognized resource binding: " + slot); }
            string resourceClass = binding.GetProperty("class").GetString();
            string sourceClass = original.GetProperty("resourceClass").GetString();
            if (resourceClass != sourceClass && !(resourceClass == "depth-texture" && sourceClass == "texture")) { throw new InvalidOperationException("Tint changed the resource class at binding " + slot); }
            JsonElement layout = binding.TryGetProperty("layout", out JsonElement value) ? value.Clone() : JsonSerializer.SerializeToElement(new { });
            bindings.Add(new WebGpuShaderBinding(original.GetProperty("name").GetString(), resourceClass,
                original.GetProperty("registerIndex").GetInt32(), binding.GetProperty("group").GetInt32(), slot,
                binding.GetProperty("size").GetInt32(), layout, original.GetProperty("layout").Clone(),
                binding.GetProperty("textureDimension").GetInt32(), binding.GetProperty("sampleType").GetInt32(),
                binding.GetProperty("samplerType").GetInt32(), [request.Stage]));
        }
        return bindings.OrderBy(value => value.Group).ThenBy(value => value.Binding).ToList();
    }

    /// <summary>Reflects original aggregate bytes, preserving members, array strides and matrix orientation.</summary>
    JsonElement ReadStructure(uint typeId, int depth) {
        if (depth > 32) { throw new InvalidOperationException("Shader structure nesting exceeds 32."); }
        CrossType* type = Api.CompilerGetTypeHandle(Compiler, typeId);
        nuint size = 0;
        Check(Api, Context, Api.CompilerGetDeclaredStructSize(Compiler, type, &size));
        List<WebGpuShaderBufferMember> members = [];
        for (uint index = 0; index < Api.TypeGetNumMemberTypes(type); index++) {
            uint offset = 0; nuint memberSize = 0; uint arrayStride = 0; uint matrixStride = 0;
            Check(Api, Context, Api.CompilerTypeStructMemberOffset(Compiler, type, index, &offset));
            Check(Api, Context, Api.CompilerGetDeclaredStructMemberSize(Compiler, type, index, &memberSize));
            uint memberTypeId = Api.TypeGetMemberType(type, index);
            CrossType* memberType = Api.CompilerGetTypeHandle(Compiler, memberTypeId);
            uint dimensionCount = Api.TypeGetNumArrayDimensions(memberType);
            List<uint> dimensions = [];
            for (uint dimension = 0; dimension < dimensionCount; dimension++) {
                if (Api.TypeArrayDimensionIsLiteral(memberType, dimension) == 0) { throw new NotSupportedException("Runtime constant-buffer arrays are unsupported."); }
                dimensions.Add(Api.TypeGetArrayDimension(memberType, dimension));
            }
            if (dimensionCount != 0) { Check(Api, Context, Api.CompilerTypeStructMemberArrayStride(Compiler, type, index, &arrayStride)); }
            uint columns = Api.TypeGetColumns(memberType); uint rows = Api.TypeGetVectorSize(memberType);
            if (columns > 1) { Check(Api, Context, Api.CompilerTypeStructMemberMatrixStride(Compiler, type, index, &matrixStride)); }
            string orientation = columns > 1 ? Api.CompilerHasMemberDecoration(Compiler, typeId, index, Decoration.RowMajor) != 0 ? "row-major" : "column-major" : "";
            JsonElement aggregate = Api.TypeGetBasetype(memberType).ToString() == "Struct" ? ReadStructure(memberTypeId, depth + 1) : JsonSerializer.SerializeToElement(new { });
            string name = Api.CompilerGetMemberNameS(Compiler, typeId, index);
            if (string.IsNullOrWhiteSpace(name)) { throw new InvalidOperationException("Shader constant member has no original name."); }
            JsonElement description = JsonSerializer.SerializeToElement(new { dimensions, rows, columns, structure = aggregate });
            members.Add(new WebGpuShaderBufferMember(name, TypeName(memberType), checked((int)offset), checked((int)memberSize),
                checked((int)arrayStride), checked((int)matrixStride), orientation, description));
        }
        return JsonSerializer.SerializeToElement(new { size = checked((int)size), members }, JsonOptions);
    }

    /// <summary>Matches final stage locations to original HLSL semantics without parsing source text.</summary>
    List<WebGpuShaderInterfaceElement> ReadInterface(Resources* resources, ResourceType kind, JsonElement final) {
        Dictionary<int, ReflectedResource> originals = [];
        foreach (ReflectedResource value in ResourcesOf(resources, kind)) {
            if (Api.CompilerHasDecoration(Compiler, value.Id, Decoration.BuiltIn) != 0) { continue; }
            int location = checked((int)Api.CompilerGetDecoration(Compiler, value.Id, Decoration.Location));
            if (!originals.TryAdd(location, value)) { throw new InvalidOperationException("Duplicate shader interface location " + location); }
        }
        List<WebGpuShaderInterfaceElement> result = [];
        foreach (JsonElement value in final.EnumerateArray()) {
            int location = value.GetProperty("location").GetInt32();
            if (!originals.TryGetValue(location, out ReflectedResource original)) { throw new InvalidOperationException("Final shader location has no original semantic: " + location); }
            string semantic = Api.CompilerGetDecorationStringS(Compiler, original.Id, (Decoration)5635);
            if (string.IsNullOrWhiteSpace(semantic)) { throw new InvalidOperationException("Compiled interface lacks its HLSL semantic at location " + location); }
            int suffix = semantic.Length;
            while (suffix > 0 && char.IsAsciiDigit(semantic[suffix - 1])) { suffix--; }
            int semanticIndex = suffix == semantic.Length ? 0 : int.Parse(semantic[suffix..], System.Globalization.CultureInfo.InvariantCulture);
            CrossType* type = Api.CompilerGetTypeHandle(Compiler, original.TypeId);
            if (Api.TypeGetNumArrayDimensions(type) != 0 || Api.TypeGetColumns(type) > 1) { throw new NotSupportedException("Aggregate stage interface locations require explicit lowering support."); }
            result.Add(new WebGpuShaderInterfaceElement(Api.CompilerGetNameS(Compiler, original.Id), semantic[..suffix].ToUpperInvariant(),
                semanticIndex, location, TypeName(type), value.GetProperty("interpolationType").GetRawText() + ":" + value.GetProperty("interpolationSampling").GetRawText()));
        }
        return result.OrderBy(value => value.Location).ToList();
    }

    /// <summary>Describes compiled scalar/vector/matrix shape without inventing an authored semantic.</summary>
    string TypeName(CrossType* type) {
        return Api.TypeGetBasetype(type) + ":" + Api.TypeGetBitWidth(type) + ":" + Api.TypeGetVectorSize(type) + ":" + Api.TypeGetColumns(type);
    }

    /// <summary>Propagates the native reflection failure at the exact API boundary.</summary>
    static void Check(Cross api, Context* context, Result result) {
        if (result != Result.Success) { throw new InvalidOperationException("WebGPU SPIR-V reflection failed: " + (context == null ? result.ToString() : api.ContextGetLastErrorStringS(context))); }
    }
}
