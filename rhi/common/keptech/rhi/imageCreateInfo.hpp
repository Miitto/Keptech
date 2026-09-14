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
    eCube,
  };

  class ImageCreateInfo {
  public:
    /// Creates an ImageCreateInfo object with the specified parameters.
    /// @param imageDim The dimension of the image (1D, 2D, 3D, or Cube).
    /// @param format The format of the image (e.g., R8G8B8A8_UNORM).
    /// @param extent The size of the image in pixels (width, height, depth).
    /// @param usage The intended usage of the image (e.g., RenderTarget, DepthStencil, Sampled).
    /// @param mipLevels The number of mipmap levels for the image. Set to 0 to automatically calculate.
    /// @param arrayLayers The number of array layers for the image. Must be at least 1.
    /// @param name An optional name for the image, useful for debugging and profiling.
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