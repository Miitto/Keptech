#pragma once

#include "renderResources.hpp"
#include <Volk/volk.h>
#include <functional>
#include <glm/ext/vector_uint2.hpp>

namespace kt::rdr {
  class RenderGraphBuilder;
  class RenderGraph;
  class RenderPass;
  class CommandBuffer;
  class RenderPass;
  class RenderPassBuilder;
  class Renderer;
  class RenderPassInterface;

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
    RenderPassBuilder(const RenderPassBuilder&) = delete;
    RenderPassBuilder& operator=(const RenderPassBuilder&) = delete;
    RenderPassBuilder(RenderPassBuilder&&) = delete;
    RenderPassBuilder& operator=(RenderPassBuilder&&) = delete;
    ~RenderPassBuilder() = default;

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

    void setupDependencies(const Renderer& renderer);

    RenderPassBuilder& setName(const std::string& n);
    [[nodiscard]] std::string& getName();
    [[nodiscard]] const std::string& getName() const;

    [[nodiscard]] PassId getId() const;
    [[nodiscard]] QueueType getQueue() const;

    [[nodiscard]] RenderPassInterface* getInterface() const;
    RenderPassBuilder& setInterface(RenderPassInterface* i);
    RenderPassBuilder& setBuildCallback(PassExecuteCb cb);
    [[nodiscard]] PassExecuteCb& getBuildCallback();
    RenderPassBuilder& setGetClearDepthStencilCallback(std::function<bool(VkClearDepthStencilValue*)> cb);
    [[nodiscard]] std::function<bool(VkClearDepthStencilValue*)>& getGetClearDepthStencilCallback();
    RenderPassBuilder& setGetClearColorCallback(std::function<bool(unsigned, VkClearColorValue*)> cb);
    [[nodiscard]] std::function<bool(unsigned, VkClearColorValue*)>& getGetClearColorCallback();

    bool getClearColor(uint32_t attachmentIndex, VkClearColorValue* value = nullptr) const;

    bool getClearDepthStencil(VkClearDepthStencilValue* value = nullptr) const;

    [[nodiscard]] const std::vector<RenderTextureResource*>& getColorOutputs() const;
    [[nodiscard]] const std::vector<RenderTextureResource*>& getResolveOutputs() const;
    [[nodiscard]] const std::vector<RenderTextureResource*>& getColorInputs() const;
    [[nodiscard]] const std::vector<RenderTextureResource*>& getHistoryInputs() const;
    [[nodiscard]] const std::vector<RenderTextureResource*>& getAttachmentInputs() const;
    [[nodiscard]] const std::vector<RenderTextureResource*>& getStorageImageOutputs() const;
    [[nodiscard]] const std::vector<RenderTextureResource*>& getStorageImageInputs() const;
    [[nodiscard]] const std::vector<RenderBufferResource*>& getStorageOutputs() const;
    [[nodiscard]] const std::vector<RenderBufferResource*>& getStorageInputs() const;
    [[nodiscard]] const std::vector<RenderBufferResource*>& getTransferOutputs() const;
    [[nodiscard]] RenderTextureResource* getDepthStencilInput() const;
    [[nodiscard]] RenderTextureResource* getDepthStencilOutput() const;
    [[nodiscard]] const std::vector<AccessedTextureResource>& getGenericTextureInputs() const;
    [[nodiscard]] const std::vector<AccessedBufferResource>& getGenericBufferInputs() const;

    RenderPassBuilder& setIndex(size_t index);
    [[nodiscard]] size_t getIndex() const;

    void setDepthStencilLayout(VkImageLayout layout);
    [[nodiscard]] VkImageLayout getDepthStencilLayout() const;

  private:
    RenderGraphBuilder& graph; // NOLINT - Builder should never move.
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
} // namespace kt::rdr