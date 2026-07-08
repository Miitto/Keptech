#pragma once

#include <Volk/volk.h>

namespace kt::vkh {
  struct Queue {
    uint32_t index = ~0;
    VkQueue queue = nullptr;
  };

  struct CommandPool {
    VkCommandPool pool = nullptr;
    Queue queue{};
  };
} // namespace kt::vkh