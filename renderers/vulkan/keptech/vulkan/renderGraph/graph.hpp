#pragma once

#include "keptech/vulkan/wrappers/buffer.hpp"
#include "keptech/vulkan/wrappers/image.hpp"
#include "pass.hpp"
#include <glm/ext/vector_uint2.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace kt::vkh {
  class Renderer;
  class RenderGraphBuilder;
  class RenderGraph;

  enum class LoadOp : uint8_t {
    Load,
    Clear,
    DontCare,
  };

  enum class StoreOp : uint8_t {
    Store,
    DontCare,
  };

  struct RenderAttachment {
    PhysResourceId resourceId{};
    VkAttachmentLoadOp loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_DONT_CARE;

    operator bool() const { return resourceId.used(); }
  };

  class RenderPass {
  public:
    friend class RenderGraphBuilder;

    void setGraph(RenderGraph& graph) { this->graph = &graph; }
    void setInterface(RenderPassInterface* interface) { this->interface = interface; }
    void setBuildCallback(PassExecuteCb&& cb) { buildCb = std::move(cb); }
    void setGetClearDepthStencilCallback(std::function<bool(VkClearDepthStencilValue*)>&& cb) { getClearDepthStencilCb = std::move(cb); }
    void setGetClearColorCallback(std::function<bool(unsigned, VkClearColorValue*)>&& cb) { getClearColorCb = std::move(cb); }

    void setup(Renderer& renderer, VkDescriptorSetLayout descriptorSetLayout) {
      if (interface)
        interface->setup(renderer, descriptorSetLayout);
    }

    void prepare(RenderGraph& graph, Renderer& renderer) {
      if (interface)
        interface->prepare(graph, renderer);
    }

    void execute(const CommandBuffer& cmd, VkDescriptorSet descriptorSet, glm::uvec3 framebufferSize = {}) {
      if (interface) {
        interface->execute(cmd, descriptorSet, framebufferSize);
      } else if (buildCb) {
        buildCb(cmd, descriptorSet, framebufferSize);
      }
    }

    void shutdown(Renderer& renderer) {
      if (interface)
        interface->shutdown(renderer);
    }

    bool getClearColor(size_t attachmentIndex, VkClearColorValue* value = nullptr) const {
      if (interface)
        return interface->getClearColor(attachmentIndex, value);
      else if (getClearColorCb)
        return getClearColorCb(attachmentIndex, value);

      return false;
    }

    bool getClearDepthStencil(VkClearDepthStencilValue* value = nullptr) const {
      if (interface)
        return interface->getClearDepthStencil(value);
      else if (getClearDepthStencilCb)
        return getClearDepthStencilCb(value);

      return false;
    }

    [[nodiscard]] const PrePostBarriers& getBarriers() const { return barriers; }
    [[nodiscard]] const std::string& getName() const { return name; }
    [[nodiscard]] QueueType getQueue() const { return queue; }

    [[nodiscard]] const std::vector<RenderAttachment>& getColorAttachments() const { return colorAttachments; }
    [[nodiscard]] const RenderAttachment& getDepthStencilAttachment() const { return depthStencilAttachment; }

    void setExtentSourceId(PhysResourceId id) { extentSourceId = id; }
    [[nodiscard]] PhysResourceId getExtentSourceId() const { return extentSourceId; }

    void setDepthStencilLayout(VkImageLayout layout) { depthStencilLayout = layout; }
    [[nodiscard]] VkImageLayout getDepthStencilLayout() const { return depthStencilLayout; }

  private:
    RenderPass(std::string&& name, QueueType queue, PrePostBarriers&& barriers, std::vector<RenderAttachment>&& colorAttachments,
               RenderAttachment depthStencilAttachment)
        : name(std::move(name)), queue(queue), barriers(std::move(barriers)), colorAttachments(std::move(colorAttachments)),
          depthStencilAttachment(depthStencilAttachment) {}

    RenderGraph* graph = nullptr;
    std::string name;
    QueueType queue;

    PhysResourceId extentSourceId{};

    RenderPassInterface* interface = nullptr;
    PassExecuteCb buildCb = nullptr;
    std::function<bool(VkClearDepthStencilValue*)> getClearDepthStencilCb = nullptr;
    std::function<bool(unsigned, VkClearColorValue*)> getClearColorCb = nullptr;

    PrePostBarriers barriers;
    std::vector<RenderAttachment> colorAttachments;
    RenderAttachment depthStencilAttachment;
    VkImageLayout depthStencilLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  };

  class RenderGraph {
  public:
    // Friend so it is the only thing that can construct a RenderGraph.
    friend class RenderGraphBuilder;

    void execute();

    void setRenderer(Renderer& renderer) { this->renderer = &renderer; }

    [[nodiscard]] const std::vector<RenderPass>& getPasses() const { return passes; }
    [[nodiscard]] const std::vector<bool>& getPhysicalImageHasHistory() const { return resources.physicalImageHasHistory; }

    /// Get the index of the image resource with the given name. Throws if the resource does not exist.
    /// The returned index is safe to store as it will remain constant even if the image is resized. Use getImage() to get the image at the
    /// index.
    [[nodiscard]] size_t getImageIndex(const std::string& name) const {
      auto it = resources.nameToImage.find(name);
      VK_REQUIRE(it != resources.nameToImage.end(), "Image resource with name '{}' not found in render graph", name);
      return it->second;
    }

    /// Get the index of the buffer resource with the given name. Throws if the resource does not exist.
    /// The returned index is safe to store as it will remain constant even if the buffer is resized. Use getBuffer() to get the buffer at
    /// the index.
    [[nodiscard]] size_t getBufferIndex(const std::string& name) const {
      auto it = resources.nameToBuffer.find(name);
      VK_REQUIRE(it != resources.nameToBuffer.end(), "Buffer resource with name '{}' not found in render graph", name);
      return it->second;
    }

    /// Get the image at the given index. Do not store a reference to the image as it may become invalid if the image is resized. Use the
    /// index to get the image again if needed.
    [[nodiscard]] const Image& getImage(size_t index) const {
      VK_REQUIRE(index < resources.images.size(), "Image index {} is out of bounds (size: {})", index, resources.images.size());
      return resources.images[index];
    }

    /// Get the buffer at the given index. Do not store a reference to the buffer as it may become invalid if the buffer is resized. Use the
    /// index to get the buffer again if needed.
    [[nodiscard]] const Buffer& getBuffer(size_t index) const {
      VK_REQUIRE(index < resources.buffers.size(), "Buffer index {} is out of bounds (size: {})", index, resources.buffers.size());
      return resources.buffers[index];
    }

    void destroy();

    void setBackbufferSource(const std::string& name) {
      auto it = resources.nameToImage.find(name);
      VK_REQUIRE(it != resources.nameToImage.end(), "Backbuffer source '{}' not found in render graph", name);
      backbufferSourceIndex = it->second;
      VK_DEBUG("Backbuffer source set to '{}' (index {})", name, backbufferSourceIndex);
    }

    [[nodiscard]] const Image& getBackbufferImage() const {
      VK_REQUIRE(backbufferSourceIndex < resources.images.size(), "Backbuffer source index {} is out of bounds (size: {})",
                 backbufferSourceIndex, resources.images.size());
      return resources.images[backbufferSourceIndex];
    }

    void log() const;

  private:
    RenderGraph(Renderer& renderer, std::vector<PassGroup>&& passGroups, std::vector<RenderPass>&& passes, Resources&& resources,
                VkDescriptorPool descriptorPool, std::vector<Descriptors>&& descriptors)
        : renderer(&renderer), passGroups(std::move(passGroups)), passes(std::move(passes)), resources(std::move(resources)),
          descriptorPool(descriptorPool), passDescriptors(std::move(descriptors)) {
      for (const auto& group : this->passGroups) {
        if (group.queue == QueueType::Graphics) {
          graphicsQueuePassCount += group.count;
        } else if (group.queue == QueueType::AsyncCompute) {
          computeQueuePassCount += group.count;
        } // Shouldn't be any normal compute queue groups as they have been compacted into the graphics queue groups.
      }
    }

    Renderer* renderer;

    size_t graphicsQueuePassCount = 0;
    size_t computeQueuePassCount = 0;
    std::vector<PassGroup> passGroups;
    std::vector<RenderPass> passes;

    Resources resources;

    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    std::vector<Descriptors> passDescriptors;

    size_t backbufferSourceIndex = 0;

    void executeGraphicsPass(size_t passIdx, RenderPass& pass, CommandBuffer& cmd);
    void executeComputePass(size_t passIdx, RenderPass& pass, CommandBuffer& cmd);

    void pipelineBarrier(const Barriers& barriers, const CommandBuffer& cmd) const;
    void beginRendering(const RenderPass& pass, const CommandBuffer& cmd) const;
  };
} // namespace kt::vkh