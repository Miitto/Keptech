#pragma once

#include <Volk/volk.h>

namespace kt::rhi {
  struct Queue {
    uint32_t index = ~0u;
    VkQueue queue = nullptr;
  };
} // namespace kt::rhi