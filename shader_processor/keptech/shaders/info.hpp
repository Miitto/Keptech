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
    const char* name = nullptr;
    Vertex vertex{};
    Fragment fragment{};
    uint32_t bindlessIndex = ~0u;
    Resources globalResources{};
    std::vector<Resources> entryPointResources{};
  };
} // namespace kt::shaders