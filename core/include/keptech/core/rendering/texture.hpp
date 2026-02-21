#pragma once

#include <glm/glm.hpp>
#include <imgui/imgui.h>
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
    R8UNorm,
    R8SNorm,
    R16UNorm,
    R16SNorm,
    RG8UNorm,
    RG8SNorm,
    RG16UNorm,
    RG16SNorm,
    RGB8UNorm,
    RGB8SNorm,
    RGB16UNorm,
    RGB16SNorm,
    RGBA8UNorm,
    RGBA8SNorm,
    RGBA16UNorm,
    R16F,
    RG16F,
    RGB16F,
    RGBA16F,
    R32F,
    RG32F,
    RGB32F,
    RGBA32F,
    Depth16,
    Depth24,
    Depth32F,
    Depth24Stencil8,
    Depth32FStencil8,
    Stencil8,
  };

  inline bool isDepthFormat(TextureFormat format) {
    switch (format) {
    case TextureFormat::Depth16:
    case TextureFormat::Depth24:
    case TextureFormat::Depth24Stencil8:
    case TextureFormat::Depth32F:
    case TextureFormat::Depth32FStencil8:
      return true;
    default:
      return false;
    }
  }

  inline bool isStencilFormat(TextureFormat format) {
    switch (format) {
    case TextureFormat::Stencil8:
    case TextureFormat::Depth24Stencil8:
    case TextureFormat::Depth32FStencil8:
      return true;
    default:
      return false;
    }
  }

  class IImage {
  public:
    IImage(glm::uvec3 size, TextureFormat format, uint32_t mipLevels)
        : size(size), format(format), mipLevels(mipLevels) {}

    [[nodiscard]] glm::vec3 getSize() const { return size; }
    [[nodiscard]] TextureFormat getFormat() const { return format; }
    [[nodiscard]] uint32_t getMipLevels() const { return mipLevels; }

    [[nodiscard]] uint32_t getIndex() const { return index; }
    void setIndex(uint32_t idx) { index = idx; }

    IImage(const IImage&) = default;
    IImage(IImage&&) = default;
    IImage& operator=(const IImage&) = default;
    IImage& operator=(IImage&&) = default;
    virtual ~IImage() = default;

    [[nodiscard]]

    std::optional<ImTextureRef>& getImGuiHandle() {
      return imguiHandle;
    }
    void setImGuiHandle(ImTextureRef handle) { imguiHandle = handle; }

#ifdef KT_ADD_RESOURCE_INFO
    void setDebugName(const std::string& name) { debugName = name; }
    [[nodiscard]] const std::string& getDebugName() const { return debugName; }
    [[nodiscard]] Bitflag<TextureUsage> getUsageFlags() const {
      return usageFlags;
    }

    IImage(std::string name, glm::uvec3 size, TextureFormat format,
           Bitflag<TextureUsage> usage, uint32_t mipLevels)
        : size(size), format(format), mipLevels(mipLevels),
          debugName(std::move(name)), usageFlags(usage) {}
#endif

  protected:
    glm::vec3 size{0, 0, 0};
    TextureFormat format{TextureFormat::Undefined};
    uint32_t mipLevels{1};

    uint32_t index = ~0u;

    std::optional<ImTextureRef> imguiHandle = std::nullopt;

#ifdef KT_ADD_RESOURCE_INFO
    std::string debugName{};
    Bitflag<TextureUsage> usageFlags{};
#endif
  };

  enum class TextureTransitionType : uint8_t {
    UndefinedToRenderable = 0,
    RenderableToShaderRead,
    ShaderReadToRenderable,
  };

  struct TextureTransition {
    TextureTransitionType type;
    IImage* texture;
  };

  using ImgPtr = std::shared_ptr<IImage>;

  enum class SamplerFilter : uint8_t { Nearest = 0, Linear };
  enum class SamplerAddressMode : uint8_t { Repeat = 0, Mirror, ClampToEdge };

  class ISampler {};

  using SamplerPtr = std::shared_ptr<ISampler>;

  class Texture {
  public:
    Texture(ImgPtr image, SamplerPtr sampler)
        : image(std::move(image)), sampler(std::move(sampler)) {}

    [[nodiscard]] const ImgPtr& getImage() const { return image; }
    [[nodiscard]] const SamplerPtr& getSampler() const { return sampler; }

    void setImage(ImgPtr newImage) { image = std::move(newImage); }
    void setSampler(SamplerPtr newSampler) { sampler = std::move(newSampler); }

    Texture(const Texture&) = default;
    Texture(Texture&&) = default;
    Texture& operator=(const Texture&) = default;
    Texture& operator=(Texture&&) = default;
    ~Texture() = default;

  protected:
    ImgPtr image;
    SamplerPtr sampler;
  };

  using TexPtr = std::shared_ptr<Texture>;

} // namespace keptech

DEFINE_BITFLAG_ENUM_OPERATORS(keptech::TextureUsage)

template <>
struct fmt::formatter<keptech::TextureFormat>
    : fmt::formatter<std::string_view> {
  template <typename FormatContext>
  auto format(const keptech::TextureFormat format, FormatContext& ctx) const {
    using F = keptech::TextureFormat;
    std::string_view name = "";
#define C(_NAME)                                                               \
  case F::_NAME:                                                               \
    name = #_NAME;                                                             \
    break

    switch (format) {
      C(Undefined);
      C(R8UNorm);
      C(R8SNorm);
      C(R16UNorm);
      C(R16SNorm);
      C(RG8UNorm);
      C(RG8SNorm);
      C(RG16UNorm);
      C(RG16SNorm);
      C(RGB8UNorm);
      C(RGB8SNorm);
      C(RGB16UNorm);
      C(RGB16SNorm);
      C(RGBA8UNorm);
      C(RGBA8SNorm);
      C(RGBA16UNorm);
      C(R16F);
      C(RG16F);
      C(RGB16F);
      C(RGBA16F);
      C(R32F);
      C(RG32F);
      C(RGB32F);
      C(RGBA32F);
      C(Depth16);
      C(Depth24);
      C(Depth32F);
      C(Depth24Stencil8);
      C(Depth32FStencil8);
      C(Stencil8);
    }
#undef C
    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};
