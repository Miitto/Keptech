#pragma once

#include "keptech/render/renderGraph/types.hpp"
#include <Volk/volk.h>

namespace kt {
  namespace vkh {
    class Renderer;
    class Buffer;
    class Image;
    class RenderGraphBuilder;
    class RenderPassBuilder;
    class RenderPassInterface;
    class CommandBuffer;
    class RenderGraph;
  } // namespace vkh

  using AttachmentSize = vkh::AttachmentSize;
  using QueueType = vkh::QueueType;
  using RenderGraphBuilder = vkh::RenderGraphBuilder;
  using RenderPassBuilder = vkh::RenderPassBuilder;
  using RenderPassInterface = vkh::RenderPassInterface;
  using Renderer = vkh::Renderer;
  using ImageFormat = VkFormat;
  using Buffer = vkh::Buffer;
  using Image = vkh::Image;
  using ImageUsageFlags = VkImageUsageFlags;
  using CommandBuffer = vkh::CommandBuffer;
  using RenderGraph = vkh::RenderGraph;
} // namespace kt
