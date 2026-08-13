#include "core.hpp"
#include "rhi.hpp"

namespace kt::rhi {
  void Pools::resetAll() {
    auto& device = RHI::get().getDevice();
    vkResetCommandPool(device, graphics.pool, 0);
    if (graphics.pool != compute.pool) {
      vkResetCommandPool(device, compute.pool, 0);
    }
  }
} // namespace kt::rhi