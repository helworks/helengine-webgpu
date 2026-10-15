using System.Text.Json;

namespace helengine.webgpu.builder;

/// <summary>Carries WGSL and the final structured reflection emitted by the pinned Tint adapter.</summary>
public sealed class WebGpuTintResult {
    /// <summary>Initializes a validated adapter response with independent reflection ownership.</summary>
    WebGpuTintResult(string wgsl, JsonElement reflection) {
        Wgsl = wgsl;
        Reflection = reflection;
    }

    /// <summary>Gets WGSL validated by the native compiler before publication.</summary>
    public string Wgsl { get; }

    /// <summary>Gets the lowered program layout, cloned out of its temporary JSON document.</summary>
    public JsonElement Reflection { get; }

    /// <summary>Rejects incomplete or malformed native output rather than returning partial shader data.</summary>
    public static WebGpuTintResult Parse(string wgsl, string metadata) {
        if (string.IsNullOrWhiteSpace(wgsl)) {
            throw new InvalidDataException("WebGPU shader adapter returned empty WGSL.");
        }
        try {
            using JsonDocument document = JsonDocument.Parse(metadata);
            JsonElement root = document.RootElement;
            if (root.ValueKind != JsonValueKind.Object || !root.TryGetProperty("schemaVersion", out JsonElement version)
                || version.GetInt32() != 1 || !root.TryGetProperty("entryPoint", out JsonElement entry)
                || entry.ValueKind != JsonValueKind.String || string.IsNullOrWhiteSpace(entry.GetString())
                || !root.TryGetProperty("stage", out JsonElement stage) || stage.GetString() != "vertex" && stage.GetString() != "fragment"
                || !root.TryGetProperty("bindings", out JsonElement bindings) || bindings.ValueKind != JsonValueKind.Array
                || !root.TryGetProperty("inputs", out JsonElement inputs) || inputs.ValueKind != JsonValueKind.Array
                || !root.TryGetProperty("outputs", out JsonElement outputs) || outputs.ValueKind != JsonValueKind.Array) {
                throw new InvalidDataException("WebGPU shader adapter returned incomplete reflection.");
            }
            return new WebGpuTintResult(wgsl, root.Clone());
        } catch (Exception error) when (error is JsonException || error is InvalidOperationException || error is FormatException) {
            throw new InvalidDataException("WebGPU shader adapter returned malformed reflection.", error);
        }
    }
}
