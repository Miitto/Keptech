#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#ifdef KEPTECH_RENDERER_VULKAN
#include <vulkan/vulkan.h>
#endif

namespace kt {

#ifdef KEPTECH_RENDERER_VULKAN
  using ImageFormat = VkFormat;
#endif

  class Texture {
  public:
    Texture(glm::ivec3 size, uint8_t mipLevels, uint8_t components, uint8_t bytesPerComponent, ImageFormat format, uint32_t index)
        : size(size), mipLevels(mipLevels), components(components), bytesPerComponent(bytesPerComponent), format(format), index(index) {}

    glm::ivec3 getSize() const { return size; }
    uint8_t getMipLevels() const { return mipLevels; }
    uint8_t getComponents() const { return components; }
    uint8_t getBytesPerComponent() const { return bytesPerComponent; }
    ImageFormat getFormat() const { return format; }
    uint32_t getIndex() const { return index; }

  private:
    glm::ivec3 size;
    uint8_t mipLevels;
    uint8_t components;
    uint8_t bytesPerComponent;
    ImageFormat format;
    uint32_t index;
  };
} // namespace kt
