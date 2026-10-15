#include "webgpu_shader_artifact_reader.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <set>
#include <stdexcept>
#include <algorithm>

namespace helengine::webgpu {
namespace {
void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
uint32_t U32(std::span<const std::byte> bytes, size_t offset) {
    Check(offset + 4 <= bytes.size(), "Artifact prefix is truncated"); uint32_t result = 0;
    for (unsigned i = 0; i < 4; ++i) result |= uint32_t(bytes[offset + i]) << (i * 8);
    return result;
}
std::string Section(std::span<const std::byte> bytes, size_t start, size_t count) {
    Check(start <= bytes.size() && count <= bytes.size() - start, "Artifact section is outside the envelope");
    std::string text(reinterpret_cast<const char*>(bytes.data() + start), count);
    ArtifactJson(text).dump(); // Strict UTF-8 validation also applies to WGSL sections.
    return text;
}
std::string Text(const ArtifactJson& value, const char* key) {
    auto text = value.at(key).get<std::string>();
    Check(!text.empty() && text.size() <= 4096 && text.find('\0') == std::string::npos, "Artifact identity is invalid"); return text;
}
void Interfaces(const ArtifactJson& elements) {
    Check(elements.is_array(), "Shader interface must be an array"); std::set<int> locations;
    for (auto& item : elements) {
        Text(item, "semantic"); Text(item, "type"); Text(item, "interpolation");
        int location = item.at("location").get<int>();
        Check(location >= 0 && location < 32 && locations.insert(location).second && item.at("semanticIndex").get<int>() >= 0, "Shader interface location is invalid");
    }
}
void SourceLayout(const ArtifactJson& layout, size_t available, unsigned depth) {
    Check(depth <= 24 && layout.at("size").get<size_t>() <= available && layout.at("members").is_array(), "Original uniform layout is invalid");
    std::set<std::string> names; size_t previousEnd = 0;
    for (auto& member : layout.at("members")) {
        auto name = Text(member, "name"); size_t offset = member.at("offset").get<size_t>(), size = member.at("size").get<size_t>();
        Check(size > 0 && offset >= previousEnd && offset <= available && size <= available - offset && names.insert(name).second, "Original uniform member is outside its buffer");
        previousEnd = offset + size;
        auto& aggregate = member.at("aggregate");
        auto dimensions = aggregate.at("dimensions").get<std::vector<uint32_t>>();
        Check(dimensions.size() <= 1, "Multidimensional original uniform arrays need per-dimension stride metadata");
        size_t elementSize = size;
        if (!dimensions.empty()) {
            auto stride = member.at("arrayStride").get<size_t>();
            Check(dimensions[0] > 0 && stride > 0 && uint64_t(stride) * dimensions[0] <= size, "Original array stride is invalid"); elementSize = stride;
        }
        if (!aggregate.at("structure").empty()) SourceLayout(aggregate.at("structure"), elementSize, depth + 1);
        unsigned columns = aggregate.at("columns").get<unsigned>(), rows = aggregate.at("rows").get<unsigned>();
        Check(rows >= 1 && rows <= 4 && columns >= 1 && columns <= 4, "Original uniform shape is invalid");
        if (columns > 1) {
            auto orientation = Text(member, "matrixOrientation"); size_t stride = member.at("matrixStride").get<size_t>();
            Check(orientation == "row-major" || orientation == "column-major", "Original matrix orientation is invalid");
            Check(stride >= 4 * (orientation == "row-major" ? columns : rows) && stride * (orientation == "row-major" ? rows : columns) <= elementSize, "Original matrix stride exceeds its member");
        }
    }
}
}
std::vector<std::byte> ReadArtifactFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("Missing WebGPU artifact: " + path);
    auto size = input.tellg(); Check(size > 0 && size <= 64 * 1024 * 1024, "Artifact file size is invalid");
    std::vector<std::byte> bytes(static_cast<size_t>(size)); input.seekg(0);
    Check(bool(input.read(reinterpret_cast<char*>(bytes.data()), size)), "Artifact read was truncated"); return bytes;
}
ArtifactJson ParseArtifactJson(std::string_view text) {
    std::vector<std::set<std::string>> objects;
    auto callback = [&](int depth, ArtifactJson::parse_event_t event, ArtifactJson& parsed) {
        Check(depth <= 32, "Artifact JSON exceeds maximum depth");
        if (event == ArtifactJson::parse_event_t::object_start) objects.emplace_back();
        if (event == ArtifactJson::parse_event_t::key) Check(!objects.empty() && objects.back().insert(parsed.get<std::string>()).second, "Artifact JSON has duplicate keys");
        if (event == ArtifactJson::parse_event_t::object_end) objects.pop_back(); return true;
    };
    return ArtifactJson::parse(text.begin(), text.end(), callback);
}
std::string ArtifactHash(std::string_view text) {
    constexpr uint32_t k[64] = {0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    std::vector<uint8_t> bytes(text.begin(), text.end()); uint64_t bits = uint64_t(bytes.size()) * 8;
    bytes.push_back(0x80); while (bytes.size() % 64 != 56) bytes.push_back(0);
    for (int i = 7; i >= 0; --i) bytes.push_back(static_cast<uint8_t>(bits >> (i * 8)));
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    for (size_t offset = 0; offset < bytes.size(); offset += 64) {
        uint32_t w[64]; for (unsigned i = 0; i < 16; ++i) { w[i] = 0; for (unsigned j = 0; j < 4; ++j) w[i] = (w[i] << 8) | bytes[offset + i * 4 + j]; }
        for (unsigned i = 16; i < 64; ++i) w[i] = w[i-16] + (std::rotr(w[i-15],7)^std::rotr(w[i-15],18)^(w[i-15]>>3)) + w[i-7] + (std::rotr(w[i-2],17)^std::rotr(w[i-2],19)^(w[i-2]>>10));
        std::array<uint32_t,8> v; std::copy(h,h+8,v.begin());
        for (unsigned i = 0; i < 64; ++i) {
            uint32_t t1 = v[7]+(std::rotr(v[4],6)^std::rotr(v[4],11)^std::rotr(v[4],25))+((v[4]&v[5])^(~v[4]&v[6]))+k[i]+w[i];
            uint32_t t2 = (std::rotr(v[0],2)^std::rotr(v[0],13)^std::rotr(v[0],22))+((v[0]&v[1])^(v[0]&v[2])^(v[1]&v[2]));
            v = {t1+t2,v[0],v[1],v[2],v[3]+t1,v[4],v[5],v[6]};
        }
        for (unsigned i = 0; i < 8; ++i) h[i] += v[i];
    }
    std::string result; constexpr char digits[] = "0123456789abcdef";
    for (uint32_t word : h) for (int i = 7; i >= 0; --i) result += digits[(word >> (i * 4)) & 15]; return result;
}
std::string ShaderArtifactPath(const ArtifactJson& selection) {
    std::string framed;
    for (const char* key : {"shaderAssetId", "vertexProgram", "pixelProgram", "variant"}) {
        auto value = Text(selection, key); for (int i = 0; i < 4; ++i) framed += char(uint32_t(value.size()) >> (8*i)); framed += value;
    }
    return "cooked/shaders/" + ArtifactHash(framed) + ".wgsp";
}
void ValidateContentPath(const std::string& path) {
    Check(path.starts_with("cooked/") && path.size() <= 4096 && path.find_first_of("\\:") == std::string::npos && path.find('\0') == std::string::npos, "Artifact content path is invalid");
    size_t start = 0; while (start <= path.size()) {
        size_t end = path.find('/', start); if (end == std::string::npos) end = path.size();
        auto part = path.substr(start, end - start); Check(!part.empty() && part != "." && part != "..", "Artifact content path contains traversal"); start = end + 1;
    }
}
void ValidateFinalLayout(const ArtifactJson& layout, size_t available, unsigned depth) {
    Check(depth <= 24 && layout.is_object(), "Uniform layout nesting is invalid");
    size_t size = layout.at("size").get<size_t>(), alignment = layout.at("alignment").get<size_t>();
    Check(size > 0 && size <= available && alignment > 0 && alignment <= 65536 && (alignment & (alignment-1)) == 0, "Final uniform size/alignment is invalid"); Text(layout,"type");
    if (layout.contains("members")) {
        Check(layout.at("members").is_array() && !layout.at("members").empty(), "Final uniform members must be a nonempty array");
        std::set<std::string> names; size_t previousEnd = 0;
        for (auto& member : layout.at("members")) {
            Check(names.insert(Text(member,"name")).second, "Final uniform member is duplicated");
            size_t offset = member.at("offset").get<size_t>(), memberSize = member.at("size").get<size_t>();
            ValidateFinalLayout(member, memberSize, depth+1);
            Check(offset >= previousEnd && offset <= size && memberSize <= size - offset && offset % member.at("alignment").get<size_t>() == 0, "Final uniform member is outside its buffer");
            previousEnd = offset + memberSize;
        }
    } else if (layout.contains("arrayStride")) {
        size_t stride = layout.at("arrayStride").get<size_t>(), count = layout.at("arrayCount").get<size_t>();
        Check(stride > 0 && count > 0 && count <= size/stride, "Final uniform array stride is invalid"); ValidateFinalLayout(layout.at("element"), stride, depth+1);
    } else if (layout.contains("matrixStride")) {
        size_t columns = layout.at("columns").get<size_t>(), rows = layout.at("rows").get<size_t>(), stride = layout.at("matrixStride").get<size_t>();
        Check(columns >= 2 && columns <= 4 && rows >= 2 && rows <= 4 && stride >= rows * 4 && stride * columns <= size && layout.at("matrixOrientation") == "column-major", "Final uniform matrix is invalid");
    }
}
WebGpuShaderArtifact ReadShaderArtifact(std::span<const std::byte> bytes) {
    Check(bytes.size() >= 28 && bytes.size() <= 64*1024*1024 && Section(bytes,0,4) == "WGSP" && U32(bytes,4) == 1 && U32(bytes,8) == 28 && U32(bytes,12) == bytes.size(), "WGSP header/version/length is invalid");
    size_t json = U32(bytes,16), vertex = U32(bytes,20), fragment = U32(bytes,24);
    Check(json && vertex && fragment && uint64_t(28)+json+vertex+fragment == bytes.size(), "WGSP sections are invalid");
    WebGpuShaderArtifact result {ParseArtifactJson(Section(bytes,28,json)), Section(bytes,28+json,vertex), Section(bytes,28+json+vertex,fragment)};
    auto& m = result.Metadata; ShaderArtifactPath(m); Text(m,"vertexEntryPoint"); Text(m,"fragmentEntryPoint");
    Check(Text(m,"target") == "webgpu" && Text(m,"vertexWgslSha256") == ArtifactHash(result.Vertex) && Text(m,"fragmentWgslSha256") == ArtifactHash(result.Fragment), "WGSP stage hash/target mismatch");
    Text(m.at("toolchain"),"fingerprint"); Check(Text(m.at("toolchain"),"dawnRevision") == "31e25af254ab572c77054edec4946d2244e184dd", "WGSP Tint revision is incompatible"); Text(m.at("toolchain"),"adapterVersion");
    Check(m.at("dependencies").is_array(), "WGSP dependencies must be an array"); for (auto& d : m.at("dependencies")) { Text(d,"path"); Text(d,"hash"); }
    Interfaces(m.at("vertexInputs")); Interfaces(m.at("vertexOutputs")); Interfaces(m.at("fragmentInputs"));
    for (auto& input : m.at("fragmentInputs")) {
        auto output = std::find_if(m.at("vertexOutputs").begin(), m.at("vertexOutputs").end(), [&](auto& o) { return o.at("location") == input.at("location"); });
        Check(output != m.at("vertexOutputs").end() && output->at("type") == input.at("type") && output->at("interpolation") == input.at("interpolation"), "WGSP stage interface mismatch");
    }
    Check(m.at("bindings").is_array(), "WGSP bindings must be an array"); std::set<int> slots; std::set<std::string> names;
    for (auto& b : m.at("bindings")) {
        Check(names.insert(Text(b,"name")).second && b.at("group") == 0 && b.at("registerSpace") == 0 && b.at("registerIndex").get<int>() >= 0, "WGSP resource identity is invalid");
        int slot = b.at("binding").get<int>(); Check(slot >= 0 && slot < 1000 && slots.insert(slot).second, "WGSP resource slot is invalid");
        auto resource = Text(b,"class"); Check(resource == "constant-buffer" || resource == "texture" || resource == "depth-texture" || resource == "sampler", "WGSP resource class is unsupported");
        Check(b.at("stages").is_array() && !b.at("stages").empty() && b.at("stages").size() <= 2, "WGSP visibility is invalid"); std::set<std::string> stages;
        for (auto& stage : b.at("stages")) { auto s = stage.get<std::string>(); Check((s == "vertex" || s == "fragment") && stages.insert(s).second, "WGSP visibility stage is invalid"); }
        if (resource == "constant-buffer") {
            size_t size = b.at("size").get<size_t>(); Check(size && size <= 65536, "WGSP uniform exceeds device limits");
            Check(b.at("layout").at("members").is_array() && !b.at("layout").at("members").empty(), "WGSP uniform structure has invalid members");
            ValidateFinalLayout(b.at("layout"),size); SourceLayout(b.at("sourceLayout"),b.at("sourceLayout").at("size").get<size_t>(),0);
            Check(b.at("sourceLayout").at("size").get<size_t>() <= 65536, "WGSP original uniform exceeds limits");
        } else if (resource == "texture" || resource == "depth-texture") Check((b.at("textureDimension") == 1 || b.at("textureDimension") == 4) && b.at("sampleType") == (resource == "depth-texture" ? 0 : 5), "WGSP texture type is unsupported");
        else Check(b.at("samplerType") == 0 || b.at("samplerType") == 3, "WGSP sampler type is unsupported");
    }
    return result;
}
}
