#include "formats.hpp"

#include "keptech/rhi/rhi.hpp"
#include <expected>


namespace kt {
  Formats Formats::singleton{};
  const Formats& Formats::get() { return singleton; }

  std::expected<void, std::string> Formats::init() {
    auto& rhi = rhi::RHI::get();

    singleton.render.albedo = rhi::ImageFormat::R8G8B8A8_UNORM;
    singleton.render.normal = rhi::ImageFormat::R11G11B10_FLOAT;
    singleton.render.lowResDepth = rhi::ImageFormat::D16_UNORM;
    singleton.render.highResDepth = rhi::ImageFormat::D32_FLOAT;
    singleton.render.hdr = rhi::ImageFormat::R16G16B16A16_FLOAT;

    singleton.texture.color = rhi::ImageFormat::BC7_UNORM;
    singleton.texture.normal = rhi::ImageFormat::BC5_UNORM;
    singleton.texture.metallicRoughness = rhi::ImageFormat::BC7_UNORM;
    singleton.texture.ao = rhi::ImageFormat::BC4_UNORM;

    singleton.swapchain = rhi::ImageFormat::R8G8B8A8_UNORM;

    return {};
  }
} // namespace kt