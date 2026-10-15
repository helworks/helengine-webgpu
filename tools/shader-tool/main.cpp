#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>
#include <nlohmann/json.hpp>
#include "src/tint/api/tint.h"
#include "src/tint/lang/wgsl/reader/reader.h"
#include "src/tint/lang/wgsl/program/program.h"
#include "src/tint/lang/wgsl/inspector/inspector.h"
#include "src/tint/lang/wgsl/sem/variable.h"
#include "src/tint/lang/wgsl/ast/variable.h"
#include "src/tint/lang/wgsl/ast/module.h"
#include "src/tint/lang/core/type/struct.h"
#include "src/tint/lang/core/type/array.h"
#include "src/tint/lang/core/type/matrix.h"

using Json = nlohmann::ordered_json;

/// Reads bounded binary input without interpreting host endianness as a shader property.
std::vector<uint32_t> ReadSpirv(const std::filesystem::path& path) {
    auto size = std::filesystem::file_size(path);
    if (size < 20 || size > 64 * 1024 * 1024 || size % 4 != 0) throw std::runtime_error("Invalid SPIR-V word count");
    std::vector<uint32_t> words(size / 4);
    std::ifstream stream(path, std::ios::binary);
    if (!stream.read(reinterpret_cast<char*>(words.data()), static_cast<std::streamsize>(size))) throw std::runtime_error("Cannot read SPIR-V input");
    if (words[0] != 0x07230203) throw std::runtime_error("Invalid SPIR-V magic");
    return words;
}

/// Serializes final semantic type layout, including compiler-introduced padding structures.
Json ReflectType(const tint::core::type::Type* type, uint32_t depth = 0) {
    if (depth > 32) throw std::runtime_error("Shader type nesting exceeds 32");
    type = type->UnwrapRef();
    Json result = {{"type", type->FriendlyName()}, {"size", type->Size()}, {"alignment", type->Align()}};
    if (auto* structure = type->As<tint::core::type::Struct>()) {
        result["members"] = Json::array();
        for (auto* member : structure->Members()) {
            Json value = ReflectType(member->Type(), depth + 1);
            value["name"] = member->Name().Name();
            value["offset"] = member->Offset();
            value["size"] = member->Size();
            result["members"].push_back(std::move(value));
        }
    } else if (auto* array = type->As<tint::core::type::Array>()) {
        result["arrayStride"] = array->ImplicitStride();
        result["arrayCount"] = array->ConstantCount().value_or(0);
        result["element"] = ReflectType(array->ElemType(), depth + 1);
    } else if (auto* matrix = type->As<tint::core::type::Matrix>()) {
        result["matrixStride"] = matrix->ColumnStride();
        result["columns"] = matrix->Columns(); result["rows"] = matrix->Rows();
        result["matrixOrientation"] = "column-major";
    }
    return result;
}

/// Reports final stage locations and interpolation from Tint's semantic inspector.
Json ReflectInterface(const std::vector<tint::inspector::StageVariable>& variables) {
    Json result = Json::array();
    for (const auto& value : variables) {
        if (!value.attributes.location) continue;
        result.push_back({{"name", value.name}, {"location", *value.attributes.location},
            {"componentType", static_cast<uint32_t>(value.component_type)},
            {"compositionType", static_cast<uint32_t>(value.composition_type)},
            {"interpolationType", static_cast<uint32_t>(value.interpolation_type)},
            {"interpolationSampling", static_cast<uint32_t>(value.interpolation_sampling)}});
    }
    return result;
}

/// Resolves final resource classes into the backend's stable metadata spelling.
std::string ResourceClass(tint::inspector::ResourceBinding::ResourceType type) {
    using Type = tint::inspector::ResourceBinding::ResourceType;
    switch (type) {
        case Type::kUniformBuffer: return "constant-buffer";
        case Type::kSampler: return "sampler";
        case Type::kSampledTexture: return "texture";
        case Type::kDepthTexture: return "depth-texture";
        default: throw std::runtime_error("Shader uses a resource class outside the current WebGPU vertex/pixel contract");
    }
}

/// Reflects the generated WGSL semantic model rather than assuming SPIR-V and WGSL layouts coincide.
Json Reflect(const tint::Program& program) {
    tint::inspector::Inspector inspector(program);
    auto entries = inspector.GetEntryPoints();
    if (entries.size() != 1) throw std::runtime_error("A cooked shader stage must have exactly one entry point");
    const auto& entry = entries[0];
    std::string stage;
    if (entry.stage == tint::inspector::PipelineStage::kVertex) stage = "vertex";
    else if (entry.stage == tint::inspector::PipelineStage::kFragment) stage = "fragment";
    else throw std::runtime_error("Only vertex and fragment stages are supported");
    Json result = {{"schemaVersion", 1}, {"entryPoint", entry.name}, {"stage", stage},
        {"inputs", ReflectInterface(entry.input_variables)}, {"outputs", ReflectInterface(entry.output_variables)}, {"bindings", Json::array()}};
    for (const auto& resource : inspector.GetResourceBindings(entry.name)) {
        if (resource.bind_group != 0) throw std::runtime_error("Only register space/group zero is supported");
        bool buffer = resource.resource_type == tint::inspector::ResourceBinding::ResourceType::kUniformBuffer;
        bool sampler = resource.resource_type == tint::inspector::ResourceBinding::ResourceType::kSampler;
        Json binding = {{"name", resource.variable_name}, {"class", ResourceClass(resource.resource_type)},
            {"group", resource.bind_group}, {"binding", resource.binding}, {"size", buffer ? resource.size : 0},
            {"textureDimension", !buffer && !sampler ? static_cast<uint32_t>(resource.dim) : 0},
            {"sampleType", !buffer && !sampler ? static_cast<uint32_t>(resource.sampled_kind) : 0},
            {"samplerType", sampler ? static_cast<uint32_t>(resource.sampler_type) : 0}};
        if (resource.resource_type == tint::inspector::ResourceBinding::ResourceType::kUniformBuffer) {
            bool found = false;
            for (auto* variable : program.AST().GlobalVariables()) {
                auto* semantic = program.Sem().Get<tint::sem::GlobalVariable>(variable);
                if (!semantic || !semantic->Attributes().binding_point) continue;
                auto point = *semantic->Attributes().binding_point;
                if (point.group == resource.bind_group && point.binding == resource.binding) {
                    binding["layout"] = ReflectType(semantic->Type());
                    found = true;
                }
            }
            if (!found) throw std::runtime_error("Uniform reflection has no corresponding semantic variable");
        }
        result["bindings"].push_back(std::move(binding));
    }
    if (inspector.has_error()) throw std::runtime_error(inspector.error());
    return result;
}

/// Writes only complete validated output; the caller owns publication of final shader pairs.
void Write(const std::filesystem::path& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary);
    if (!stream.write(text.data(), static_cast<std::streamsize>(text.size()))) throw std::runtime_error("Cannot write shader output");
}

/// Exposes pinned SPIR-V lowering and final reflection as a small command-line build tool.
int wmain(int argc, wchar_t** argv) {
    try {
        if (argc != 7) throw std::runtime_error("Usage: helengine-webgpu-shader --input source.spv --wgsl output.wgsl --reflection reflection.json");
        std::map<std::wstring, std::wstring> args;
        for (int index = 1; index < argc; index += 2) {
            if (!args.emplace(argv[index], argv[index + 1]).second) throw std::runtime_error("Duplicate shader argument");
        }
        if (!args.contains(L"--input") || !args.contains(L"--wgsl") || !args.contains(L"--reflection")) throw std::runtime_error("Invalid shader arguments");
        auto words = ReadSpirv(std::filesystem::path(args.at(L"--input")));
        tint::Initialize();
        auto generated = tint::SpirvToWgsl(words);
        if (generated != tint::Success) throw std::runtime_error(generated.Failure().reason);
        tint::Source::File source("lowered.wgsl", generated.Get());
        auto program = tint::wgsl::reader::Parse(&source);
        if (!program.IsValid()) throw std::runtime_error(program.Diagnostics().Str());
        auto metadata = Reflect(program);
        Write(std::filesystem::path(args.at(L"--wgsl")), generated.Get());
        Write(std::filesystem::path(args.at(L"--reflection")), metadata.dump(2));
        tint::Shutdown();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "WEBGPU_TINT: " << error.what() << '\n';
        return 1;
    }
}
