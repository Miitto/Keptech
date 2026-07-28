#pragma once
//! Utils to reduce the boilerplate code for common image layout transitions.

#include "keptech/rendering/interface/image.hpp"
#include <Volk/volk.h>
#include <array>

namespace kt::rdr {

  namespace utils {
#include "transitions.inl"
  }

  struct TransitionInfo {
    VkImageMemoryBarrier2 barrier;

    constexpr TransitionInfo(kt::ImageType imageType, kt::ImageLayout oldLayout, kt::ImageLayout newLayout, uint8_t mips = 1,
                             uint8_t layers = 1)
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

    constexpr VkImageMemoryBarrier2 image(VkImage image) const {
      VkImageMemoryBarrier2 barrierCopy = barrier;
      barrierCopy.image = image;
      return barrierCopy;
    }

    constexpr TransitionInfo& queueTransfer(uint32_t srcQueueFamily, uint32_t dstQueueFamily) {
      barrier.srcQueueFamilyIndex = srcQueueFamily;
      barrier.dstQueueFamilyIndex = dstQueueFamily;
      return *this;
    }
  };

  constexpr VkImageMemoryBarrier2 layoutTransition(const VkImage image, const TransitionInfo transition) { return transition.image(image); }

  template <size_t N> constexpr void layoutTransitions(VkCommandBuffer cmdBuf, const std::array<VkImageMemoryBarrier2, N>& transitions) {
    VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = transitions.size(),
        .pImageMemoryBarriers = transitions.data(),
    };
    vkCmdPipelineBarrier2(cmdBuf, &dependencyInfo);
  }

  constexpr VkRenderingAttachmentInfo clearColorAttachment(const VkImageView view, float r = 0.f, float g = 0.f, float b = 0.f,
                                                           float a = 1.f) {
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

  constexpr VkRenderingAttachmentInfo clearDepthAttachment(const VkImageView view, float depth, uint32_t stencil = 0) {
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
                      uint32_t layerCount = 1) {
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
                      const VkRenderingAttachmentInfo& depthAttachment, uint32_t layerCount = 1) {
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
                               uint32_t layerCount = 1) {
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
} // namespace kt::rdr