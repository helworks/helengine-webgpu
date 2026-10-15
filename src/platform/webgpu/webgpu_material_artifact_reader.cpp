#include "webgpu_material_artifact_reader.hpp"
#include <set>
#include <stdexcept>
namespace helengine::webgpu {
namespace {
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
uint32_t U32(std::span<const std::byte> bytes, size_t offset) {
    uint32_t result = 0; for (int i = 0; i < 4; ++i) result |= uint32_t(bytes[offset+i]) << (i*8); return result;
}
std::vector<std::byte> Decode(const std::string& value) {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    Check(!value.empty() && value.size() % 4 == 0 && value.size() <= 87384, "WGPM base64 length is invalid");
    std::vector<std::byte> result;
    for (size_t i = 0; i < value.size(); i += 4) {
        uint32_t word = 0; int padding = 0;
        for (size_t j = 0; j < 4; ++j) {
            auto c = value[i+j]; auto digit = alphabet.find(c);
            if (c == '=') { Check(i+4 == value.size() && j >= 2, "WGPM base64 padding is invalid"); ++padding; digit = 0; }
            else Check(digit != std::string_view::npos && !padding, "WGPM base64 contains invalid characters");
            word = (word << 6) | uint32_t(digit);
        }
        Check(padding <= 2 && (!padding || (word & ((1u << (padding == 2 ? 16 : 8))-1)) == 0), "WGPM base64 is not canonical");
        for (int j = 0; j < 3-padding; ++j) result.push_back(std::byte(word >> (16-j*8)));
    }
    Check(!result.empty() && result.size() <= 65536 && result.size()%4 == 0, "WGPM constant buffer size is invalid"); return result;
}
std::string Text(const ArtifactJson& value, const char* key) {
    auto text = value.at(key).get<std::string>(); Check(!text.empty() && text.size() <= 4096 && text.find('\0') == std::string::npos, "WGPM identity is invalid"); return text;
}
}
WebGpuMaterialArtifact ReadMaterialArtifact(std::span<const std::byte> bytes) {
    Check(bytes.size() > 16 && bytes.size() <= 16+16*1024*1024 && std::string_view(reinterpret_cast<const char*>(bytes.data()),4) == "WGPM"
        && U32(bytes,4) == 1 && U32(bytes,8) == bytes.size() && uint64_t(U32(bytes,12))+16 == bytes.size(), "WGPM header/version/length is invalid");
    WebGpuMaterialArtifact result {ParseArtifactJson(std::string_view(reinterpret_cast<const char*>(bytes.data()+16),bytes.size()-16)), {}};
    auto& m = result.Metadata; Text(m,"materialAssetId"); Check(Text(m,"shaderPath") == ShaderArtifactPath(m), "WGPM shader selection/path mismatch");
    m.at("castsShadows").get<bool>(); m.at("receivesShadows").get<bool>();
    auto& state = m.at("renderState"); auto alpha = Text(state,"alphaMode"); Check(alpha == "opaque" || alpha == "alpha-blend" || alpha == "additive", "WGPM alpha mode is invalid");
    for (auto key : {"doubleSided","useVertexColor","lit","depthTest","depthWrite"}) state.at(key).get<bool>();
    Check(m.at("constantBuffers").is_array() && m.at("textures").is_array(), "WGPM resource lists are invalid");
    for (auto& buffer : m.at("constantBuffers")) Check(result.Constants.emplace(Text(buffer,"name"),Decode(buffer.at("data").get<std::string>())).second, "WGPM named buffer is duplicated");
    std::set<std::string> names;
    for (auto& texture : m.at("textures")) { Check(names.insert(Text(texture,"name")).second, "WGPM texture is duplicated"); Text(texture,"assetId"); ValidateContentPath(Text(texture,"path")); }
    return result;
}
}
