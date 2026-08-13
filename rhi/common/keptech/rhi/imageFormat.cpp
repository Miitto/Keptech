#include "imageFormat.hpp"

namespace kt::rhi {
  RawImageFormat raw(ImageFormat format) { return static_cast<RawImageFormat>(format); }
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
