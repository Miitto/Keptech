#pragma once

#include "glm/ext/vector_uint3.hpp"
#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"
#include "keptech/rhi/buffer.hpp"
#include "keptech/rhi/bufferTypes.hpp"
#include "keptech/rhi/bufferUsage.hpp"
#include "keptech/rhi/constants.hpp"
#include "keptech/rhi/descriptorLayout.hpp"
#include "keptech/rhi/descriptorSet.hpp"
#include "keptech/rhi/descriptorTypes.hpp"
#include "keptech/rhi/image.hpp"
#include "keptech/rhi/imageLayout.hpp"
#include <array>
#include <cstdint>
#include <glm/ext/vector_float3.hpp>
#include <spdlog/fmt/bundled/format.h>
#include <string>

namespace kt::rhi {
  struct ResourceSet;
  class CommandBuffer;
} // namespace kt::rhi

namespace kt {
  class RenderGraph;

  using PassExecuteCb = std::function<void(RenderGraph&, rhi::CommandBuffer&, const rhi::DescriptorSet&, glm::uvec2)>;

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

    constexpr PhysResourceId operator+(size_t offset) const { return PhysResourceId(id + offset); }

    constexpr PhysResourceId& operator+=(size_t offset) {
      id += offset;
      return *this;
    }

    [[nodiscard]] constexpr bool used() const { return id != ~0u; }
    [[nodiscard]] constexpr bool unused() const { return id == ~0u; }

  private:
    size_t id = ~0u;
  };

  enum class QueueType : uint8_t {
    /// Runs on the graphics queue to do graphics work. This is the default queue type for render passes.
    Graphics = BIT(0),
    /// Runs on the graphics queue to do synchronous compute work.
    Compute = BIT(1),
    /// Runs on the async compute queue to do asynchronous compute work. This is useful for compute passes that can run in parallel with
    /// graphics work.
    AsyncCompute = BIT(2),
    /// Only runs prepare, not execute. Useful for dedicated preparation passes that don't need to be executed on the GPU, such as uploading
    /// resources.
    Cpu = BIT(3),
  };
} // namespace kt

DEFINE_BITFLAG_ENUM_OPERATORS(kt::QueueType)

namespace kt {
  enum class AttachmentSize : uint8_t {
    Absolute,
    SwapchainRelative,
    ResolutionRelative,
  };

  struct AttachmentInfo {
    AttachmentSize sizeType = AttachmentSize::ResolutionRelative;
    glm::vec3 size = {1.f, 1.f, 1.f};
    rhi::ImageFormat format = rhi::ImageFormat::Undefined;
    uint32_t samples = 1;
    uint32_t mipLevels = 1;
    uint32_t layers = 1;
    bool persistent = true;
  };

  struct BufferInfo {
    size_t size = 0;
    size_t stride = 0;
    Bitflag<rhi::BufferUsage> usage = rhi::BufferUsage::None;
    rhi::BufferType type = rhi::BufferType::Default;
    bool persistent = true;

    bool isHostAccessible() const;
  };

  struct ResourceInfo {
    std::string name;
    rhi::ImageFormat format = rhi::ImageFormat::Undefined;
    BufferInfo bufferInfo{};
    AttachmentSize sizeType = AttachmentSize::ResolutionRelative;
    glm::vec3 ratioSize{1.f, 1.f, 1.f};
    glm::uvec3 size{0, 0, 1};
    uint32_t layers = 1;
    uint32_t mipLevels = 1;
    uint32_t samples = 1;
    bool persistent = true;
    Bitflag<QueueType> queues{};
    Bitflag<rhi::ImageUsage> imageUsage = rhi::ImageUsage::None;

    constexpr bool operator==(const ResourceInfo& other) const {
      return format == other.format && bufferInfo.size == other.bufferInfo.size &&
#ifdef KT_VULKAN
             bufferInfo.usage == other.bufferInfo.usage && bufferInfo.memoryUsage == other.bufferInfo.memoryUsage &&
             bufferInfo.mappingMode == other.bufferInfo.mappingMode &&
#endif
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

  struct ImageTransition {
    PhysResourceId resourceId;
    rhi::ImageLayout newLayout;
  };

  struct ImageBarrier {
    PhysResourceId resourceId;
#ifdef KT_VULKAN
    VkPipelineStageFlags2 srcStages = 0;
    VkPipelineStageFlags2 dstStages = 0;
    VkAccessFlags2 srcAccess = 0;
    VkAccessFlags2 dstAccess = 0;
#endif
    rhi::ImageLayout oldLayout = rhi::ImageLayout::Undefined;
    rhi::ImageLayout newLayout = rhi::ImageLayout::Undefined;
    QueueHandoff handoff = QueueHandoff::No;
    bool history = false;
  };

  struct BufferBarrier {
    PhysResourceId resourceId;
#ifdef KT_VULKAN
    VkPipelineStageFlags2 srcStages = 0;
    VkPipelineStageFlags2 dstStages = 0;
    VkAccessFlags2 srcAccess = 0;
    VkAccessFlags2 dstAccess = 0;
#endif
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

  struct UsedInPass {
    size_t passIndex = ~0u;
    uint32_t binding = ~0u;
    rhi::DescriptorType descriptorType;
    rhi::ImageLayout layout = rhi::ImageLayout::Undefined;
    bool history = false;
  };

  struct RelativeImage {
    size_t index;
    glm::vec3 ratio;
  };

  struct Resources {
    std::vector<rhi::Image> images;
    std::vector<rhi::Buffer> buffers;
    std::unordered_map<std::string, size_t> nameToImage;
    std::unordered_map<std::string, size_t> nameToBuffer;
    std::vector<bool> imageHasHistory;
    std::vector<RelativeImage> swapchainRelativeImages;
    std::vector<RelativeImage> resolutionRelativeImages;
    std::vector<std::vector<UsedInPass>> imageUsedInPass;
    std::vector<std::vector<UsedInPass>> bufferUsedInPass;
  };

  struct Descriptors {
    rhi::DescriptorLayout layout;
    std::array<rhi::DescriptorSet, MAX_FRAMES_IN_FLIGHT> sets;
  };
} // namespace kt

namespace std {
  template <> struct hash<kt::PassId> {
    size_t operator()(const kt::PassId& id) const { return std::hash<size_t>()(*id); }
  };
  template <> struct hash<kt::ResourceId> {
    size_t operator()(const kt::ResourceId& id) const { return std::hash<size_t>()(*id); }
  };
  template <> struct hash<kt::PhysResourceId> {
    size_t operator()(const kt::PhysResourceId& id) const { return std::hash<size_t>()(*id); }
  };
} // namespace std

template <> struct fmt::formatter<kt::AttachmentSize> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(const kt::AttachmentSize& size, FormatContext& ctx) const {
    std::string_view name;
    switch (size) {
    case kt::AttachmentSize::Absolute:
      name = "Absolute";
      break;
    case kt::AttachmentSize::SwapchainRelative:
      name = "SwapchainRelative";
      break;
    case kt::AttachmentSize::ResolutionRelative:
      name = "ResolutionRelative";
      break;
    }
    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};

template <> struct fmt::formatter<kt::QueueType> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(const kt::QueueType& queue, FormatContext& ctx) const {
    std::string_view name;
    switch (queue) {
    case kt::QueueType::Graphics:
      name = "Graphics";
      break;
    case kt::QueueType::Compute:
      name = "Compute";
      break;
    case kt::QueueType::AsyncCompute:
      name = "AsyncCompute";
      break;
    case kt::QueueType::Cpu:
      name = "Cpu";
      break;
    }
    return fmt::formatter<std::string_view>::format(name, ctx);
  }
};

template <> struct fmt::formatter<kt::PassId> : fmt::formatter<size_t> {
  template <typename FormatContext> auto format(const kt::PassId& id, FormatContext& ctx) const {
    return fmt::formatter<size_t>::format(*id, ctx);
  }
};

template <> struct fmt::formatter<kt::ResourceId> : fmt::formatter<size_t> {
  template <typename FormatContext> auto format(const kt::ResourceId& id, FormatContext& ctx) const {
    return fmt::formatter<size_t>::format(*id, ctx);
  }
};

template <> struct fmt::formatter<kt::PhysResourceId> : fmt::formatter<size_t> {
  template <typename FormatContext> auto format(const kt::PhysResourceId& id, FormatContext& ctx) const {
    return fmt::formatter<size_t>::format(*id, ctx);
  }
};