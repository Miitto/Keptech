#pragma once

#include "keptech/rendering/interface.hpp"
#include <cstdint>
#include <glm/glm.hpp>

namespace kt {
  class Texture {
  public:
    enum class Type : uint8_t { e2D, e3D, eCube };

    Texture() = default;
    Texture(Type type, rendering::Image image) : type(type), image(std::move(image)) {}

    [[nodiscard]] glm::ivec3 extent() const { return {image.extent().width, image.extent().height, image.extent().depth}; }
    [[nodiscard]] const rendering::Image& getImage() const { return image; }
    rendering::Image& operator*() { return image; }
    rendering::Image* operator->() { return &image; }
    [[nodiscard]] Type getType() const { return type; }

  private:
    Type type = Type::e2D;
    rendering::Image image;
  };
} // namespace kt
