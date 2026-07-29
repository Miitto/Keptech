#pragma once

#include <Volk/volk.h>

namespace kt::rdr {
  class Renderer;
  class Buffer;
  class Image;
  class RenderGraphBuilder;
  class RenderPassBuilder;
  class RenderPassInterface;
  class CommandBuffer;
  class RenderGraph;

  using ImageFormat = VkFormat;
  using ImageUsageFlags = VkImageUsageFlags;

} // namespace kt::rdr
