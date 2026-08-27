#pragma once

#include "keptech/shaders/fragment.hpp"
#include "keptech/shaders/resources.hpp"
#include "keptech/shaders/vertex.hpp"
#include <unordered_map>

namespace kt::shaders {
  struct PushConstantData {
    size_t size;
    std::unordered_map<const char*, size_t> memberOffsets;
  };

  struct ShaderInfo {
    const char* name;
    Vertex vertex;
    Fragment fragment;
    uint32_t bindlessIndex;
    std::vector<ResourceSet> resources;
    PushConstantData pushConstants;
  };
} // namespace kt::shaders