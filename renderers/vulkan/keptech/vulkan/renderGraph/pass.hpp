#pragma once

#include "renderResources.hpp"
#include "vk-logger.hpp"
#include <Volk/volk.h>
#include <functional>

namespace kt::vkh {
  struct VulkanCore;
  class RenderGraph;
  class RenderPass;
  class CommandBuffer;

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

    virtual void setupDependencies(RenderPass& self, RenderGraph& graph) {}
    virtual void setup(const VulkanCore& vkcore) {}

    virtual void prepare(RenderGraph& graph) {}
    virtual void execute(CommandBuffer& cmd) {}
  };

  class RenderPass {
  public:
    RenderPass(RenderGraph& graph, PassId id, Bitflag<QueueType> queue) : graph(graph), id(id), queue(queue) {}

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

    RenderBufferResource& addStorageOutput(const std::string& name, const BufferInfo& info, const std::string& input = "");
    RenderBufferResource& addTransferOutput(const std::string& name, const BufferInfo& info);

    RenderBufferResource& addVertexBufferInput(const std::string& name);
    RenderBufferResource& addIndexBufferInput(const std::string& name);
    RenderBufferResource& addIndirectBufferInput(const std::string& name);

    void setupDependencies() {
      if (interface)
        interface->setupDependencies(*this, graph);
    }

    void setup(const VulkanCore& vkcore) {
      if (interface)
        interface->setup(vkcore);
    }

    void prepare(RenderGraph& graph) {
      if (interface)
        interface->prepare(graph);
    }

    void execute(CommandBuffer& cmd) {
      if (interface) {
        interface->execute(cmd);
      } else if (buildCb) {
        buildCb(cmd);
      }
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

    RenderPass& setName(const std::string& name) {
      this->name = name;
      return *this;
    }
    [[nodiscard]] const std::string& getName() const { return name; }

    [[nodiscard]] PassId getId() const { return id; }
    [[nodiscard]] Bitflag<QueueType> getQueues() const { return queue; }

    [[nodiscard]] RenderPassInterface* getInterface() const { return interface; }
    RenderPass& setInterface(RenderPassInterface* interface) {
      this->interface = interface;
      return *this;
    }
    RenderPass& setBuildCallback(std::function<void(CommandBuffer&)> cb) {
      this->buildCb = std::move(cb);
      return *this;
    }
    [[nodiscard]] const std::function<void(CommandBuffer&)>& getBuildCallback() const { return buildCb; }
    RenderPass& setGetClearDepthStencilCallback(std::function<bool(VkClearDepthStencilValue*)> cb) {
      this->getClearDepthStencilCb = std::move(cb);
      return *this;
    }
    [[nodiscard]] const std::function<bool(VkClearDepthStencilValue*)>& getGetClearDepthStencilCallback() const {
      return getClearDepthStencilCb;
    }
    RenderPass& setGetClearColorCallback(std::function<bool(unsigned, VkClearColorValue*)> cb) {
      this->getClearColorCb = std::move(cb);
      return *this;
    }
    [[nodiscard]] const std::function<bool(unsigned, VkClearColorValue*)>& getGetClearColorCallback() const { return getClearColorCb; }

    [[nodiscard]] const std::vector<RenderTextureResource*>& getColorOutputs() const { return colorOutputs; }
    [[nodiscard]] const std::vector<RenderTextureResource*>& getResolveOutputs() const { return resolveOutputs; }
    [[nodiscard]] const std::vector<RenderTextureResource*>& getColorInputs() const { return colorInputs; }
    [[nodiscard]] const std::vector<RenderTextureResource*>& getHistoryInputs() const { return historyInputs; }
    [[nodiscard]] const std::vector<RenderTextureResource*>& getAttachmentInputs() const { return attachmentInputs; }
    [[nodiscard]] const std::vector<RenderBufferResource*>& getStorageOutputs() const { return storageOutputs; }
    [[nodiscard]] const std::vector<RenderBufferResource*>& getStorageInputs() const { return storageInputs; }
    [[nodiscard]] const std::vector<RenderBufferResource*>& getTransferOutputs() const { return transferOutputs; }
    [[nodiscard]] RenderTextureResource* getDepthStencilInput() const { return depthStencilInput; }
    [[nodiscard]] RenderTextureResource* getDepthStencilOutput() const { return depthStencilOutput; }
    [[nodiscard]] const std::vector<AccessedTextureResource>& getGenericTextureInputs() const { return genericTexutre; }
    [[nodiscard]] const std::vector<AccessedBufferResource>& getGenericBufferInputs() const { return genericBuffers; }

    RenderPass& setIndex(size_t index) {
      this->index = index;
      return *this;
    }
    [[nodiscard]] size_t getIndex() const { return index; }

  private:
    RenderGraph& graph;
    PassId id;
    size_t index = ~0u;
    Bitflag<QueueType> queue;
    std::string name;

    RenderPassInterface* interface = nullptr;
    std::function<void(CommandBuffer&)> buildCb = nullptr;
    std::function<bool(VkClearDepthStencilValue*)> getClearDepthStencilCb = nullptr;
    std::function<bool(unsigned, VkClearColorValue*)> getClearColorCb = nullptr;

    std::vector<RenderTextureResource*> colorOutputs;
    std::vector<RenderTextureResource*> resolveOutputs;
    std::vector<RenderTextureResource*> colorInputs;
    std::vector<RenderTextureResource*> historyInputs;
    std::vector<RenderTextureResource*> attachmentInputs;
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