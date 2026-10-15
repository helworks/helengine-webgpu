#include "webgpu_material_bindings.hpp"
#include <cstring>
#include <stdexcept>
#include <algorithm>
namespace helengine::webgpu {
namespace {
void Copy(std::span<const std::byte> source, size_t from, std::span<std::byte> target, size_t to, size_t count) {
    if (from > source.size() || count > source.size()-from || to > target.size() || count > target.size()-to) throw std::runtime_error("Uniform packing range is invalid");
    std::memcpy(target.data()+to, source.data()+from, count);
}
void Structure(const ArtifactJson& source, const ArtifactJson& target, std::span<const std::byte> input, std::span<std::byte> output);
void Member(const ArtifactJson& source, const ArtifactJson& target, std::span<const std::byte> input, std::span<std::byte> output, bool element = false) {
    auto& aggregate = source.at("aggregate");
    if (!element && !aggregate.at("dimensions").empty()) {
        size_t count = aggregate.at("dimensions").at(0).get<size_t>(), sourceStride = source.at("arrayStride").get<size_t>();
        if (aggregate.at("dimensions").size()!=1 || target.at("arrayCount").get<size_t>() != count) throw std::runtime_error("Uniform array dimensions differ after lowering");
        size_t targetStride = target.at("arrayStride").get<size_t>();
        for (size_t i=0;i<count;++i) Member(source,target.at("element"),input.subspan(i*sourceStride,sourceStride),output.subspan(i*targetStride,targetStride),true);
    } else if (!aggregate.at("structure").empty()) Structure(aggregate.at("structure"),target,input,output);
    else if (aggregate.at("columns").get<unsigned>() > 1) {
        size_t rows = aggregate.at("rows").get<size_t>(), columns = aggregate.at("columns").get<size_t>();
        if (rows != target.at("rows") || columns != target.at("columns")) throw std::runtime_error("Uniform matrix shape differs after lowering");
        size_t sourceStride = source.at("matrixStride").get<size_t>(), targetStride = target.at("matrixStride").get<size_t>(); bool row = source.at("matrixOrientation") == "row-major";
        for (size_t r=0;r<rows;++r) for (size_t c=0;c<columns;++c) Copy(input,row?r*sourceStride+c*4:c*sourceStride+r*4,output,c*targetStride+r*4,4);
    } else {
        auto type = source.at("type").get<std::string>();
        if (type.find(":32:") == std::string::npos) throw std::runtime_error("Uniform scalar width is unsupported");
        size_t size = aggregate.at("rows").get<size_t>()*4;
        if (target.at("size").get<size_t>() != size) throw std::runtime_error("Uniform scalar/vector shape differs after lowering");
        Copy(input,0,output,0,size);
    }
}
void Structure(const ArtifactJson& source, const ArtifactJson& target, std::span<const std::byte> input, std::span<std::byte> output) {
    for (auto& member : target.at("members")) {
        auto& members = source.at("members"); auto found = std::find_if(members.begin(),members.end(),[&](auto& m){return m.at("name") == member.at("name");});
        if (found == members.end()) throw std::runtime_error("Lowered uniform member is absent from its original layout");
        Member(*found,member,input.subspan(found->at("offset").get<size_t>(),found->at("size").get<size_t>()),output.subspan(member.at("offset").get<size_t>(),member.at("size").get<size_t>()));
    }
}
}
std::vector<std::byte> PackMaterialBuffer(const ArtifactJson& binding, std::span<const std::byte> original) {
    if (original.size() != binding.at("sourceLayout").at("size").get<size_t>()) throw std::runtime_error("Original named buffer size does not match shader reflection: " + binding.at("name").get<std::string>());
    std::vector<std::byte> result(binding.at("size").get<size_t>()); Structure(binding.at("sourceLayout"),binding.at("layout"),original,result); return result;
}
void WriteEngineMatrix(const ArtifactJson& member, const float* engineRows, std::span<std::byte> original) {
    size_t offset=member.at("offset").get<size_t>(), stride=member.at("matrixStride").get<size_t>(); bool row=member.at("matrixOrientation")=="row-major";
    if (member.at("aggregate").at("rows") != 4 || member.at("aggregate").at("columns") != 4) throw std::runtime_error("Engine matrix requires float4x4");
    for (size_t r=0;r<4;++r) for (size_t c=0;c<4;++c) {
        size_t to=offset+(row?r*stride+c*4:c*stride+r*4);
        if(to+4>original.size()) throw std::runtime_error("Engine matrix exceeds buffer storage");
        // HLSL mul(rowVector, M) lowers to M * columnVector: M is transpose(engineRows).
        std::memcpy(original.data()+to,engineRows+c*4+r,4);
    }
}
}
