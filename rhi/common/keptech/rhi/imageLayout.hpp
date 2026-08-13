#pragma once

#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"

#ifdef KT_VULKAN
#include <Volk/volh.h>
#define F(vk, dx12) vk // NOLINT
#else
#include <d3d12.h>
#define F(vk, dx12) dx12 // NOLINT
#endif

namespace kt::rhi {
#ifdef KT_VULKAN
  using RawImageLayout = VkImageLayout;
#else
  using RawImageLayout = D3D12_RESOURCE_STATES;
#endif

  enum class ImageLayout { // NOLINT
    Undefined = F(VK_IMAGE_LAYOUT_UNDEFINED, BIT(16)),
    General = F(VK_IMAGE_LAYOUT_GENERAL, D3D12_RESOURCE_STATE_COMMON),
    RenderTarget = F(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, D3D12_RESOURCE_STATE_RENDER_TARGET),
    DepthStencilTarget = F(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, D3D12_RESOURCE_STATE_DEPTH_WRITE),
    DepthStencilReadOnly = F(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, D3D12_RESOURCE_STATE_DEPTH_READ),
    ShaderReadOnly = F(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                       D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE),
    TransferSrc = F(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, D3D12_RESOURCE_STATE_COPY_SOURCE),
    TransferDst = F(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, D3D12_RESOURCE_STATE_COPY_DEST),
    Present = F(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, D3D12_RESOURCE_STATE_PRESENT),
  };

  RawImageLayout raw(ImageLayout layout);
} // namespace kt::rhi

template <> struct fmt::formatter<kt::rhi::ImageLayout> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::rhi::ImageLayout& layout, fmt::format_context& ctx) const;
};

DEFINE_BITFLAG_ENUM_OPERATORS(kt::rhi::ImageLayout)