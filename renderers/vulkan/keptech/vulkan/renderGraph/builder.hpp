#pragma once

/** @file */

#include "keptech/vulkan/wrappers/buffer.hpp"
#include "keptech/vulkan/wrappers/image.hpp"
#include "pass.hpp"
#include "renderResources.hpp"
#include <Volk/volk.h>
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_uint2.hpp>
#include <vector>
#include <vma/vk_mem_alloc.h>

namespace kt::vkh {
  struct Members;

  class RenderGraphBuilder {
  public:
    RenderGraphBuilder() = default;

    RenderPass& addPass(const std::string& name, Bitflag<QueueType> queueTypes = {});
    RenderPass* findPass(const std::string& name);

    void build();
    /// Logs the current state of the render graph to the console. This includes the passes, resources, and their dependencies.
    /// Graph should be built before calling this function. Uses log level INFO.
    void log() const;
    void setupAttachments();

    void execute();

    RenderTextureResource& getTextureResource(const std::string& name);
    RenderBufferResource& getBufferResource(const std::string& name);

    RenderGraphBuilder& setBackbufferSource(const std::string& name) {
      backbufferSource = name;
      return *this;
    }
    [[nodiscard]] const std::string& getBackbufferSource() const { return backbufferSource; }

    RenderGraphBuilder& setSwapchainSize(const glm::uvec2& size) {
      swapchainSize = size;
      return *this;
    }
    [[nodiscard]] const glm::uvec2& getSwapchainSize() const { return swapchainSize; }
    RenderGraphBuilder& setRenderResolution(const glm::uvec2& resolution) {
      renderResolution = resolution;
      return *this;
    }
    [[nodiscard]] const glm::uvec2& getRenderResolution() const { return renderResolution; }
    RenderGraphBuilder& setSwapchainFormat(VkFormat format) {
      swapchainFormat = format;
      return *this;
    }
    [[nodiscard]] VkFormat getSwapchainFormat() const { return swapchainFormat; }

    /// Public for fmt formatter specialization
    /// Internal use only. Do not use directly.
    enum class QueueHandoff : uint8_t {
      No,
      ToCompute,
      FromCompute,
    };

  private:
    glm::uvec2 renderResolution{0, 0};
    glm::uvec2 swapchainSize{0, 0};
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;

    std::vector<std::unique_ptr<RenderPass>> passes;
    std::vector<std::unique_ptr<RenderResource>> resources;
    std::unordered_map<std::string, PassId> passNameToId;
    std::unordered_map<std::string, ResourceId> resourceNameToId;

    std::string backbufferSource = "";

    std::vector<PassId> passStack;

    struct Requirement {
      PhysResourceId resourceId;
      VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
      VkAccessFlags2 access = 0;
      VkPipelineStageFlags2 stages = 0;
      bool history = false;

      operator bool() const { return layout != VK_IMAGE_LAYOUT_UNDEFINED || access != 0 || stages != 0 || history; }
    };

    struct Requirements {
      /// Barriers for resources we read from
      std::vector<Requirement> invalidate;
      /// Barriers for resources we write to
      std::vector<Requirement> flush;
    };

    /// Defines the format and access stages/mask a render pass needs to access a resource with.
    std::vector<Requirements> passRequirements;

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
    std::vector<Barriers> passBarriers;

    std::vector<std::unordered_set<PassId>> passDependencies;

    std::vector<ResourceInfo> physicalResourceInfos;
    std::vector<std::unique_ptr<VkImageView>> physicalAttachments;
    std::vector<std::unique_ptr<Buffer>> physicalBuffers;
    std::vector<std::unique_ptr<Image>> physicalImageAttachments;
    std::vector<bool> physicalImageHasHistory;

    /// Validate the registered passes. Aborts if any pass has invalid inputs or outputs.
    void validatePasses() const;

    void traverseDependencies(const RenderPass& pass, size_t stackCount);
    void dependPassesRecursive(const RenderPass& self, const std::unordered_set<PassId>& writtenPasses, size_t stackCount, bool noCheck,
                               bool ignoreSelf, bool mergeDeps);

    /// Returns true if the pass with id `dst` depends on the pass with id `src`, false otherwise.
    [[nodiscard]] bool dependsOnPass(PassId dst, PassId src) const;

    /// Deduplicate passOrder. The order of the passes is preserved.
    void filterPasses(std::vector<PassId>& passOrder);
    /// Reorder the passes in `passOrder` based on their dependencies.
    /// After this function completes, the passes in `passOrder` can be run in the order they appear in the vector without violating any
    /// dependencies.
    void reorderPasses(std::vector<PassId>& passOrder);

    /// Create the actual resources described by the render graph.
    void buildPhysicalResources();

    /// Calculate what resources each pass needs to access and how they need to access them. This is used to build the barriers for each
    /// pass.
    void buildRequirements();

    /** Build the barriers for each pass based on the registered resources and their usage.
     *  @see buildRequirements()
     */
    void buildBarriers();

    ResourceInfo getResourceInfo(RenderTextureResource& resource) const;
    ResourceInfo getResourceInfo(RenderBufferResource& resource) const;
  };
} // namespace kt::vkh