#pragma once

#include "keptech/renderGraph/graph.hpp"
#include "pass.hpp"
#include "renderResources.hpp"
#include <Volk/volk.h>
#include <glm/ext/vector_float2.hpp>
#include <vector>
#include <vma/vk_mem_alloc.h>

namespace kt::vkh {
  struct Members;

  class RenderGraph : public kt::render_graph::RenderGraph {
  public:
    RenderGraph(Members& m) : m(m) {}

    RenderPass& addPass(const std::string& name, Bitflag<QueueType> queueTypes = {});
    RenderPass* findPass(const std::string& name);

    void build();
    void setupAttachments();

    void execute();

    RenderTextureResource& getTextureResource(const std::string& name);
    RenderBufferResource& getBufferResource(const std::string& name);

    RenderGraph& setBackbufferSource(const std::string& name) {
      backbufferSource = name;
      return *this;
    }
    [[nodiscard]] const std::string& getBackbufferSource() const { return backbufferSource; }

    RenderGraph& setSwapchainSize(const glm::uvec2& size) {
      swapchainSize = size;
      return *this;
    }
    [[nodiscard]] const glm::uvec2& getSwapchainSize() const { return swapchainSize; }
    RenderGraph& setRenderResolution(const glm::uvec2& resolution) {
      renderResolution = resolution;
      return *this;
    }
    [[nodiscard]] const glm::uvec2& getRenderResolution() const { return renderResolution; }
    RenderGraph& setSwapchainFormat(VkFormat format) {
      swapchainFormat = format;
      return *this;
    }
    [[nodiscard]] VkFormat getSwapchainFormat() const { return swapchainFormat; }

  private:
    Members& m;
    glm::uvec2 renderResolution{0, 0};
    glm::uvec2 swapchainSize{0, 0};
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;

    std::vector<std::unique_ptr<RenderPass>> passes;
    std::vector<std::unique_ptr<RenderResource>> resources;
    std::unordered_map<std::string, PassId> passNameToId;
    std::unordered_map<std::string, ResourceId> resourceNameToId;

    std::string backbufferSource = "";

    std::vector<PassId> passStack;

    struct Barrier {
      PhysResourceId resourceId;
      VkImageLayout layout;
      VkAccessFlags2 access;
      VkPipelineStageFlags2 stages;
      bool history;
    };

    struct Barriers {
      /// Run before the pass starts executing.
      std::vector<Barrier> invalidate;
      /// Run after the pass finishes executing.
      std::vector<Barrier> flush;
    };

    std::vector<Barriers> passBarriers;

    std::vector<std::unordered_set<PassId>> passDependencies;

    std::vector<ResourceInfo> physicalResourceInfos;
    std::vector<VkImageView> physicalAttachments;
    std::vector<Buffer> physicalBuffers;
    std::vector<Image> physicalImageAttachments;
    std::vector<bool> physicalImageHasHistory;

    struct RenderPassInfo {

      std::vector<PhysResourceId> colorOutputs;
      PhysResourceId depthStencilOutput = PhysResourceId{};
      uint32_t clearAttachmentMask = 0;
      uint32_t loadAttachmentMask = 0;
      VkAttachmentLoadOp depthStencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      VkAttachmentStoreOp depthStencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    };

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
    /// Populate the render pass info for each pass.
    /// Calculates the necessary barriers and clear / load operations for each pass based on the registered resources and their usage.
    /// Also stores any necessary semaphore timeline values for inter-queue synchronization.
    void buildRenderPassInfo();

    /// Build the barriers for each pass based on the registered resources and their usage.
    void buildBarriers();

    ResourceInfo getResourceInfo(RenderTextureResource& resource) const;
    ResourceInfo getResourceInfo(RenderBufferResource& resource) const;
  };
} // namespace kt::vkh