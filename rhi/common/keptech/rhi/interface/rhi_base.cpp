#include "rhi_base.hpp"

#include "rhi.hpp"

namespace kt::rhi {
  uint8_t RhiBase::getLastFrameIndex(this const RHI& self) { return (self.m.frameIndex + MAX_FRAMES_IN_FLIGHT - 1) % MAX_FRAMES_IN_FLIGHT; }
  uint8_t RhiBase::getFrameIndex(this const RHI& self) { return self.m.frameIndex; }
  uint8_t RhiBase::getNextFrameIndex(this const RHI& self) { return (self.m.frameIndex + 1) % MAX_FRAMES_IN_FLIGHT; }
  RhiBase::Stats& RhiBase::getStats() { return stats; }

  void RhiBase::resetStats() {
#ifndef KT_DISABLE_STATS
    stats.drawCalls = 0;
    stats.dispatchCalls = 0;
    stats.pipelineSwitches = 0;
    stats.renderPasses = 0;
#endif
  }

} // namespace kt::rhi