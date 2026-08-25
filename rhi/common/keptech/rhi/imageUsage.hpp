#pragma once

#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"

#ifdef KT_VULKAN
#include <Volk/volk.h>
#define F(vk, dx12) vk // NOLINT
#else
#include <d3d12.h>
#define F(vk, dx12) dx12 // NOLINT
#endif

namespace kt::rhi {

#ifdef KT_VULKAN
  using RawImageUsage = VkImageUsageFlags;
#else
  using RawImageUsage = D3D12_RESOURCE_FLAGS;
#endif

  // DX12 does not have most of the usage flags, so we use custom bits for when we need to check the usage anyway. `raw` will mask them out.
  enum class ImageUsage { // NOLINT
    None = 0,
    RenderTarget = F(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
    DepthStencil = F(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
    Sampled = F(VK_IMAGE_USAGE_SAMPLED_BIT, BIT(16)),
    Storage = F(VK_IMAGE_USAGE_STORAGE_BIT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
    TransferSrc = F(VK_IMAGE_USAGE_TRANSFER_SRC_BIT, BIT(17)),
    TransferDst = F(VK_IMAGE_USAGE_TRANSFER_DST_BIT, BIT(18)),
  };

  RawImageUsage raw(ImageUsage usage);
} // namespace kt::rhi

#undef F

template <> struct fmt::formatter<kt::rhi::ImageUsage> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::rhi::ImageUsage& usage, fmt::format_context& ctx) const;
};

template <> struct fmt::formatter<kt::Bitflag<kt::rhi::ImageUsage>> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::Bitflag<kt::rhi::ImageUsage>& usage, fmt::format_context& ctx) const;
};

DEFINE_BITFLAG_ENUM_OPERATORS(kt::rhi::ImageUsage)