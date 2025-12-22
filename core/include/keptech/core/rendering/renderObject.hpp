#pragma once

#include "keptech/core/maths/transform.hpp"
#include "keptech/core/slotmap.hpp"

namespace keptech::core::rendering {
  struct RenderObject {
    // Handle type is independant of the template arg
    SlotMapSmartHandle mesh;
    SlotMapSmartHandle material;
  };
} // namespace keptech::core::rendering
