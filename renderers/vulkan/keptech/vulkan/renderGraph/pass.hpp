#pragma once

#include "renderResources.hpp"
#include <Volk/volk.h>
#include <functional>

namespace kt::vkh {
  class RenderGraphBuilder;
  class RenderGraph;
  class RenderPass;
  class CommandBuffer;
  class RenderPass;
  class RenderPassBuilder;
  class Renderer;

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

    /// Called once before the render graph is baked.
    virtual void setupDependencies(RenderPassBuilder& self, RenderGraphBuilder& graph) {}
    /// Called once after the render graph has been built.
    virtual void setup(const Renderer& renderer) {}

    /// Called before the pass is executed. This is where you should update any resources that are used by the pass.
    virtual void prepare(RenderGraph& graph) {}
    /// Called when the pass is executed. This is where you should record the commands for the pass.
    virtual void execute(const CommandBuffer& cmd) {}
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

    void setupDependencies() {
      if (interface)
        interface->setupDependencies(*this, graph);
    }

    RenderPassBuilder& setName(const std::string& name) {
      this->name = name;
      return *this;
    }
    [[nodiscard]] std::string& getName() { return name; }
    [[nodiscard]] const std::string& getName() const { return name; }

    [[nodiscard]] PassId getId() const { return id; }
    [[nodiscard]] QueueType getQueue() const { return queue; }

    [[nodiscard]] RenderPassInterface* getInterface() const { return interface; }
    RenderPassBuilder& setInterface(RenderPassInterface* interface) {
      this->interface = interface;
      return *this;
    }
    RenderPassBuilder& setBuildCallback(std::function<void(const CommandBuffer&)> cb) {
      this->buildCb = std::move(cb);
      return *this;
    }
    [[nodiscard]] std::function<void(const CommandBuffer&)>& getBuildCallback() { return buildCb; }
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

  private:
    RenderGraphBuilder& graph;
    PassId id;
    size_t index = ~0u;
    QueueType queue;
    std::string name;

    RenderPassInterface* interface = nullptr;
    std::function<void(const CommandBuffer&)> buildCb = nullptr;
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

    RenderBufferResource& addGenericBufferInput(const std::string& name, VkPipelineStageFlags2 stages, VkAccessFlags2 access,
                                                VkBufferUsageFlags usage);
  };
} // namespace kt::vkh