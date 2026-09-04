#include "imageFormat.hpp"

namespace kt::rhi {
  RawImageFormat raw(ImageFormat format) { return static_cast<RawImageFormat>(format); }

  size_t size(ImageFormat format) {
    switch (format) {
    case ImageFormat::R8_UNORM:
      return 1;
    case ImageFormat::R8G8_UNORM:
      return 2;
    case ImageFormat::R8G8B8_UNORM:
      return 3;
    case ImageFormat::R8G8B8A8_UNORM:
      return 4;
    case ImageFormat::R16G16B16A16_FLOAT:
      return 8;
    case ImageFormat::R11G11B10_FLOAT:
      return 4;
    case ImageFormat::D16_UNORM:
      return 2;
    case ImageFormat::D32_FLOAT:
      return 4;
    case ImageFormat::BC3_UNORM:
      return 16;
    case ImageFormat::BC4_UNORM:
      return 8;
    case ImageFormat::BC5_UNORM:
    case ImageFormat::BC7_UNORM:
      return 16;
    case ImageFormat::R32_FLOAT:
      return 4;
    case ImageFormat::R32G32_FLOAT:
      return 8;
    case ImageFormat::R32G32B32_FLOAT:
      return 12;
    case ImageFormat::R32G32B32A32_FLOAT:
      return 16;
    }
  }
} // namespace kt::rhi

fmt::format_context::iterator fmt::formatter<kt::rhi::ImageFormat>::format(const kt::rhi::ImageFormat& format,
                                                                           fmt::format_context& ctx) const {
  std::string_view name;
  switch (format) {
  case kt::rhi::ImageFormat::R8G8B8A8_UNORM:
    name = "R8G8B8A8_UNORM";
    break;
  case kt::rhi::ImageFormat::R16G16B16A16_FLOAT:
    name = "R16G16B16A16_FLOAT";
    break;
  case kt::rhi::ImageFormat::R11G11B10_FLOAT:
    name = "R11G11B10_FLOAT";
    break;
  case kt::rhi::ImageFormat::D16_UNORM:
    name = "D16_UNORM";
    break;
  case kt::rhi::ImageFormat::D32_FLOAT:
    name = "D32_FLOAT";
    break;
  case kt::rhi::ImageFormat::BC4_UNORM:
    name = "BC4_UNORM";
    break;
  case kt::rhi::ImageFormat::BC5_UNORM:
    name = "BC5_UNORM";
    break;
  case kt::rhi::ImageFormat::BC7_UNORM:
    name = "BC7_UNORM";
    break;
  case kt::rhi::ImageFormat::Undefined:
    name = "Undefined";
    break;
  }
  return fmt::formatter<std::string_view>::format(name, ctx);
}
