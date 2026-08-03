#include "core.hpp"
#include "renderer.hpp"

namespace kt::rdr {
  void Pools::resetAll() {
    auto& device = Renderer::get().getDevice();
    vkResetCommandPool(device, graphics.pool, 0);
    if (graphics.pool != compute.pool) {
      vkResetCommandPool(device, compute.pool, 0);
    }
  }
} // namespace kt::rdr