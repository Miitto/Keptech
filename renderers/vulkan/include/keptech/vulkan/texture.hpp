#pragma once

#include "keptech/core/rendering/texture.hpp"
#include "keptech/vulkan/structs.hpp"

namespace keptech::vkh {
  struct Texture : public keptech::core::rendering::Texture {
    AllocatedImage image;
  };
} // namespace keptech::vkh
