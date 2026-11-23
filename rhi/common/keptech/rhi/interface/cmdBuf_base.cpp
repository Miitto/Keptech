#include "cmdBuf_base.hpp"

#include "keptech/rhi/cmdBuf.hpp"

namespace kt::rhi {
  CommandBufferBase::ScopedEvent::~ScopedEvent() {
    if (cmd) {
      cmd->endEvent();
    }
  }

  CommandBufferBase::ScopedEvent CommandBufferBase::scopedEvent(this CommandBuffer& self, const std::string& name, glm::u8vec3 color) {
    self.beginEvent(name, color);
    return ScopedEvent(&self);
  }
} // namespace kt::rhi
