#pragma once

#include "renderResources.hpp"
#include <Volk/volk.h>
#include <functional>
#include <glm/ext/vector_uint2.hpp>

namespace kt::vkh {
  class RenderGraphBuilder;
  class RenderGraph;
  class RenderPass;
  class CommandBuffer;
  class RenderPass;
  class RenderPassBuilder;
  class Renderer;

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
  class RenderPassInterface {
  public:
    RenderPassInterface() = default;
    RenderPassInterface(const RenderPassInterface&) = default;
    RenderPassInterface(RenderPassInterface&&) = default;
    RenderPassInterface& operator=(const RenderPassInterface&) = default;
    RenderPassInterface& operator=(RenderPassInterface&&) = default;
    virtual ~RenderPassInterface() = default;

    [[nodiscard]] virtual bool needRenderPass() const { return true; }
    [[nodiscard]] virtual bool getClearDepthStencil(VkClearDepthStencilValue* value) const {
      if (value)
        *value = {};
      return true;
    }
    [[nodiscard]] virtual bool getClearColor(size_t attachmentIndex, VkClearColorValue* value) const {
      if (value)
        *value = {};
      return true;
    }

    /// Called once before the render graph is baked. The renderer should not be used to create resources in this function, only used to
    /// query information about the device and queues. Create resources in `setup()` instead.
    virtual void setupDependencies(RenderPassBuilder& self, RenderGraphBuilder& graph, const Renderer& renderer) {}

    /// Called once after the render graph has been built.
    virtual void setup(Renderer& renderer, VkDescriptorSetLayout descriptorSetLayout) {}

    /// Called before the pass is executed. This is where you should update any resources that are used by the pass.
    virtual void prepare(RenderGraph& graph, Renderer& renderer) {}
    /// @brief Called when the pass is executed. This is where you should record the commands for the pass.
    /// @param cmd The command buffer to record commands to.
    /// @param descriptorSet The descriptor set for the pass. This is populated with the resources that were specified during setup.
    /// @param framebufferSize The size of the framebuffer for this pass. This is useful for setting the viewport and scissor.
    virtual void execute(const CommandBuffer& cmd, VkDescriptorSet descriptorSet, glm::uvec2 framebufferSize = {}) {}

    /// Called when the render graph is destroyed.
    virtual void shutdown(Renderer& renderer) {}

#if __clang__
#pragma clang diagnostic pop
#endif
  };

  struct AccessedResource {
    VkPipelineStageFlags2 stages = 0;
    VkAccessFlags2 access = 0;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
  };

  struct AccessedTextureResource : public AccessedResource {
    RenderTextureResource* texture = nullptr;
  };

  struct AccessedBufferResource : public AccessedResource {
    RenderBufferResource* buffer = nullptr;
  };

  class RenderPassBuilder {
  public:
    RenderPassBuilder(RenderGraphBuilder& graph, PassId id, Bitflag<QueueType> queue) : graph(graph), id(id), queue(queue) {}

    RenderTextureResource& setDepthStencilInput(const std::string& name);
    RenderTextureResource& setDepthStencilOutput(const std::string& name, const AttachmentInfo& info);
    RenderTextureResource& addColorOutput(const std::string& name, const AttachmentInfo& info, const std::string& input = "");
    RenderTextureResource& addResolveOutput(const std::string& name, const AttachmentInfo& info);
    RenderTextureResource& addAttachmentInput(const std::string& name);
    /// Add a texture that is used from last frame.
    RenderTextureResource& addHistoryInput(const std::string& name);

    RenderTextureResource& addTextureInput(const std::string& name, VkPipelineStageFlags2 stages = 0);
    RenderBufferResource& addUniformInput(const std::string& name, VkPipelineStageFlags2 stages = 0);
    RenderBufferResource& addStorageReadOnlyInput(const std::string& name, VkPipelineStageFlags2 stages = 0);

    RenderTextureResource& addStorageImageOutput(const std::string& name, const AttachmentInfo& info, const std::string& input = "");

    RenderBufferResource& addStorageOutput(const std::string& name, const BufferInfo& info, const std::string& input = "");
    RenderBufferResource& addTransferOutput(const std::string& name, const BufferInfo& info);

    RenderBufferResource& addVertexBufferInput(const std::string& name);
    RenderBufferResource& addIndexBufferInput(const std::string& name);
    RenderBufferResource& addIndirectBufferInput(const std::string& name);

    void setupDependencies(const Renderer& renderer) {
      if (interface)
        interface->setupDependencies(*this, graph, renderer);
    }

    RenderPassBuilder& setName(const std::string& n) {
      name = n;
      return *this;
    }
    [[nodiscard]] std::string& getName() { return name; }
    [[nodiscard]] const std::string& getName() const { return name; }

    [[nodiscard]] PassId getId() const { return id; }
    [[nodiscard]] QueueType getQueue() const { return queue; }

    [[nodiscard]] RenderPassInterface* getInterface() const { return interface; }
    RenderPassBuilder& setInterface(RenderPassInterface* i) {
      interface = i;
      return *this;
    }
    RenderPassBuilder& setBuildCallback(PassExecuteCb cb) {
      this->buildCb = std::move(cb);
      return *this;
    }
    [[nodiscard]] PassExecuteCb& getBuildCallback() { return buildCb; }
    RenderPassBuilder& setGetClearDepthStencilCallback(std::function<bool(VkClearDepthStencilValue*)> cb) {
      this->getClearDepthStencilCb = std::move(cb);
      return *this;
    }
    [[nodiscard]] std::function<bool(VkClearDepthStencilValue*)>& getGetClearDepthStencilCallback() { return getClearDepthStencilCb; }
    RenderPassBuilder& setGetClearColorCallback(std::function<bool(unsigned, VkClearColorValue*)> cb) {
      this->getClearColorCb = std::move(cb);
      return *this;
    }
    [[nodiscard]] std::function<bool(unsigned, VkClearColorValue*)>& getGetClearColorCallback() { return getClearColorCb; }

    bool getClearColor(uint32_t attachmentIndex, VkClearColorValue* value = nullptr) const {
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

    [[nodiscard]] const std::vector<RenderTextureResource*>& getColorOutputs() const { return colorOutputs; }
    [[nodiscard]] const std::vector<RenderTextureResource*>& getResolveOutputs() const { return resolveOutputs; }
    [[nodiscard]] const std::vector<RenderTextureResource*>& getColorInputs() const { return colorInputs; }
    [[nodiscard]] const std::vector<RenderTextureResource*>& getHistoryInputs() const { return historyInputs; }
    [[nodiscard]] const std::vector<RenderTextureResource*>& getAttachmentInputs() const { return attachmentInputs; }
    [[nodiscard]] const std::vector<RenderTextureResource*>& getStorageImageOutputs() const { return storageImageOutputs; }
    [[nodiscard]] const std::vector<RenderTextureResource*>& getStorageImageInputs() const { return storageImageInputs; }
    [[nodiscard]] const std::vector<RenderBufferResource*>& getStorageOutputs() const { return storageOutputs; }
    [[nodiscard]] const std::vector<RenderBufferResource*>& getStorageInputs() const { return storageInputs; }
    [[nodiscard]] const std::vector<RenderBufferResource*>& getTransferOutputs() const { return transferOutputs; }
    [[nodiscard]] RenderTextureResource* getDepthStencilInput() const { return depthStencilInput; }
    [[nodiscard]] RenderTextureResource* getDepthStencilOutput() const { return depthStencilOutput; }
    [[nodiscard]] const std::vector<AccessedTextureResource>& getGenericTextureInputs() const { return genericTexutre; }
    [[nodiscard]] const std::vector<AccessedBufferResource>& getGenericBufferInputs() const { return genericBuffers; }

    RenderPassBuilder& setIndex(size_t index) {
      this->index = index;
      return *this;
    }
    [[nodiscard]] size_t getIndex() const { return index; }

    void setDepthStencilLayout(VkImageLayout layout) { depthStencilLayout = layout; }
    [[nodiscard]] VkImageLayout getDepthStencilLayout() const { return depthStencilLayout; }

  private:
    RenderGraphBuilder& graph;
    PassId id;
    size_t index = ~0u;
    QueueType queue;
    std::string name;

    RenderPassInterface* interface = nullptr;
    PassExecuteCb buildCb = nullptr;
    std::function<bool(VkClearDepthStencilValue*)> getClearDepthStencilCb = nullptr;
    std::function<bool(unsigned, VkClearColorValue*)> getClearColorCb = nullptr;

    std::vector<RenderTextureResource*> colorOutputs;
    std::vector<RenderTextureResource*> resolveOutputs;
    std::vector<RenderTextureResource*> colorInputs;
    std::vector<RenderTextureResource*> historyInputs;
    std::vector<RenderTextureResource*> attachmentInputs;
    std::vector<RenderTextureResource*> storageImageOutputs;
    std::vector<RenderTextureResource*> storageImageInputs;
    std::vector<RenderBufferResource*> storageOutputs;
    std::vector<RenderBufferResource*> storageInputs;
    std::vector<RenderBufferResource*> transferOutputs;
    std::vector<AccessedTextureResource> genericTexutre;
    std::vector<AccessedBufferResource> genericBuffers;
    RenderTextureResource* depthStencilInput = nullptr;
    RenderTextureResource* depthStencilOutput = nullptr;

    VkImageLayout depthStencilLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    RenderBufferResource& addGenericBufferInput(const std::string& name, VkPipelineStageFlags2 stages, VkAccessFlags2 access,
                                                VkBufferUsageFlags usage);
  };
} // namespace kt::vkh