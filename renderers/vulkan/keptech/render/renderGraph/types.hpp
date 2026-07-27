#pragma once

#include "glm/ext/vector_uint3.hpp"
#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"
#include "keptech/render/constants.hpp"
#include "keptech/render/wrappers/buffer.hpp"
#include "keptech/render/wrappers/image.hpp"
#include <Volk/volk.h>
#include <array>
#include <cstdint>
#include <glm/ext/vector_float3.hpp>
#include <spdlog/fmt/bundled/format.h>
#include <string>
#include <vma/vk_mem_alloc.h>

namespace kt::vkh {
  class CommandBuffer;
  using PassExecuteCb = std::function<void(const CommandBuffer&, VkDescriptorSet, glm::uvec3)>;

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
    [[nodiscard]] constexpr bool unused() const { return id == ~0u; }

  private:
    size_t id = ~0u;
  };

  enum class QueueType : uint8_t {
    Graphics = BIT(0),
    Compute = BIT(1),
    AsyncCompute = BIT(2),
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
    AttachmentSize sizeType = AttachmentSize::ResolutionRelative;
    glm::vec3 ratioSize{1.f, 1.f, 1.f};
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

  enum class QueueHandoff : uint8_t {
    No,
    ToCompute,
    FromCompute,
  };

  struct ImageBarrier {
    PhysResourceId resourceId;
    VkPipelineStageFlags2 srcStages = 0;
    VkPipelineStageFlags2 dstStages = 0;
    VkAccessFlags2 srcAccess = 0;
    VkAccessFlags2 dstAccess = 0;
    VkImageLayout oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout newLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    QueueHandoff handoff = QueueHandoff::No;
  };

  struct BufferBarrier {
    PhysResourceId resourceId;
    VkPipelineStageFlags2 srcStages = 0;
    VkPipelineStageFlags2 dstStages = 0;
    VkAccessFlags2 srcAccess = 0;
    VkAccessFlags2 dstAccess = 0;
    QueueHandoff handoff = QueueHandoff::No;
  };

  struct Barriers {
    std::vector<ImageBarrier> image;
    std::vector<BufferBarrier> buffer;
  };

  struct PrePostBarriers {
    Barriers pre;
    Barriers post;
    size_t needsWaitFor = ~0u;
  };

  struct PassGroup {
    QueueType queue = static_cast<QueueType>(0);
    size_t count = 0;
    /// The index of the pass group that this group should wait for before executing. If ~0, then no wait is needed.
    uint64_t waitFor = ~0ull;
  };

  struct RelativeImage {
    size_t index;
    glm::vec3 ratio;
  };

  struct Resources {
    std::vector<Image> images;
    std::vector<Buffer> buffers;
    std::unordered_map<std::string, size_t> nameToImage;
    std::unordered_map<std::string, size_t> nameToBuffer;
    std::vector<bool> physicalImageHasHistory;
    std::vector<RelativeImage> swapchainRelativeImages;
    std::vector<RelativeImage> resolutionRelativeImages;
  };

  struct Descriptors {
    VkDescriptorSetLayout layout = nullptr;
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> sets{};
  };
} // namespace kt::vkh

namespace std {
  template <> struct hash<kt::vkh::PassId> {
    size_t operator()(const kt::vkh::PassId& id) const { return std::hash<size_t>()(*id); }
  };
  template <> struct hash<kt::vkh::ResourceId> {
    size_t operator()(const kt::vkh::ResourceId& id) const { return std::hash<size_t>()(*id); }
  };
  template <> struct hash<kt::vkh::PhysResourceId> {
    size_t operator()(const kt::vkh::PhysResourceId& id) const { return std::hash<size_t>()(*id); }
  };
} // namespace std

template <> struct fmt::formatter<kt::vkh::AttachmentSize> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(const kt::vkh::AttachmentSize& size, FormatContext& ctx) const {
    std::string_view name;
    switch (size) {
    case kt::vkh::AttachmentSize::Absolute:
      name = "Absolute";
      break;
    case kt::vkh::AttachmentSize::SwapchainRelative:
      name = "SwapchainRelative";
      break;
    case kt::vkh::AttachmentSize::ResolutionRelative:
      name = "ResolutionRelative";
      break;
    }
    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};

template <> struct fmt::formatter<kt::vkh::QueueType> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(const kt::vkh::QueueType& queue, FormatContext& ctx) const {
    std::string_view name;
    switch (queue) {
    case kt::vkh::QueueType::Graphics:
      name = "Graphics";
      break;
    case kt::vkh::QueueType::Compute:
      name = "Compute";
      break;
    case kt::vkh::QueueType::AsyncCompute:
      name = "AsyncCompute";
      break;
    }
    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};

template <> struct fmt::formatter<kt::vkh::PassId> : fmt::formatter<size_t> {
  template <typename FormatContext> auto format(const kt::vkh::PassId& id, FormatContext& ctx) const {
    return fmt::formatter<size_t>::format(*id, ctx);
  }
};

template <> struct fmt::formatter<kt::vkh::ResourceId> : fmt::formatter<size_t> {
  template <typename FormatContext> auto format(const kt::vkh::ResourceId& id, FormatContext& ctx) const {
    return fmt::formatter<size_t>::format(*id, ctx);
  }
};

template <> struct fmt::formatter<kt::vkh::PhysResourceId> : fmt::formatter<size_t> {
  template <typename FormatContext> auto format(const kt::vkh::PhysResourceId& id, FormatContext& ctx) const {
    return fmt::formatter<size_t>::format(*id, ctx);
  }
};