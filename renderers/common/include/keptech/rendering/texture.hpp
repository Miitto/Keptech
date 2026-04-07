#pragma once

#include "keptech/rendering/interface.hpp"
#include <cstdint>
#include <glm/glm.hpp>

namespace kt {
  class Texture {
  public:
    Texture(glm::ivec3 size, uint8_t mipLevels, rendering::ImageFormat format, uint32_t index)
        : size(size), mipLevels(mipLevels), format(format), index(index) {}

    [[nodiscard]] glm::ivec3 getSize() const { return size; }
    [[nodiscard]] uint8_t getMipLevels() const { return mipLevels; }
    [[nodiscard]] rendering::ImageFormat getFormat() const { return format; }
    [[nodiscard]] uint32_t getIndex() const { return index; }

  private:
    glm::ivec3 size;
    uint8_t mipLevels;
    rendering::ImageFormat format;
    uint32_t index;
  };
} // namespace kt
