#pragma once

#include "glm/fwd.hpp"
#include "keptech/core/image.hpp"
#include "keptech/core/scene.hpp"
#include "keptech/rendering/texture.hpp"
#include <array>
#include <concepts>
#include <expected>
#include <keptech/core/window.hpp>
#include <string>

namespace kt {
  struct RendererCreateInfo {
    const char* applicationName = "Keptech App";
  };

  struct ImageCreateInfo {
    std::string name;
    glm::uvec3 size;
    TextureFormat format;
    Bitflag<TextureUsage> usage;
    uint32_t mipLevels = 1;
    const void* data = nullptr;
  };

  struct ImageUploadInfo {
    std::string name;
    const Image* image;
    Bitflag<TextureUsage> usage;
    uint32_t mipLevels = 1;
  };

  struct SamplerCreateInfo {
    std::string name;
    SamplerFilter magFilter = SamplerFilter::Linear;
    SamplerFilter minFilter = SamplerFilter::Linear;
    SamplerAddressMode addressModeU = SamplerAddressMode::Repeat;
    SamplerAddressMode addressModeV = SamplerAddressMode::Repeat;
    SamplerAddressMode addressModeW = SamplerAddressMode::Repeat;
    bool enableAnisotropy = false;
    float anisotropyLevel = 1.f;
    SamplerFilter mipmapFilter = SamplerFilter::Linear;
  };

  template <typename T>
  concept CRenderer = requires(T a, const RendererCreateInfo& ci,
                               const core::window::Window& w, Scene& scene) {
    { T::create(ci, w) } -> std::same_as<std::expected<T, std::string>>;
    { a.newFrame() } -> std::same_as<void>;
    { a.setScene(scene) } -> std::same_as<void>;
    { a.render() } -> std::same_as<void>;
    { T::getName() } -> std::same_as<const char*>;
  };

  struct TextureFormats {
    TextureFormat albedo = TextureFormat::RGBA8UNorm;
    TextureFormat normal = TextureFormat::RGBA16F;
    TextureFormat emissiveAo = TextureFormat::RGBA8UNorm;
    TextureFormat metallicRoughness = TextureFormat::RG8UNorm;
    TextureFormat depth = TextureFormat::Depth32F;
    TextureFormat diffuse = TextureFormat::RGBA16F;
    TextureFormat specular = TextureFormat::RGBA16F;
    TextureFormat combined = TextureFormat::RGBA16F;
  };

  constexpr std::array PREFERRED_NORMAL_FORMATS{
      TextureFormat::RGB16F,
      TextureFormat::RGBA16F,
      TextureFormat::RGB32F,
      TextureFormat::RGBA32F,
  };

  constexpr std::array PREFERRED_METALLIC_ROUGHNESS_FORMATS = {
      TextureFormat::RG8UNorm,
      TextureFormat::RGB8UNorm,
      TextureFormat::RGBA8UNorm,
  };

  constexpr std::array PREFERRED_DEPTH_FORMATS{
      TextureFormat::Depth24,         TextureFormat::Depth32F,
      TextureFormat::Depth24Stencil8, TextureFormat::Depth32FStencil8,
      TextureFormat::Depth16,
  };

  constexpr std::array PREFERRED_LIGHT_FORMATS{
      TextureFormat::RGBA16F,
      TextureFormat::RGBA32F,
  };

} // namespace kt
