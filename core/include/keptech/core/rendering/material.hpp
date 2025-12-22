#pragma once

#include "keptech/core/slotmap.hpp"

namespace keptech::core::rendering {
  struct Material {
    using Handle = SlotMapSmartHandle;

    enum class Stage : uint8_t { Deferred, Forward, Transparent };

    Stage stage;
  };
} // namespace keptech::core::rendering
