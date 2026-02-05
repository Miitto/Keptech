#pragma once

#include "keptech/core/rendering/texture.hpp"
#include "keptech/vulkan/structs.hpp"

namespace keptech::vkh {
  class Texture final : public keptech::IImage {
  public:
    AllocatedImage& getImage() { return image; }

    Texture(vma::Allocator& allocator, vk::raii::Device& device,
            AllocatedImage image, glm::uvec3 size, TextureFormat format,
            Bitflag<TextureUsage> usage, uint32_t mipLevels)
        : IImage(size, format, mipLevels), allocator(&allocator),
          device(&device), image(image) {}

    Texture(const Texture&) = delete;
    Texture(Texture&& o) noexcept
        : IImage(o.size, o.format, o.mipLevels), allocator(o.allocator),
          device(o.device), image(o.image) {
      o.allocator = nullptr;
#ifdef KT_ADD_RESOURCE_INFO
      debugName = std::move(o.debugName);
      usageFlags = o.usageFlags;
#endif
    }
    Texture& operator=(const Texture&) = delete;
    Texture& operator=(Texture&& o) noexcept {
      if (this != &o) {
        size = o.size;
        format = o.format;
        mipLevels = o.mipLevels;
        allocator = o.allocator;
        device = o.device;
        image = o.image;
        o.allocator = nullptr;
#ifdef KT_ADD_RESOURCE_INFO
        debugName = std::move(o.debugName);
        usageFlags = o.usageFlags;
#endif
      }
      return *this;
    }
    ~Texture() final {
      if (allocator) {
        image.destroy(*allocator, *device);
        allocator = nullptr;
      }
    }

#ifdef KT_ADD_RESOURCE_INFO
    Texture(vma::Allocator& allocator, vk::raii::Device& device,
            AllocatedImage image, glm::uvec3 size, TextureFormat format,
            uint32_t mipLevels, std::string name, Bitflag<TextureUsage> usage)
        : IImage(std::move(name), size, format, usage, mipLevels),
          allocator(&allocator), device(&device), image(image) {}
#endif

  private:
    vma::Allocator* allocator;
    vk::raii::Device* device;
    AllocatedImage image;
    std::shared_ptr<vk::raii::Sampler> sampler;
  };

  class Sampler final : public keptech::ISampler {
  public:
    Sampler(vk::raii::Sampler&& sampler) : sampler(std::move(sampler)) {}

    [[nodiscard]] const vk::raii::Sampler& getVkSampler() const {
      return sampler;
    }

  private:
    vk::raii::Sampler sampler;
  };
} // namespace keptech::vkh
