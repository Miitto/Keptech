#pragma once
//! Utils to reduce the boilerplate code for common image layout transitions. Include "transitions_impl.hpp" to use the functions.

#include "keptech/rhi/interface/image.hpp"
#include <Volk/volk.h>
#include <array>

namespace kt::rhi {

  class TransitionInfo {
  public:
    constexpr TransitionInfo(kt::ImageType imageType, kt::ImageLayout oldLayout, kt::ImageLayout newLayout, uint8_t mips = 1,
                             uint8_t layers = 1);

    constexpr VkImageMemoryBarrier2 image(VkImage image) const;

    constexpr TransitionInfo& queueTransfer(uint32_t srcQueueFamily, uint32_t dstQueueFamily);

  private:
    VkImageMemoryBarrier2 barrier;
  };

  constexpr VkImageMemoryBarrier2 layoutTransition(const VkImage image, const TransitionInfo transition);

  template <size_t N> constexpr void layoutTransitions(VkCommandBuffer cmdBuf, const std::array<VkImageMemoryBarrier2, N>& transitions);

  constexpr VkRenderingAttachmentInfo clearColorAttachment(const VkImageView view, float r = 0.f, float g = 0.f, float b = 0.f,
                                                           float a = 1.f);

  constexpr VkRenderingAttachmentInfo clearDepthAttachment(const VkImageView view, float depth, uint32_t stencil = 0);

  constexpr VkRenderingAttachmentInfo loadColorAttachment(const VkImageView view);

  constexpr VkRenderingAttachmentInfo loadDepthAttachment(const VkImageView view);

  template <size_t N>
  void beginRendering(VkCommandBuffer cmdBuf, VkRect2D renderArea, const std::array<VkRenderingAttachmentInfo, N>& colorAttachments,
                      uint32_t layerCount = 1);

  template <size_t N>
  void beginRendering(VkCommandBuffer cmdBuf, VkRect2D renderArea, const std::array<VkRenderingAttachmentInfo, N>& colorAttachments,
                      const VkRenderingAttachmentInfo& depthAttachment, uint32_t layerCount = 1);

  template <size_t N>
  void beginRenderingDepthOnly(VkCommandBuffer cmdBuf, VkRect2D renderArea, const VkRenderingAttachmentInfo& depthAttachment,
                               uint32_t layerCount = 1);
} // namespace kt::rhi