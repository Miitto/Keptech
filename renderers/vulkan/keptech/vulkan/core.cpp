#include "core.hpp"

namespace kt::vkh {
  void Pools::resetAll(VkDevice device) {
    vkResetCommandPool(device, graphics.pool, 0);
    if (graphics.pool != compute.pool) {
      vkResetCommandPool(device, compute.pool, 0);
    }
  }
} // namespace kt::vkh