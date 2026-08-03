#pragma once

#include "keptech/render/interface.hpp"

namespace kt::rdr {
  struct RenderFormats {
    ImageFormat albedo = KT_FORMAT_UNDEFINED;
    ImageFormat position = KT_FORMAT_UNDEFINED;
    ImageFormat normal = KT_FORMAT_UNDEFINED;
    ImageFormat emissive = KT_FORMAT_UNDEFINED;
    ImageFormat metRough = KT_FORMAT_UNDEFINED;
    ImageFormat depth = KT_FORMAT_UNDEFINED;
    ImageFormat hdr = KT_FORMAT_UNDEFINED;
  };

  struct TextureFormats {
    ImageFormat albedo = KT_FORMAT_UNDEFINED;
    ImageFormat normal = KT_FORMAT_UNDEFINED;
    ImageFormat metRough = KT_FORMAT_UNDEFINED;
    ImageFormat emissive = KT_FORMAT_UNDEFINED;
  };

  struct Formats {
    RenderFormats render{};
    TextureFormats texture{};
    ImageFormat swapchain = KT_FORMAT_UNDEFINED;
  };
} // namespace kt::rdr