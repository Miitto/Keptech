#pragma once

#include "keptech/graph/pass.hpp"
#include "keptech/graph/renderResources.hpp"
#include "keptech/rhi/constants.hpp"
#include "keptech/rhi/descriptorPool.hpp"
#include "keptech/rhi/descriptorSet.hpp"
#include <glm/ext/vector_float2.hpp>
#include <glm/ext/vector_uint2.hpp>
#include <memory>
#include <vector>

namespace kt {
  namespace rhi {
    class RHI;
    struct Members;
  } // namespace rhi

  class RenderGraph;

  class RenderGraphBuilder {
  public:
    RenderGraphBuilder() = default;

    /// @brief Adds a new render pass to the render graph.
    /// @param name The name of the render pass. Must be unique.
    /// @param queueType The queue type the render pass will be executed on.
    /// @param autoBeginRendering If true as QueueType::Graphics, the render pass will automatically call vkCmdBeginRendering and
    /// vkCmdEndRendering with the given color and depth attachments.
    RenderPassBuilder& addPass(const std::string& name, QueueType queueType = QueueType::Graphics, bool autoBeginRendering = true);
    RenderPassBuilder* findPass(const std::string& name);

    /// Analyses the registered passes and resources, and populates the internal data structures. This function must be called before
    /// build().
    void bake();
    /// @brief Constructs the render graph. bake() must have been called before this function. This function will return a RenderGraph
    /// object that can be used to execute the render passes in the correct order.
    /// @param renderer The renderer. Used to create the Vulkan resources.
    /// @warning This function will invalidate the RenderGraphBuilder object. Do not use it after calling this function.
    RenderGraph build();

    /// Logs the current state of the render graph to the console. This includes the passes, resources, and their dependencies.
    /// Graph should be built before calling this function. Uses log level DEBUG.
    void log() const;

    bool hasBufferResource(const std::string& name) const;
    bool hasTextureResource(const std::string& name) const;
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
    RenderGraphBuilder& setSwapchainFormat(rhi::ImageFormat format) {
      swapchainFormat = format;
      return *this;
    }
    [[nodiscard]] rhi::ImageFormat getSwapchainFormat() const { return swapchainFormat; }

  private:
    glm::uvec2 renderResolution{0, 0};
    glm::uvec2 swapchainSize{0, 0};
    rhi::ImageFormat swapchainFormat = rhi::ImageFormat::Undefined;

    std::vector<std::unique_ptr<RenderPassBuilder>> passes;
    std::vector<std::unique_ptr<RenderResource>> resources;
    std::unordered_map<std::string, PassId> passNameToId;
    std::unordered_map<std::string, ResourceId> resourceNameToId;

    std::string backbufferSource = "";

    std::vector<PassId> passStack;

    struct Requirement {
      PhysResourceId resourceId;
      rhi::ImageLayout layout = rhi::ImageLayout::Undefined;
#ifdef KT_VULKAN
      VkAccessFlags2 access = 0;
      VkPipelineStageFlags2 stages = 0;
#endif
      bool history = false;

      operator bool() const {
        return layout != rhi::ImageLayout::Undefined
#ifdef KT_VULKAN
               || access != 0 || stages != 0
#endif
               || history;
      }
    };

    struct Requirements {
      /// Barriers for resources we read from
      std::vector<Requirement> invalidate;
      /// Barriers for resources we write to
      std::vector<Requirement> flush;
    };

    /// Defines the format and access stages/mask a render pass needs to access a resource with.
    std::vector<Requirements> passRequirements;

    std::vector<PrePostBarriers> passBarriers;

    std::vector<std::unordered_set<PassId>> passDependencies;

    std::vector<ResourceInfo> physicalResourceInfos;
    std::vector<bool> physicalImageHasHistory;

    std::vector<ImageTransition> initialTransitions;

    /// Validate the registered passes. Aborts if any pass has invalid inputs or outputs.
    void validatePasses() const;

    void traverseDependencies(const RenderPassBuilder& pass, size_t stackCount);
    void dependPassesRecursive(const RenderPassBuilder& self, const std::unordered_set<PassId>& writtenPasses, size_t stackCount,
                               bool noCheck, bool ignoreSelf);

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

    Resources buildResources();

    rhi::DescriptorPool buildDescriptorPool(rhi::RHI& rhi);
    std::vector<std::array<rhi::DescriptorSet, MAX_FRAMES_IN_FLIGHT>> buildDescriptors(rhi::RHI& rhi, Resources& resources,
                                                                                       rhi::DescriptorPool& descriptorPool);

    std::vector<RenderPass> bakePasses(const Resources& resources);

    ResourceInfo getResourceInfo(RenderTextureResource& resource) const;
    ResourceInfo getResourceInfo(RenderBufferResource& resource) const;
  };
} // namespace kt