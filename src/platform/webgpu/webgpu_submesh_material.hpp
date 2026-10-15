#pragma once
#include <stdexcept>
namespace helengine::webgpu {
/// Matches RenderFrameDrawableClassifier: an unbound submesh reuses authored material slot zero.
inline int ResolveSubmeshMaterialIndex(int submeshIndex,int materialCount) {
    if(submeshIndex<0 || materialCount<0) throw std::runtime_error("Submesh and material count must be nonnegative");
    if(materialCount==0) return -1;
    return submeshIndex<materialCount?submeshIndex:0;
}
}
