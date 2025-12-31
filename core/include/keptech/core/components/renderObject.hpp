#pragma once

#include "keptech/core/slotmap.hpp"

namespace keptech::components {
  struct RenderObject {
    // Handle type is independant of the template arg
    core::SlotMapSmartHandle mesh;
    core::SlotMapSmartHandle material;
  };
} // namespace keptech::components
