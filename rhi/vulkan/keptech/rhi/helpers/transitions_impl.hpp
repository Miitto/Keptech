#pragma once

#include "keptech/rhi/helpers/transitions.hpp"

namespace kt::rhi {

  namespace utils {
    constexpr VkImageAspectFlags toVkImageAspectFlags(kt::ImageType t);
    constexpr VkImageLayout toVkImageLayout(kt::ImageType t, kt::ImageLayout layout);
    constexpr VkPipelineStageFlags2 toVkPipelineStageFlags(kt::ImageType imageType, kt::ImageLayout layout);
    constexpr VkAccessFlags2 toVkAccessFlags(kt::ImageType imageType, kt::ImageLayout layout);
  } // namespace utils

  constexpr TransitionInfo::TransitionInfo(kt::ImageType imageType, kt::ImageLayout oldLayout, kt::ImageLayout newLayout, uint8_t mips,
                                           uint8_t layers)
      : barrier(VkImageMemoryBarrier2{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                      .pNext = nullptr,
                                      .srcStageMask = utils::toVkPipelineStageFlags(imageType, oldLayout),
                                      .srcAccessMask = utils::toVkAccessFlags(imageType, oldLayout),
                                      .dstStageMask = utils::toVkPipelineStageFlags(imageType, newLayout),
                                      .dstAccessMask = utils::toVkAccessFlags(imageType, newLayout),
                                      .oldLayout = utils::toVkImageLayout(imageType, oldLayout),
                                      .newLayout = utils::toVkImageLayout(imageType, newLayout),
                                      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                      .image = nullptr,
                                      .subresourceRange = VkImageSubresourceRange{
                                          .aspectMask = utils::toVkImageAspectFlags(imageType),
                                          .baseMipLevel = 0,
                                          .levelCount = mips,
                                          .baseArrayLayer = 0,
                                          .layerCount = layers,
                                      }}) {}

  constexpr VkImageMemoryBarrier2 TransitionInfo::image(VkImage image) const {
    VkImageMemoryBarrier2 barrierCopy = barrier;
    barrierCopy.image = image;
    return barrierCopy;
  }

  constexpr TransitionInfo& TransitionInfo::queueTransfer(uint32_t srcQueueFamily, uint32_t dstQueueFamily) {
    barrier.srcQueueFamilyIndex = srcQueueFamily;
    barrier.dstQueueFamilyIndex = dstQueueFamily;
    return *this;
  }

  constexpr VkImageMemoryBarrier2 layoutTransition(const VkImage image, const TransitionInfo transition) { return transition.image(image); }

  template <size_t N> constexpr void layoutTransitions(VkCommandBuffer cmdBuf, const std::array<VkImageMemoryBarrier2, N>& transitions) {
    VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = transitions.size(),
        .pImageMemoryBarriers = transitions.data(),
    };
    vkCmdPipelineBarrier2(cmdBuf, &dependencyInfo);
  }

  constexpr VkRenderingAttachmentInfo clearColorAttachment(const VkImageView view, float r, float g, float b, float a) {
    return VkRenderingAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = view,
        .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode = VkResolveModeFlagBits::VK_RESOLVE_MODE_NONE,
        .resolveImageView = nullptr,
        .resolveImageLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue =
            VkClearValue{
                .color =
                    VkClearColorValue{
                        .float32 = {r, g, b, a},
                    },
            },
    };
  }

  constexpr VkRenderingAttachmentInfo clearDepthAttachment(const VkImageView view, float depth, uint32_t stencil) {
    return VkRenderingAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = view,
        .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .resolveMode = VkResolveModeFlagBits::VK_RESOLVE_MODE_NONE,
        .resolveImageView = nullptr,
        .resolveImageLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue =
            VkClearValue{
                .depthStencil =
                    {
                        .depth = depth,
                        .stencil = stencil,
                    },
            },
    };
  }
  constexpr VkRenderingAttachmentInfo loadColorAttachment(const VkImageView view) {
    return VkRenderingAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = view,
        .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveMode = VkResolveModeFlagBits::VK_RESOLVE_MODE_NONE,
        .resolveImageView = nullptr,
        .resolveImageLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = VkClearValue{},
    };
  }

  constexpr VkRenderingAttachmentInfo loadDepthAttachment(const VkImageView view) {
    return VkRenderingAttachmentInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = nullptr,
        .imageView = view,
        .imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .resolveMode = VkResolveModeFlagBits::VK_RESOLVE_MODE_NONE,
        .resolveImageView = nullptr,
        .resolveImageLayout = VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = VkClearValue{},
    };
  }

  template <size_t N>
  void beginRendering(VkCommandBuffer cmdBuf, VkRect2D renderArea, const std::array<VkRenderingAttachmentInfo, N>& colorAttachments,
                      uint32_t layerCount) {
    VkRenderingInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = renderArea,
        .layerCount = layerCount,
        .colorAttachmentCount = colorAttachments.size(),
        .pColorAttachments = colorAttachments.data(),
    };
    vkCmdBeginRendering(cmdBuf, &renderingInfo);
  }

  template <size_t N>
  void beginRendering(VkCommandBuffer cmdBuf, VkRect2D renderArea, const std::array<VkRenderingAttachmentInfo, N>& colorAttachments,
                      const VkRenderingAttachmentInfo& depthAttachment, uint32_t layerCount) {
    VkRenderingInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = renderArea,
        .layerCount = layerCount,
        .colorAttachmentCount = colorAttachments.size(),
        .pColorAttachments = colorAttachments.data(),
        .pDepthAttachment = &depthAttachment,
    };
    vkCmdBeginRendering(cmdBuf, &renderingInfo);
  }

  template <size_t N>
  void beginRenderingDepthOnly(VkCommandBuffer cmdBuf, VkRect2D renderArea, const VkRenderingAttachmentInfo& depthAttachment,
                               uint32_t layerCount) {
    VkRenderingInfo renderingInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .pNext = nullptr,
        .flags = 0,
        .renderArea = renderArea,
        .layerCount = layerCount,
        .viewMask = 0,
        .colorAttachmentCount = 0,
        .pColorAttachments = nullptr,
        .pDepthAttachment = &depthAttachment,
        .pStencilAttachment = nullptr,
    };
    vkCmdBeginRendering(cmdBuf, &renderingInfo);
  }

  namespace utils {

    constexpr VkImageAspectFlags toVkImageAspectFlags(kt::ImageType t) {
      switch (t) {
      case kt::ImageType::Color:
        return VK_IMAGE_ASPECT_COLOR_BIT;
      case kt::ImageType::Depth:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
      case kt::ImageType::Stencil:
        return VK_IMAGE_ASPECT_STENCIL_BIT;
      case kt::ImageType::DepthStencil:
        return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      default:
        return 0;
      }
    }

    constexpr VkImageLayout toVkImageLayout(kt::ImageType t, kt::ImageLayout layout) {
      switch (layout) {
      case kt::ImageLayout::Undefined:
        return VK_IMAGE_LAYOUT_UNDEFINED;
      case kt::ImageLayout::RenderTarget: {
        switch (t) {
        case kt::ImageType::Color:
          return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case kt::ImageType::Depth:
        case kt::ImageType::Stencil:
        case kt::ImageType::DepthStencil:
          return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
      }
      case kt::ImageLayout::ShaderReadOnly: {
        switch (t) {
        case kt::ImageType::Color:
          return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case kt::ImageType::Depth:
        case kt::ImageType::Stencil:
        case kt::ImageType::DepthStencil:
          return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        }
      }
      case kt::ImageLayout::TransferSrc:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      case kt::ImageLayout::TransferDst:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      case kt::ImageLayout::ComputeReadWrite:
        return VK_IMAGE_LAYOUT_GENERAL;
      case kt::ImageLayout::Present:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
      default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
      }
    }

    constexpr VkPipelineStageFlags2 toVkPipelineStageFlags(kt::ImageType imageType, kt::ImageLayout layout) {
      switch (layout) {
      case kt::ImageLayout::Undefined:
        return VK_PIPELINE_STAGE_2_NONE;
      case kt::ImageLayout::RenderTarget: {
        switch (imageType) {
        case kt::ImageType::Color:
          return VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        case kt::ImageType::Depth:
        case kt::ImageType::Stencil:
        case kt::ImageType::DepthStencil:
          return VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        }
      }
      case kt::ImageLayout::ShaderReadOnly:
        return VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
      case kt::ImageLayout::TransferSrc:
      case kt::ImageLayout::TransferDst:
        return VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      case kt::ImageLayout::ComputeReadWrite:
        return VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
      case kt::ImageLayout::Present:
        return VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
      default:
        return VK_PIPELINE_STAGE_2_NONE;
      }
    }

    constexpr VkAccessFlags2 toVkAccessFlags(kt::ImageType imageType, kt::ImageLayout layout) {
      switch (layout) {
      case kt::ImageLayout::Undefined:
        return 0;
      case kt::ImageLayout::RenderTarget: {
        switch (imageType) {
        case kt::ImageType::Color:
          return VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
        case kt::ImageType::Depth:
        case kt::ImageType::Stencil:
        case kt::ImageType::DepthStencil:
          return VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        }
      }
      case kt::ImageLayout::ShaderReadOnly:
        return VK_ACCESS_2_SHADER_READ_BIT;
      case kt::ImageLayout::TransferSrc:
        return VK_ACCESS_2_TRANSFER_READ_BIT;
      case kt::ImageLayout::TransferDst:
        return VK_ACCESS_2_TRANSFER_WRITE_BIT;
      case kt::ImageLayout::ComputeReadWrite:
        return VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
      default:
        return 0;
      }
    }
  } // namespace utils
} // namespace kt::rhi