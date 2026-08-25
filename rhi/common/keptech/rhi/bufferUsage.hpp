#pragma once

#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"
#include <spdlog/fmt/bundled/format.h>

#ifdef KT_VULKAN
#include <Volk/volk.h>
#define F(vk, dx12) vk // NOLINT
#else
#include <d3d12.h>
#define F(vk, dx12) dx12 // NOLINT
#endif

namespace kt::rhi {

#ifdef KT_VULKAN
  using RawBufferUsage = VkBufferUsageFlags;
#else
  using RawBufferUsage = D3D12_RESOURCE_FLAGS;
#endif

  // DX12 does not have most of the usage flags, so we use custom bits for when we need to check the usage anyway. `raw` will mask them out.
  enum class BufferUsage : uint32_t { // NOLINT, size is set to uint32_t to match the size of VkBufferUsageFlags and D3D12_RESOURCE_FLAGS
    None = 0,
    Vertex = F(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, BIT(16)),
    Index = F(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, BIT(17)),
    Uniform = F(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, BIT(18)),
    Storage = F(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
    Indirect = F(VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, BIT(19)),
    TransferSrc = F(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, BIT(20)),
    TransferDst = F(VK_BUFFER_USAGE_TRANSFER_DST_BIT, BIT(21)),
  };

  RawBufferUsage raw(BufferUsage usage);
} // namespace kt::rhi

template <> struct fmt::formatter<kt::rhi::BufferUsage> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::rhi::BufferUsage& usage, fmt::format_context& ctx) const;
};

template <> struct fmt::formatter<kt::Bitflag<kt::rhi::BufferUsage>> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::Bitflag<kt::rhi::BufferUsage>& usage, fmt::format_context& ctx) const;
};

DEFINE_BITFLAG_ENUM_OPERATORS(kt::rhi::BufferUsage)