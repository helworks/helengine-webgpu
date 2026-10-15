#include "platform/webgpu/webgpu_shader_artifact_reader.hpp"
#include "platform/webgpu/webgpu_material_artifact_reader.hpp"
#include "platform/webgpu/webgpu_material_bindings.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstring>

using namespace helengine::webgpu;
void Require(bool condition) { if (!condition) throw std::runtime_error("Artifact assertion failed"); }
template<class F> void Reject(F operation) {
    bool rejected = false; try { operation(); } catch (const std::exception&) { rejected = true; }
    Require(rejected);
}
void Put32(std::vector<std::byte>& bytes,size_t offset,uint32_t value) { for(unsigned i=0;i<4;++i) bytes[offset+i]=std::byte(value>>(i*8)); }
std::vector<std::byte> Repack(const WebGpuShaderArtifact& shader, const ArtifactJson& metadata) {
    std::string json=metadata.dump(); std::vector<std::byte> bytes(28+json.size()+shader.Vertex.size()+shader.Fragment.size());
    std::memcpy(bytes.data(),"WGSP",4); Put32(bytes,4,1); Put32(bytes,8,28); Put32(bytes,12,static_cast<uint32_t>(bytes.size()));
    Put32(bytes,16,static_cast<uint32_t>(json.size())); Put32(bytes,20,static_cast<uint32_t>(shader.Vertex.size())); Put32(bytes,24,static_cast<uint32_t>(shader.Fragment.size()));
    std::memcpy(bytes.data()+28,json.data(),json.size()); std::memcpy(bytes.data()+28+json.size(),shader.Vertex.data(),shader.Vertex.size()); std::memcpy(bytes.data()+28+json.size()+shader.Vertex.size(),shader.Fragment.data(),shader.Fragment.size()); return bytes;
}
int main(int argc, char** argv) {
    try {
        Require(argc == 2); std::string root = argv[1]; int shaders = 0;
        Require(ArtifactHash("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        Require(ArtifactHash("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
        for (auto& file : std::filesystem::directory_iterator(root + "/shaders")) {
            auto bytes = ReadArtifactFile(file.path().string());
            auto shader = ReadShaderArtifact(bytes); ++shaders;
            Require(!shader.Vertex.empty() && !shader.Fragment.empty());
            auto broken = bytes; broken[4] = std::byte{2}; Reject([&] { ReadShaderArtifact(broken); });
            broken = bytes; broken.push_back(std::byte{}); Reject([&] { ReadShaderArtifact(broken); });
            broken = bytes; broken[28] = std::byte{255}; Reject([&] { ReadShaderArtifact(broken); });
            broken = bytes; broken.back() = std::byte{1}; Reject([&] { ReadShaderArtifact(broken); });
            for (auto& binding : shader.Metadata.at("bindings")) {
                if (binding.at("class") != "constant-buffer") continue;
                std::vector<std::byte> data(binding.at("sourceLayout").at("size").get<size_t>());
                Require(PackMaterialBuffer(binding, data).size() == binding.at("size").get<size_t>());
                Reject([&] { PackMaterialBuffer(binding,{}); });
                auto changed=shader.Metadata;
                auto slot=binding.at("binding");
                for(auto& b:changed.at("bindings")) if(b.at("binding")==slot) b.at("layout").at("members").at(0)["offset"]=65536;
                Reject([&] { ReadShaderArtifact(Repack(shader,changed)); });
                changed=shader.Metadata;
                for(auto& b:changed.at("bindings")) if(b.at("binding")==slot) b.at("layout").at("members").at(0)["alignment"]=0;
                Reject([&] { ReadShaderArtifact(Repack(shader,changed)); });
            }
            if(!shader.Metadata.at("fragmentInputs").empty()) {
                auto changed=shader.Metadata; changed.at("fragmentInputs").at(0)["type"]="FP32:32:4:1-invalid";
                Reject([&] { ReadShaderArtifact(Repack(shader,changed)); });
            }
        }
        Require(shaders >= 6);
        std::string materialPath=std::filesystem::exists(root+"/M4R4.wgpm")?root+"/M4R4.wgpm":root+"/matrix_fixture.wgpm";
        auto bytes = ReadArtifactFile(materialPath); auto material = ReadMaterialArtifact(bytes);
        Require(material.Constants.at("MetallicBuffer").size() == 16);
        bytes[12] = std::byte{255}; Reject([&] { ReadMaterialArtifact(bytes); });
        Reject([&] { ParseArtifactJson("{\"name\":1,\"name\":2}"); });
        Reject([&] { ValidateContentPath("cooked/../escape"); });
        std::cout << "WGSP/WGPM native checks passed; shaders=" << shaders << '\n'; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
