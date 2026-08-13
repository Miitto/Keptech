#include "imageUsage.hpp"

#include <spdlog/fmt/bundled/ranges.h>

namespace kt::rhi {
  RawImageUsage raw(ImageUsage usage) { return static_cast<RawImageUsage>(static_cast<uint32_t>(usage) & 0xFFFF); }
} // namespace kt::rhi

fmt::format_context::iterator fmt::formatter<kt::rhi::ImageUsage>::format(const kt::rhi::ImageUsage& usage,
                                                                          fmt::format_context& ctx) const {
  std::string_view name;
  switch (usage) {
  case kt::rhi::ImageUsage::None:
    name = "None";
    break;
  case kt::rhi::ImageUsage::RenderTarget:
    name = "RenderTarget";
    break;
  case kt::rhi::ImageUsage::DepthStencil:
    name = "DepthStencil";
    break;
  case kt::rhi::ImageUsage::Sampled:
    name = "Sampled";
    break;
  case kt::rhi::ImageUsage::Storage:
    name = "Storage";
    break;
  case kt::rhi::ImageUsage::TransferSrc:
    name = "TransferSrc";
    break;
  case kt::rhi::ImageUsage::TransferDst:
    name = "TransferDst";
    break;
  }
  return fmt::formatter<std::string_view>::format(name, ctx);
}

fmt::format_context::iterator fmt::formatter<kt::Bitflag<kt::rhi::ImageUsage>>::format(const kt::Bitflag<kt::rhi::ImageUsage>& usage,
                                                                                       fmt::format_context& ctx) const {
  std::string_view name;
  if (usage == kt::rhi::ImageUsage::None) {
    name = "None";
    return fmt::formatter<std::string_view>::format(name, ctx);
  }

  using U = kt::rhi::ImageUsage;

  std::vector<std::string_view> names;
  if (usage.has(U::RenderTarget)) {
    names.push_back("RenderTarget");
  }
  if (usage.has(U::DepthStencil)) {
    names.push_back("DepthStencil");
  }
  if (usage.has(U::Sampled)) {
    names.push_back("Sampled");
  }
  if (usage.has(U::Storage)) {
    names.push_back("Storage");
  }
  if (usage.has(U::TransferSrc)) {
    names.push_back("TransferSrc");
  }
  if (usage.has(U::TransferDst)) {
    names.push_back("TransferDst");
  }

  return fmt::format_to(ctx.out(), "{}", fmt::join(names, " | "));
}