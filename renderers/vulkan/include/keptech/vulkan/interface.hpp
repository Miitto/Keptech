#pragma once

#include "keptech/vulkan/structs.hpp"
#include <vulkan/vulkan.h>

namespace kt {
  namespace vkh {
    class Renderer;
    class AddressedAllocatedBuffer;
  } // namespace vkh

  namespace rendering {

    using Renderer = vkh::Renderer;
    using ImageFormat = VkFormat;
    using Buffer = vkh::AddressedAllocatedBuffer;
    using Image = vkh::AllocatedImage;
    using RendererMesh = vkh::RendererMesh;
  } // namespace rendering
} // namespace kt
