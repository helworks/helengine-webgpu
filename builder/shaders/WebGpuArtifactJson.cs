using System.Text;
using System.Text.Json;

namespace helengine.webgpu.builder;

/// <summary>Provides strict bounded JSON parsing and deterministic canonical metadata bytes.</summary>
internal static class WebGpuArtifactJson {
    /// <summary>Rejects invalid UTF-8 instead of replacing malformed bytes.</summary>
    internal static readonly UTF8Encoding Utf8 = new(false, true);
    /// <summary>Uses the published camel-case schema for typed compiler records.</summary>
    internal static readonly JsonSerializerOptions Options = new() { PropertyNamingPolicy = JsonNamingPolicy.CamelCase, MaxDepth = 32 };

    /// <summary>Parses an owned metadata value and rejects duplicates throughout nested structures.</summary>
    internal static JsonElement Parse(ReadOnlySpan<byte> bytes) {
        try {
            using JsonDocument document = JsonDocument.Parse(Utf8.GetString(bytes), new JsonDocumentOptions { MaxDepth = 32 });
            ValidateUnique(document.RootElement);
            return document.RootElement.Clone();
        } catch (Exception error) when (error is JsonException || error is DecoderFallbackException) {
            throw new InvalidDataException("WebGPU artifact contains malformed JSON or UTF-8.", error);
        }
    }

    /// <summary>Serializes object keys in ordinal order while retaining meaningful array ordering.</summary>
    internal static byte[] Serialize(JsonElement value) {
        ValidateUnique(value);
        using MemoryStream output = new();
        using (Utf8JsonWriter writer = new(output)) { Write(writer, value); }
        return output.ToArray();
    }

    /// <summary>Requires a nonempty bounded identity, path or entry-point field.</summary>
    internal static string Text(JsonElement value, string name) {
        if (value.ValueKind != JsonValueKind.Object || !value.TryGetProperty(name, out JsonElement field)
            || field.ValueKind != JsonValueKind.String || string.IsNullOrWhiteSpace(field.GetString())
            || field.GetString().Length > 4096 || field.GetString().Contains('\0')) {
            throw new InvalidDataException("WebGPU artifact is missing valid text field: " + name);
        }
        return field.GetString();
    }

    /// <summary>Requires a correctly typed container for the published metadata schema.</summary>
    internal static JsonElement Container(JsonElement value, string name, JsonValueKind kind) {
        if (value.ValueKind != JsonValueKind.Object || !value.TryGetProperty(name, out JsonElement field) || field.ValueKind != kind) {
            throw new InvalidDataException("WebGPU artifact has invalid container: " + name);
        }
        return field;
    }

    /// <summary>Walks every object so nested duplicate keys cannot override validated values.</summary>
    static void ValidateUnique(JsonElement value) {
        if (value.ValueKind == JsonValueKind.Object) {
            HashSet<string> names = new(StringComparer.Ordinal);
            foreach (JsonProperty property in value.EnumerateObject()) {
                if (!names.Add(property.Name)) { throw new InvalidDataException("WebGPU artifact contains duplicate JSON property: " + property.Name); }
                ValidateUnique(property.Value);
            }
        } else if (value.ValueKind == JsonValueKind.Array) {
            foreach (JsonElement item in value.EnumerateArray()) { ValidateUnique(item); }
        }
    }

    /// <summary>Writes a checked metadata value without nondeterministic object property ordering.</summary>
    static void Write(Utf8JsonWriter writer, JsonElement value) {
        if (value.ValueKind == JsonValueKind.Object) {
            writer.WriteStartObject();
            foreach (JsonProperty property in value.EnumerateObject().OrderBy(item => item.Name, StringComparer.Ordinal)) {
                writer.WritePropertyName(property.Name); Write(writer, property.Value);
            }
            writer.WriteEndObject();
        } else if (value.ValueKind == JsonValueKind.Array) {
            writer.WriteStartArray();
            foreach (JsonElement item in value.EnumerateArray()) { Write(writer, item); }
            writer.WriteEndArray();
        } else { value.WriteTo(writer); }
    }
}
