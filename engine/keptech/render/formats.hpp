#pragma once

#include "keptech/rhi/imageFormat.hpp"

namespace std {
  template <typename T, typename U> class expected;
}

namespace kt {
  struct RenderFormats {
    using ImageFormat = rhi::ImageFormat;
    /// Color format. Likely 8bit RGBA.
    ImageFormat albedo;
    /// Normal format. Likely high precison UNORM such as R11G11B10, but may be float.
    ImageFormat normal;
    /// Low res depth format. Likely 16bit UNORM.
    ImageFormat lowResDepth;
    /// High res depth format. Likely 32bit float.
    ImageFormat highResDepth;
    /// HDR. Likely high res float. Also used for lighting.
    ImageFormat hdr;
  };

  struct TextureFormats {
    using ImageFormat = rhi::ImageFormat;
    /// Color format. Likely BC7 or BC3.
    ImageFormat color;
    /// Normal format. Likely BC5 or BC7.
    ImageFormat normal;
    /// Metallic-Roughness format. Likely BC7 or BC3.
    ImageFormat metallicRoughness;
    /// AO format. Likely BC4 or BC7.
    ImageFormat ao;
  };

  struct Formats {
    /// @brief Formats the engine will use for its internal render targets.
    RenderFormats render;
    /// @brief Formats the engine will use for its textures.
    TextureFormats texture;
    /// @brief Format of the swapchain images.
    rhi::ImageFormat swapchain;

    static const Formats& get();
    static std::expected<void, std::string> init();

  private:
    static Formats singleton;
  };
} // namespace kt