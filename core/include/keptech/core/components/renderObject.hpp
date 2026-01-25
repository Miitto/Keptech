#pragma once

#include "keptech/core/rendering/mesh.hpp"
#include "keptech/core/rendering/pipeline.hpp"

namespace keptech::components {
  using Mesh = MeshPtr;

  struct Material {
    PipelinePtr pipeline;
    std::vector<InstanceData> instanceData;
  };
} // namespace keptech::components
