#pragma once

#include "keptech/shaders/fragment.hpp"
#include "keptech/shaders/resources.hpp"
#include "keptech/shaders/vertex.hpp"

namespace kt::shaders {
  struct ShaderInfo {
    const char* name;
    Vertex vertex;
    Fragment fragment;
    uint32_t bindlessIndex;
    std::vector<ResourceBinding> resources;
    size_t pushConstantSize;
  };
} // namespace kt::shaders