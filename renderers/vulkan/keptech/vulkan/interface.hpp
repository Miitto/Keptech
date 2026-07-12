#pragma once

#include <Volk/volk.h>

namespace kt {
  namespace vkh {
    class Renderer;
    class Buffer;
    class Image;
  } // namespace vkh

  namespace rendering {
    using Renderer = vkh::Renderer;
    using ImageFormat = VkFormat;
    using Buffer = vkh::Buffer;
    using Image = vkh::Image;
    using ImageUsageFlags = VkImageUsageFlags;
  } // namespace rendering
} // namespace kt
