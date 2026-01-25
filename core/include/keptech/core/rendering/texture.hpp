#pragma once

#include <glm/glm.hpp>
#include <keptech/core/bitflag.hpp>
#include <keptech/core/macros.hpp>
#include <keptech/core/slotmap.hpp>
#include <memory>
#include <spdlog/fmt/bundled/format.h>

namespace keptech {
  enum class TextureUsage : uint8_t {
    Sampled = BIT(0),
    RenderTarget = BIT(1),
    Storage = BIT(2),
    DepthStencil = BIT(3),
    TransferSrc = BIT(4),
    TransferDst = BIT(5),
  };
  enum class TextureFormat : uint8_t {
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

  class ITexture {
  public:
    virtual void writeData(const void* data, glm::uvec3 offset, glm::uvec3 size,
                           uint32_t mipLevel = 0) = 0;

    [[nodiscard]] glm::vec3 getSize() const { return size; }

    ITexture(const ITexture&) = default;
    ITexture(ITexture&&) = default;
    ITexture& operator=(const ITexture&) = default;
    ITexture& operator=(ITexture&&) = default;
    virtual ~ITexture() = default;

  private:
    glm::vec3 size{0, 0, 0};
  };

  using UTexPtr = std::unique_ptr<ITexture>;
  using STexPtr = std::shared_ptr<ITexture>;
} // namespace keptech

DEFINE_BITFLAG_ENUM_OPERATORS(keptech::TextureUsage)

template <>
struct fmt::formatter<keptech::TextureFormat>
    : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const keptech::TextureFormat format, FormatContext& ctx) const {
    using F = keptech::TextureFormat;
    std::string_view name = "";
    switch (format) {
    case F::Undefined:
      name = "Undefined";
      break;
    case F::Default:
      name = "Default";
      break;
    case F::R8:
      name = "R8";
      break;
    case F::R16F:
      name = "R16F";
      break;
    case F::R32F:
      name = "R32F";
      break;
    case F::RG8:
      name = "RG8";
      break;
    case F::RG16F:
      name = "RG16F";
      break;
    case F::RG32F:
      name = "RG32F";
      break;
    case F::RGB8:
      name = "RGB8";
      break;
    case F::RGB16F:
      name = "RGB16F";
      break;
    case F::RGB32F:
      name = "RGB32F";
      break;
    case F::RGBA8:
      name = "RGBA8";
      break;
    case F::RGBA16F:
      name = "RGBA16F";
      break;
    case F::RGBA32F:
      name = "RGBA32F";
      break;
    case F::Depth16:
      name = "Depth16";
      break;
    case F::Depth24Stencil8:
      name = "Depth24Stencil8";
      break;
    }
    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};
