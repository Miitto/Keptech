#pragma once

#include "keptech/core/rendering/material.hpp"
#include "keptech/core/rendering/mesh.hpp"

namespace keptech::components {
  struct RenderObject {
    // Handle type is independant of the template arg
    core::rendering::Mesh::Handle mesh;
    core::rendering::Material::Handle material;
  };
} // namespace keptech::components
