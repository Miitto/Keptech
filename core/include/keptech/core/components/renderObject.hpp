#pragma once

#include "keptech/core/rendering/material.hpp"
#include "keptech/core/rendering/mesh.hpp"

#include <keptech/core/gui.h>

namespace keptech::components {
  struct Mesh {
    core::rendering::Mesh::Handle mesh;
  };

  struct Material {
    core::rendering::Material::Handle material;
  };
} // namespace keptech::components
