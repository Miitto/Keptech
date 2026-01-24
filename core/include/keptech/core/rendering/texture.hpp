#pragma once

#include <keptech/core/bitflag.hpp>
#include <keptech/core/macros.hpp>
#include <keptech/core/slotmap.hpp>
#include <spdlog/fmt/bundled/format.h>

namespace keptech::core::rendering {
  namespace _internal {
    struct TextureHandleDifferentiator {};
  } // namespace _internal

  using TextureHandle =
      core::SlotMapHandle<_internal::TextureHandleDifferentiator>;

  struct Texture {
    enum class Usage : uint8_t {
      Sampled = BIT(0),
      RenderTarget = BIT(1),
      Storage = BIT(2),
      DepthStencil = BIT(3),
      TransferSrc = BIT(4),
      TransferDst = BIT(5),
    };

    enum class Format : uint8_t {
      Undefined = 0,
      Default,
      R8,
      R16F,
      R32F,
      RG8,
      RG16F,
      RG32F,
      RGB8,
      RGB16F,
      RGB32F,
      RGBA8,
      RGBA16F,
      RGBA32F,
      Depth16,
      Depth24Stencil8,
    };

    std::string name;
    Format format;
  };
} // namespace keptech::core::rendering

DEFINE_BITFLAG_ENUM_OPERATORS(keptech::core::rendering::Texture::Usage)

template <>
struct fmt::formatter<keptech::core::rendering::Texture::Format>
    : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const keptech::core::rendering::Texture::Format format,
              FormatContext& ctx) const {
    using S = keptech::core::rendering::Texture::Format;
    std::string_view name = "";
    switch (format) {
    case keptech::core::rendering::Texture::Format::Undefined:
      name = "Undefined";
      break;
    case keptech::core::rendering::Texture::Format::Default:
      name = "Default";
      break;
    case keptech::core::rendering::Texture::Format::R8:
      name = "R8";
      break;
    case keptech::core::rendering::Texture::Format::R16F:
      name = "R16F";
      break;
    case keptech::core::rendering::Texture::Format::R32F:
      name = "R32F";
      break;
    case keptech::core::rendering::Texture::Format::RG8:
      name = "RG8";
      break;
    case keptech::core::rendering::Texture::Format::RG16F:
      name = "RG16F";
      break;
    case keptech::core::rendering::Texture::Format::RG32F:
      name = "RG32F";
      break;
    case keptech::core::rendering::Texture::Format::RGB8:
      name = "RGB8";
      break;
    case keptech::core::rendering::Texture::Format::RGB16F:
      name = "RGB16F";
      break;
    case keptech::core::rendering::Texture::Format::RGB32F:
      name = "RGB32F";
      break;
    case keptech::core::rendering::Texture::Format::RGBA8:
      name = "RGBA8";
      break;
    case keptech::core::rendering::Texture::Format::RGBA16F:
      name = "RGBA16F";
      break;
    case keptech::core::rendering::Texture::Format::RGBA32F:
      name = "RGBA32F";
      break;
    case keptech::core::rendering::Texture::Format::Depth16:
      name = "Depth16";
      break;
    case keptech::core::rendering::Texture::Format::Depth24Stencil8:
      name = "Depth24Stencil8";
      break;
    }
    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};
