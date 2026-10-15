using helengine;
using System.Text.Json;

namespace helengine.webgpu.builder;

/// <summary>Links compiled stages and checks interface and binding compatibility before artifact publication.</summary>
public sealed class WebGpuShaderProgramPair {
    /// <summary>Initializes a validated pair with deterministic merged resource visibility.</summary>
    WebGpuShaderProgramPair(WebGpuShaderStage vertex, WebGpuShaderStage fragment, IReadOnlyList<WebGpuShaderBinding> bindings) {
        Vertex = vertex; Fragment = fragment; Bindings = bindings;
    }
    /// <summary>Gets the compiled vertex stage.</summary>
    public WebGpuShaderStage Vertex { get; }
    /// <summary>Gets the compiled fragment stage.</summary>
    public WebGpuShaderStage Fragment { get; }
    /// <summary>Gets the merged binding layout used to create WebGPU pipeline layouts.</summary>
    public IReadOnlyList<WebGpuShaderBinding> Bindings { get; }

    /// <summary>Rejects incompatible stage selection, varying locations, layouts and resource classes.</summary>
    public static WebGpuShaderProgramPair Link(WebGpuShaderStage vertex, WebGpuShaderStage fragment) {
        ArgumentNullException.ThrowIfNull(vertex); ArgumentNullException.ThrowIfNull(fragment);
        if (vertex.Request.Stage != ShaderStage.Vertex || fragment.Request.Stage != ShaderStage.Pixel
            || vertex.Request.Variant != fragment.Request.Variant || vertex.ToolchainFingerprint != fragment.ToolchainFingerprint) {
            throw new InvalidOperationException("WebGPU shader pair has incompatible stages, variants or toolchains.");
        }
        RequireUniqueLocations(vertex.Inputs); RequireUniqueLocations(vertex.Outputs);
        RequireUniqueLocations(fragment.Inputs); RequireUniqueLocations(fragment.Outputs);
        foreach (WebGpuShaderInterfaceElement input in fragment.Inputs) {
            WebGpuShaderInterfaceElement output = vertex.Outputs.SingleOrDefault(value => value.Location == input.Location);
            if (output == null || output.Type != input.Type || output.Semantic != input.Semantic || output.SemanticIndex != input.SemanticIndex
                || output.Interpolation != input.Interpolation) {
                throw new InvalidOperationException("WebGPU varying mismatch at location " + input.Location);
            }
        }
        Dictionary<int, WebGpuShaderBinding> merged = [];
        foreach (WebGpuShaderBinding binding in vertex.Bindings.Concat(fragment.Bindings)) {
            if (binding.Group != 0) { throw new InvalidOperationException("WebGPU pair contains an unsupported bind group."); }
            if (merged.TryGetValue(binding.Binding, out WebGpuShaderBinding previous)) {
                if (previous.Class != binding.Class || previous.Name != binding.Name || previous.RegisterIndex != binding.RegisterIndex
                    || previous.Size != binding.Size || previous.TextureDimension != binding.TextureDimension
                    || previous.SampleType != binding.SampleType || previous.SamplerType != binding.SamplerType
                    || !CompatibleLayout(previous.Layout, binding.Layout) || previous.SourceLayout.GetRawText() != binding.SourceLayout.GetRawText()
                    || previous.Stages.Intersect(binding.Stages).Any()) {
                    throw new InvalidOperationException("WebGPU resource layout mismatch or duplicate at binding " + binding.Binding);
                }
                merged[binding.Binding] = previous.WithStages(previous.Stages.Concat(binding.Stages).Order().ToArray());
            } else { merged.Add(binding.Binding, binding); }
        }
        return new WebGpuShaderProgramPair(vertex, fragment, merged.Values.OrderBy(value => value.Group).ThenBy(value => value.Binding).ToArray());
    }

    /// <summary>Rejects manually constructed or malformed metadata with repeated interface locations.</summary>
    static void RequireUniqueLocations(IReadOnlyList<WebGpuShaderInterfaceElement> values) {
        if (values.Any(value => value.Location < 0) || values.Select(value => value.Location).Distinct().Count() != values.Count) {
            throw new InvalidOperationException("WebGPU shader interface has duplicate or invalid locations.");
        }
    }

    /// <summary>Compares actual layout shapes while ignoring compiler-generated structure/member names.</summary>
    static bool CompatibleLayout(JsonElement left, JsonElement right) {
        if (left.ValueKind != right.ValueKind) { return false; }
        if (left.ValueKind == JsonValueKind.Object) {
            bool structure = left.TryGetProperty("members", out _);
            var properties = left.EnumerateObject().Where(value => value.Name != "name" && !(structure && value.Name == "type")).ToArray();
            var other = right.EnumerateObject().Where(value => value.Name != "name" && !(structure && value.Name == "type")).ToArray();
            return properties.Length == other.Length && properties.All(value => right.TryGetProperty(value.Name, out JsonElement item) && CompatibleLayout(value.Value, item));
        }
        if (left.ValueKind == JsonValueKind.Array) {
            JsonElement[] other = right.EnumerateArray().ToArray();
            return left.GetArrayLength() == other.Length && left.EnumerateArray().Select((value, index) => CompatibleLayout(value, other[index])).All(value => value);
        }
        return left.GetRawText() == right.GetRawText();
    }
}
