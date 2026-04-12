#pragma once

#include "keptech/rendering/interface.hpp"
#include <cstdint>
#include <glm/glm.hpp>

namespace kt {
  class Texture {
  public:
    enum class Type : uint8_t { e2D, e3D, eCube };

    Texture() = default;
    Texture(Type type, glm::ivec3 size, uint8_t mipLevels, rendering::ImageFormat format, uint32_t index, rendering::Image image)
        : size(size), mipLevels(mipLevels), type(type), format(format), index(index), image(image) {}

    [[nodiscard]] glm::ivec3 getSize() const { return size; }
    [[nodiscard]] uint8_t getMipLevels() const { return mipLevels; }
    [[nodiscard]] rendering::ImageFormat getFormat() const { return format; }
    [[nodiscard]] uint32_t getIndex() const { return index; }
    [[nodiscard]] const rendering::Image& getImage() const { return image; }

  private:
    glm::ivec3 size;
    uint8_t mipLevels;
    Type type;
    rendering::ImageFormat format;
    uint32_t index = ~0u;
    rendering::Image image;
  };
} // namespace kt
