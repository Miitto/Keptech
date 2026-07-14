#pragma once

#include "glm/ext/vector_uint3.hpp"
#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"
#include <Volk/volk.h>
#include <cstdint>
#include <glm/ext/vector_float3.hpp>
#include <string>
#include <vma/vk_mem_alloc.h>

namespace kt::vkh {
  class PassId {
  public:
    PassId() = default;
    explicit PassId(size_t id) : id(id) {}
    PassId(const PassId&) = default;
    PassId(PassId&&) = default;
    PassId& operator=(const PassId&) = default;
    PassId& operator=(PassId&&) = default;
    ~PassId() = default;

    operator bool() const { return id != ~0u; }
    bool operator==(const PassId& other) const { return id == other.id; }
    operator size_t() const { return id; }
    size_t operator*() const { return id; }

  private:
    size_t id = 0;
  };

  class ResourceId {
  public:
    constexpr ResourceId() = default;
    constexpr ResourceId(size_t id) : id(id) {}

    constexpr operator bool() const { return id != ~0u; }
    constexpr bool operator==(const ResourceId& other) const { return id == other.id; }
    constexpr operator size_t() const { return id; }
    constexpr size_t operator*() const { return id; }

    constexpr ResourceId& operator++() {
      ++id;
      return *this;
    }
    constexpr ResourceId operator++(int) {
      ResourceId tmp = *this;
      ++id;
      return tmp;
    }

    [[nodiscard]] constexpr bool used() const { return id != ~0u; }

  private:
    size_t id = ~0u;
  };

  class PhysResourceId {
  public:
    constexpr PhysResourceId() = default;
    constexpr PhysResourceId(size_t id) : id(id) {}

    constexpr operator bool() const { return id != ~0u; }
    constexpr bool operator==(const PhysResourceId& other) const { return id == other.id; }
    constexpr operator size_t() const { return id; }
    constexpr size_t operator*() const { return id; }

    constexpr PhysResourceId& operator++() {
      ++id;
      return *this;
    }
    constexpr PhysResourceId operator++(int) {
      PhysResourceId tmp = *this;
      ++id;
      return tmp;
    }

    [[nodiscard]] constexpr bool used() const { return id != ~0u; }

  private:
    size_t id = ~0u;
  };

  enum class QueueType : uint8_t {
    Graphics = BIT(0),
    Compute = BIT(1),
    AsyncCompute = BIT(2),
    Cpu = BIT(3),
  };
} // namespace kt::vkh

DEFINE_BITFLAG_ENUM_OPERATORS(kt::vkh::QueueType)

namespace kt::vkh {
  enum class AttachmentSize : uint8_t {
    Absolute,
    SwapchainRelative,
    ResolutionRelative,
  };

  struct AttachmentInfo {
    AttachmentSize sizeType = AttachmentSize::ResolutionRelative;
    glm::vec3 size = {1.f, 1.f, 1.f};
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t samples = 1;
    uint32_t mipLevels = 1;
    uint32_t layers = 1;
    bool persistent = true;
  };

  struct BufferInfo {
    VkDeviceSize size = 0;
    VkBufferUsageFlags usage = 0;
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO;
    VmaAllocationCreateFlags allocationFlags = 0;
    bool persistent = true;
  };

  struct ResourceInfo {
    std::string name;
    VkFormat format = VK_FORMAT_UNDEFINED;
    BufferInfo bufferInfo{};
    glm::uvec3 size{0, 0, 1};
    uint32_t layers = 1;
    uint32_t mipLevels = 1;
    uint32_t samples = 1;
    bool persistent = true;
    Bitflag<QueueType> queues{};
    VkImageUsageFlags imageUsage = 0;

    constexpr bool operator==(const ResourceInfo& other) const {
      return format == other.format && bufferInfo.size == other.bufferInfo.size && bufferInfo.usage == other.bufferInfo.usage &&
             bufferInfo.memoryUsage == other.bufferInfo.memoryUsage && bufferInfo.allocationFlags == other.bufferInfo.allocationFlags &&
             size == other.size && layers == other.layers && mipLevels == other.mipLevels && samples == other.samples &&
             persistent == other.persistent; // Queues and imageUsage are omitted.
    }

    constexpr bool operator!=(const ResourceInfo& other) const { return !(*this == other); }

    [[nodiscard]] bool needsSemaphores() const {
      // Uses Async Compute and Graphics/Compute queues, so we need to use semaphores to synchronize between the two queues.
      return (queues.has(QueueType::AsyncCompute) && queues.intersects(QueueType::Graphics | QueueType::Compute));
    }

    [[nodiscard]] bool isBufferLike() const { return bufferInfo.size > 0; }
    [[nodiscard]] bool isLayoutSensitive() const { return !isBufferLike(); }
  };
} // namespace kt::vkh
