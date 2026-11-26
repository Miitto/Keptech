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

  const RhiBase::GPUInfo& RhiBase::getGPUInfo(this const RHI& self) { return self.gpuInfo; }

  size_t RhiBase::getByteSize(this const RHI&, ImageFormat format, glm::uvec3 size, uint32_t mipLevels, uint32_t layers) {
    size_t pixels = static_cast<size_t>(size.x) * static_cast<size_t>(size.y) * static_cast<size_t>(size.z) *
                    static_cast<size_t>(mipLevels > 1 ? 2 : 1) * static_cast<size_t>(layers);
    switch (format) {
    case ImageFormat::R8_UNORM:
    case ImageFormat::BC5_UNORM:
    case ImageFormat::BC7_UNORM:
      return pixels;
    case ImageFormat::R8G8_UNORM:
    case ImageFormat::D16_UNORM:
    case ImageFormat::R16_FLOAT:
      return pixels * 2;
    case ImageFormat::R8G8B8_UNORM:
      return pixels * 3;
    case ImageFormat::R8G8B8A8_UNORM:
    case ImageFormat::R16G16_FLOAT:
    case ImageFormat::R32_FLOAT:
    case ImageFormat::R11G11B10_FLOAT:
    case ImageFormat::D32_FLOAT:
      return pixels * 4;
    case ImageFormat::R16G16B16A16_FLOAT:
    case ImageFormat::R32G32_FLOAT:
      return pixels * 8;
    case ImageFormat::BC3_UNORM:
    case ImageFormat::BC4_UNORM:
      return pixels / 2;
    case ImageFormat::R32G32B32_FLOAT:
      return pixels * 12;
    case ImageFormat::R32G32B32A32_FLOAT:
      return pixels * 16;
    }
  }
} // namespace kt::rhi