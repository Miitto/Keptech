#pragma once

#include "keptech/core/rendering/mesh.hpp"
#include "keptech/core/rendering/pipeline.hpp"

namespace keptech::components {

  using Mesh = core::rendering::Mesh::Handle;

  struct Material {
    core::rendering::Pipeline::Handle pipeline;
    std::vector<core::rendering::Pipeline::InstanceData> instanceData;
  };
} // namespace keptech::components
