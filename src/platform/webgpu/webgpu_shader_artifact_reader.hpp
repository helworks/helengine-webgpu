#pragma once
#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <vector>
#include <cstddef>

namespace helengine::webgpu {
using ArtifactJson = nlohmann::json;
/// Owns a fully checked shader pair before any device objects are created.
struct WebGpuShaderArtifact { ArtifactJson Metadata; std::string Vertex, Fragment; };
/// Reads a bounded binary artifact without trusting source-controlled length fields.
std::vector<std::byte> ReadArtifactFile(const std::string& path);
/// Parses strict UTF-8 JSON, rejecting duplicate keys and excessive depth.
ArtifactJson ParseArtifactJson(std::string_view text);
/// Computes the published lowercase SHA-256 identity from bytes.
std::string ArtifactHash(std::string_view text);
/// Reproduces the managed length-framed program selection identity.
std::string ShaderArtifactPath(const ArtifactJson& selection);
/// Requires a canonical cooked content path without filesystem traversal.
void ValidateContentPath(const std::string& path);
/// Checks a recursively lowered uniform layout against its bounded storage.
void ValidateFinalLayout(const ArtifactJson& layout, size_t available, unsigned depth = 0);
/// Verifies the complete WGSP v1 contract and its stage text hashes.
WebGpuShaderArtifact ReadShaderArtifact(std::span<const std::byte> bytes);
}
