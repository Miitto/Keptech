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

  inline bool isDepthFormat(TextureFormat format) {
    switch (format) {
    case TextureFormat::Depth16:
    case TextureFormat::Depth24Stencil8:
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
