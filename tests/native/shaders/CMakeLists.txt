cmake_minimum_required(VERSION 3.26)
project(shader_artifact_checks LANGUAGES CXX)
get_filename_component(BACKEND_ROOT "../../.." ABSOLUTE)
set(HELENGINE_WEBGPU_SHADER_TOOL_ROOT "C:/dev/helworks/tools/webgpu-shaders" CACHE PATH "Pinned shader tools")
add_executable(shader_artifact_harness ../shader_artifact_harness.cpp
    "${BACKEND_ROOT}/src/platform/webgpu/webgpu_shader_artifact_reader.cpp"
    "${BACKEND_ROOT}/src/platform/webgpu/webgpu_material_artifact_reader.cpp"
    "${BACKEND_ROOT}/src/platform/webgpu/webgpu_material_bindings.cpp")
target_compile_features(shader_artifact_harness PRIVATE cxx_std_20)
target_include_directories(shader_artifact_harness PRIVATE "${BACKEND_ROOT}/src" "${HELENGINE_WEBGPU_SHADER_TOOL_ROOT}/include")
