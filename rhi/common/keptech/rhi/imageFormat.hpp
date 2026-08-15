#pragma once

#ifdef KT_VULKAN
#include <Volk/volh.h>
#define F(vk, dx12) vk // NOLINT
#else
#include <dxgiformat.h>
#define F(vk, dx12) dx12 // NOLINT
#endif

namespace kt::rhi {

#ifdef KT_VULKAN
  using RawImageFormat = VkFormat;
#else
  using RawImageFormat = DXGI_FORMAT;
#endif

  enum class ImageFormat { // NOLINT
    R8_UNORM = F(VK_FORMAT_R8_UNORM, DXGI_FORMAT_R8_UNORM),
    R8G8_UNORM = F(VK_FORMAT_R8G8_UNORM, DXGI_FORMAT_R8G8_UNORM),
    R8G8B8A8_UNORM = F(VK_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM),
    R16G16B16A16_FLOAT = F(VK_FORMAT_R16G16B16A16_SFLOAT, DXGI_FORMAT_R16G16B16A16_FLOAT),
    R11G11B10_FLOAT = F(VK_FORMAT_B10G11R11_UFLOAT_PACK32, DXGI_FORMAT_R11G11B10_FLOAT),
    D16_UNORM = F(VK_FORMAT_D16_UNORM, DXGI_FORMAT_D16_UNORM),
    D32_FLOAT = F(VK_FORMAT_D32_SFLOAT, DXGI_FORMAT_D32_FLOAT),
    BC4_UNORM = F(VK_FORMAT_BC4_UNORM_BLOCK, DXGI_FORMAT_BC4_UNORM),
    BC5_UNORM = F(VK_FORMAT_BC5_UNORM_BLOCK, DXGI_FORMAT_BC5_UNORM),
    BC7_UNORM = F(VK_FORMAT_BC7_UNORM_BLOCK, DXGI_FORMAT_BC7_UNORM),

    Undefined = F(VK_FORMAT_UNDEFINED, DXGI_FORMAT_UNKNOWN),
  };

  RawImageFormat raw(ImageFormat format);
} // namespace kt::rhi

#undef F

template <> struct fmt::formatter<kt::rhi::ImageFormat> : fmt::formatter<std::string_view> {
  fmt::format_context::iterator format(const kt::rhi::ImageFormat& format, fmt::format_context& ctx) const;
};