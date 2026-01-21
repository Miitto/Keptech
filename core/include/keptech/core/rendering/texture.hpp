#pragma once

#include <keptech/core/bitflag.hpp>
#include <keptech/core/macros.hpp>
#include <keptech/core/slotmap.hpp>

namespace keptech::core::rendering {
  namespace _internal {
    struct TextureHandleDifferentiator {};
  } // namespace _internal

  using TextureHandle =
      core::SlotMapHandle<_internal::TextureHandleDifferentiator>;

  class Texture {
  public:
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
  };
} // namespace keptech::core::rendering

DEFINE_BITFLAG_ENUM_OPERATORS(keptech::core::rendering::Texture::Usage)
