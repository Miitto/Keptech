#pragma once

#include "keptech/render/renderGraph/types.hpp"
#include <Volk/volk.h>

namespace kt {
  namespace rdr {
    class Renderer;
    class Buffer;
    class Image;
    class RenderGraphBuilder;
    class RenderPassBuilder;
    class RenderPassInterface;
    class CommandBuffer;
    class RenderGraph;
  } // namespace rdr

  using AttachmentSize = rdr::AttachmentSize;
  using QueueType = rdr::QueueType;
  using RenderGraphBuilder = rdr::RenderGraphBuilder;
  using RenderPassBuilder = rdr::RenderPassBuilder;
  using RenderPassInterface = rdr::RenderPassInterface;
  using Renderer = rdr::Renderer;
  using ImageFormat = VkFormat;
  using Buffer = rdr::Buffer;
  using Image = rdr::Image;
  using ImageUsageFlags = VkImageUsageFlags;
  using CommandBuffer = rdr::CommandBuffer;
  using RenderGraph = rdr::RenderGraph;
} // namespace kt
