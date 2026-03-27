#pragma once

#include "keptech/rendering/texture.hpp"
#include "keptech/vulkan/structs.hpp"

namespace kt::vkh {
  class Texture final : public kt::IImage {
  public:
    AllocatedImage& getImage() { return image; }

    Texture(VmaAllocator& allocator, VkDevice& device, AllocatedImage image,
            glm::uvec3 size, TextureFormat format, Bitflag<TextureUsage> usage,
            uint32_t mipLevels)
        : IImage(size, format, mipLevels), allocator(allocator), device(device),
          image(image) {}

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
        vmaDestroyImage(allocator, image.image, image.alloc);
        allocator = nullptr;
      }
    }

#ifdef KT_ADD_RESOURCE_INFO
    Texture(VmaAllocator& allocator, VkDevice& device, AllocatedImage image,
            glm::uvec3 size, TextureFormat format, uint32_t mipLevels,
            std::string name, Bitflag<TextureUsage> usage)
        : IImage(std::move(name), size, format, usage, mipLevels),
          allocator(allocator), device(device), image(image) {}
#endif

  private:
    VmaAllocator allocator;
    VkDevice device;
    AllocatedImage image;
    VkSampler sampler;
  };

  class Sampler final : public kt::ISampler {
  public:
    Sampler(VkSampler&& sampler) : sampler(std::move(sampler)) {}

    [[nodiscard]] const VkSampler& getVkSampler() const { return sampler; }

  private:
    VkSampler sampler;
  };
} // namespace kt::vkh
