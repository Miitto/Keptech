#include "imageLayout.hpp"

namespace kt::rhi {
  RawImageLayout raw(ImageLayout layout) { return static_cast<RawImageLayout>(static_cast<uint32_t>(layout) & 0xFFFF); }
} // namespace kt::rhi

fmt::format_context::iterator fmt::formatter<kt::rhi::ImageLayout>::format(const kt::rhi::ImageLayout& layout,
                                                                           fmt::format_context& ctx) const {
  std::string_view name;
  switch (layout) {
  case kt::rhi::ImageLayout::Undefined:
    name = "Undefined";
    break;
  case kt::rhi::ImageLayout::General:
    name = "General";
    break;
  case kt::rhi::ImageLayout::RenderTarget:
    name = "RenderTarget";
    break;
  case kt::rhi::ImageLayout::DepthStencilTarget:
    name = "DepthStencilTarget";
    break;
  case kt::rhi::ImageLayout::DepthStencilReadOnly:
    name = "DepthStencilReadOnly";
    break;
  case kt::rhi::ImageLayout::ShaderReadOnly:
    name = "ShaderReadOnly";
    break;
  case kt::rhi::ImageLayout::TransferSrc:
    name = "TransferSrc";
    break;
  case kt::rhi::ImageLayout::TransferDst:
    name = "TransferDst";
    break;
  }
  return fmt::formatter<std::string_view>::format(name, ctx);
}