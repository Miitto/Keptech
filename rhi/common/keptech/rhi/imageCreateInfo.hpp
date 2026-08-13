#pragma once

#include "keptech/core/bitflag.hpp"
#include "keptech/rhi/imageFormat.hpp"
#include "keptech/rhi/imageUsage.hpp"
#include <glm/fwd.hpp>

namespace kt::rhi {
  enum class ImageDim : uint8_t {
    e1D,
    e2D,
    e3D,
  };

  class ImageCreateInfo {
  public:
    constexpr ImageCreateInfo(ImageDim imageDim, ImageFormat format, glm::uvec3 extent, Bitflag<ImageUsage> usage, uint32_t mipLevels = 1,
                              uint32_t arrayLayers = 1, const char* name = nullptr) noexcept
        : imageDim(imageDim), format(format), extent(extent), usage(usage), mipLevels(mipLevels), arrayLayers(arrayLayers), name(name) {}

    ImageDim getImageDim() const noexcept { return imageDim; }

    ImageFormat getFormat() const noexcept { return format; }

    glm::uvec3 getExtent() const noexcept { return extent; }

    uint32_t getWidth() const noexcept { return extent.x; }

    uint32_t getHeight() const noexcept { return extent.y; }

    uint32_t getDepth() const noexcept { return extent.z; }

    uint32_t getMipLevels() const noexcept { return mipLevels; }

    uint32_t getArrayLayers() const noexcept { return arrayLayers; }

    Bitflag<ImageUsage> getUsage() const noexcept { return usage; }

    [[nodiscard]]
    const char* getName() const noexcept {
      return name;
    }

  private:
    ImageDim imageDim;
    ImageFormat format;
    glm::uvec3 extent;
    Bitflag<ImageUsage> usage;
    uint32_t mipLevels;
    uint32_t arrayLayers;
    const char* name = nullptr;
  };
} // namespace kt::rhi