#pragma once

#include "material.hpp"
#include "mesh.hpp"
#include <keptech/core/maths/transform.hpp>
#include <keptech/core/slotmap.hpp>

namespace keptech::vkh {
  struct RenderObject {
    core::SlotMap<Mesh>::Handle mesh;
    Material* material;
    maths::Transform* transform;
  };
} // namespace keptech::vkh
