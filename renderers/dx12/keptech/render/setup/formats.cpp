#include "renderer.hpp"
#include <dxgiformat.h>

namespace kt::rdr {

  namespace {
    constexpr std::array PREFERRED_ALBEDO_FORMATS = {
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_B8G8R8A8_UNORM,
    };

    constexpr std::array PREFERRED_DEPTH_FORMATS = {
        DXGI_FORMAT_D32_FLOAT,
        DXGI_FORMAT_D24_UNORM_S8_UINT,
    };

    constexpr std::array PREFERRED_NORMAL_FORMATS = {
        DXGI_FORMAT_R10G10B10A2_UNORM,
        DXGI_FORMAT_R11G11B10_FLOAT,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
    };

    constexpr std::array PREFERRED_METALLIC_ROUGHNESS_FORMATS = {
        DXGI_FORMAT_R8G8_UNORM,
        DXGI_FORMAT_R16G16_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM,
    };

    constexpr std::array PREFERRED_EMISSIVE_FORMATS = {
        DXGI_FORMAT_R11G11B10_FLOAT,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
    };

    constexpr std::array PREFERRED_HDR_FORMATS = {
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_R10G10B10A2_UNORM,
    };

    constexpr std::array PREFERRED_SHADOW_FORMATS = {
        // TODO: Shadow formats may want to differ from depth.
        DXGI_FORMAT_D32_FLOAT,
        DXGI_FORMAT_D24_UNORM_S8_UINT,
    };

    constexpr std::array PREFERRED_TEXTURE_FORMATS = {
        DXGI_FORMAT_BC7_UNORM_SRGB,
        DXGI_FORMAT_BC7_UNORM,
        DXGI_FORMAT_BC3_UNORM_SRGB,
        DXGI_FORMAT_BC3_UNORM,
    };

    constexpr std::array PREFERRED_NORMAL_TEXTURE_FORMATS = {
        DXGI_FORMAT_BC5_UNORM,
        DXGI_FORMAT_BC5_SNORM,
        DXGI_FORMAT_BC7_UNORM,
    };

    constexpr std::array PREFERRED_EMISSIVE_TEXTURE_FORMATS = {
        DXGI_FORMAT_BC7_UNORM_SRGB,
        DXGI_FORMAT_BC7_UNORM,
        DXGI_FORMAT_BC3_UNORM_SRGB,
        DXGI_FORMAT_BC3_UNORM,
    };
  } // namespace

  std::expected<void, std::string> Renderer::queryFormats() {
    auto getFirstFormat = [&](const std::span<const DXGI_FORMAT>& formats) -> DXGI_FORMAT {
      for (const auto& format : formats) {
        if (canRenderToFormat(format)) {
          return format;
        }
      }
      return DXGI_FORMAT_UNKNOWN;
    };

    auto getFirstTexFormat = [&](const std::span<const DXGI_FORMAT>& formats) -> DXGI_FORMAT {
      for (const auto& format : formats) {
        if (canSampleFromFormat(format)) {
          return format;
        }
      }
      return DXGI_FORMAT_UNKNOWN;
    };

#define CKH(var, fmt, name)                                                                                                                \
  var = getFirstFormat(fmt);                                                                                                               \
  if ((var) == DXGI_FORMAT_UNKNOWN) {                                                                                                      \
    return std::unexpected("No suitable " #name " format found");                                                                          \
  }

#define CKHT(var, fmt, name)                                                                                                               \
  var = getFirstTexFormat(fmt);                                                                                                            \
  if ((var) == DXGI_FORMAT_UNKNOWN) {                                                                                                      \
    return std::unexpected("No suitable " #name " texture format found");                                                                  \
  }

    CKH(m.formats.render.albedo, PREFERRED_ALBEDO_FORMATS, ALBEDO);
    CKH(m.formats.render.depth, PREFERRED_DEPTH_FORMATS, DEPTH);
    CKH(m.formats.render.normal, PREFERRED_NORMAL_FORMATS, NORMAL);
    CKH(m.formats.render.metRough, PREFERRED_METALLIC_ROUGHNESS_FORMATS, METALLIC_ROUGHNESS);
    CKH(m.formats.render.emissive, PREFERRED_EMISSIVE_FORMATS, EMISSIVE);
    CKH(m.formats.render.hdr, PREFERRED_HDR_FORMATS, HDR);
    CKHT(m.formats.texture.albedo, PREFERRED_TEXTURE_FORMATS, TEXTURE);
    CKHT(m.formats.texture.normal, PREFERRED_NORMAL_TEXTURE_FORMATS, TEXTURE);
    CKHT(m.formats.texture.emissive, PREFERRED_EMISSIVE_TEXTURE_FORMATS, TEXTURE);

    return {};
  }
} // namespace kt::rdr