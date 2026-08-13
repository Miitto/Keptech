#pragma once

#include "renderResources.hpp"
#include <functional>
#include <glm/ext/vector_uint2.hpp>

#ifdef KT_VULKAN
#include <volk.h>
#define KT_PIPELINE_STAGES , VkPipelineStageFlags2 stages = 0
#else
#define KT_PIPELINE_STAGES
#endif

namespace kt {
  namespace rhi {
    class RHI;
    class CommandBuffer;
    struct DepthClearValue;
    struct ColorClearValue;
  } // namespace rhi
  class RenderGraphBuilder;
  class RenderGraph;
  class RenderPass;
  class RenderPass;
  class RenderPassBuilder;
  class RenderPassInterface;

  struct AccessedResource {
#ifdef KT_VULKAN
    VkPipelineStageFlags2 stages = 0;
    VkAccessFlags2 access = 0;
#endif
    rhi::ImageLayout layout = rhi::ImageLayout::Undefined;
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

    RenderPassBuilder(RenderGraphBuilder& graph, PassId id, Bitflag<QueueType> queue, bool autoBeginRendering)
        : graph(graph), id(id), queue(queue), autoBeginRendering(autoBeginRendering) {}

    /// Sets the depth stencil attachment for this pass using VK_LOAD_OP_LOAD. To store the image after the pass, use setDepthStencilOutput
    /// with the same name.
    /// @note If there is a depth stencil input and output, they must use the same image.
    RenderTextureResource& setDepthStencilInput(const std::string& name);
    /// Sets the depth stencil attachment for this pass using VK_STORE_OP_STORE. To load the image before the pass, use setDepthStencilInput
    /// with the same name. The load operation will be LOAD if there is a depth stencil input, CLEAR if the pass returns true from
    /// getClearDepthStencil, or DontCare otherwise.
    /// @note If there is a depth stencil input and output, they must use the same image.
    RenderTextureResource& setDepthStencilOutput(const std::string& name, const AttachmentInfo& info);
    /// Adds a color attachment output for this pass. Will alias with the input and use VK_LOAD_OP_LOAD if the input name is provided.
    /// The attachment index is determined by the order of addition.
    /// @note The input must be added prior to baking, and the input and output must share the same AttachmentInfo.
    RenderTextureResource& addColorOutput(const std::string& name, const AttachmentInfo& info, const std::string& input = "");
    /// Add a texture that is used from last frame.
    RenderTextureResource& addHistoryInput(const std::string& name);

    /// Adds a texture input for this pass. The texture will be used as a sampled image in the shader.
    RenderTextureResource& addTextureInput(const std::string& name KT_PIPELINE_STAGES);
    /// Adds a uniform buffer input for this pass. The buffer will be used as a uniform buffer in the shader.
    RenderBufferResource& addUniformInput(const std::string& name KT_PIPELINE_STAGES);
    /// Adds a storage buffer input for this pass. The buffer will be used as a storage buffer in the shader. To read and write to the same
    /// buffer, use addStorageOutput with the input parameter.
    RenderBufferResource& addStorageReadOnlyInput(const std::string& name KT_PIPELINE_STAGES);

    /// Adds a storage image output for this pass. The image will be used as a storage image in the shader. Will alias with the input and
    /// use VK_LOAD_OP_LOAD if the input name is provided.
    /// @note The input must be added prior to baking, and the input and output must share the same AttachmentInfo.
    RenderTextureResource& addStorageImageOutput(const std::string& name, const AttachmentInfo& info, const std::string& input = "");

    /// Adds a storage buffer output for this pass. The buffer will be used as a storage buffer in the shader.
    RenderBufferResource& addStorageOutput(const std::string& name, const BufferInfo& info, const std::string& input = "");
    /// Adds a buffer that will be written to by a transfer operation in this pass. The buffer will be used as a transfer destination.
    RenderBufferResource& addTransferOutput(const std::string& name, const BufferInfo& info);

    /// Adds a vertex buffer input for this pass.
    RenderBufferResource& addVertexBufferInput(const std::string& name);
    /// Adds an index buffer input for this pass.
    RenderBufferResource& addIndexBufferInput(const std::string& name);
    /// Adds an indirect buffer input for this pass.
    RenderBufferResource& addIndirectBufferInput(const std::string& name);

    RenderBufferResource& addMappedBuffer(const std::string& name, size_t size, MappingMode allocFlags = MappingMode::SeqWrite,
                                          MemoryUsage memUsage = MemoryUsage::Auto);

    /// Makes an explicit dependency on another pass without any resource usage. This is useful for synchronizing passes that sahre
    /// resources not tracked by the render graph.
    RenderPassBuilder& addProxyPass(const std::string& name, QueueType queue = QueueType::Graphics, bool autoBeginRendering = true);

    void setupDependencies();

    RenderPassBuilder& setName(const std::string& n);
    [[nodiscard]] std::string& getName();
    [[nodiscard]] const std::string& getName() const;

    [[nodiscard]] PassId getId() const;
    [[nodiscard]] QueueType getQueue() const;

    [[nodiscard]] RenderPassInterface* getInterface() const;
    RenderPassBuilder& setInterface(RenderPassInterface* i);
    RenderPassBuilder& setBuildCallback(PassExecuteCb cb);
    [[nodiscard]] PassExecuteCb& getBuildCallback();
    RenderPassBuilder& setGetClearDepthStencilCallback(std::function<bool(rhi::DepthClearValue*)> cb);
    [[nodiscard]] std::function<bool(rhi::DepthClearValue*)>& getGetClearDepthStencilCallback();
    RenderPassBuilder& setGetClearColorCallback(std::function<bool(unsigned, rhi::ColorClearValue*)> cb);
    [[nodiscard]] std::function<bool(unsigned, rhi::ColorClearValue*)>& getGetClearColorCallback();

    bool getClearColor(uint32_t attachmentIndex, rhi::ColorClearValue* value = nullptr) const;

    bool getClearDepthStencil(rhi::DepthClearValue* value = nullptr) const;

    [[nodiscard]] const std::vector<RenderTextureResource*>& getColorOutputs() const;
    [[nodiscard]] const std::vector<RenderTextureResource*>& getColorInputs() const;
    [[nodiscard]] const std::vector<RenderTextureResource*>& getHistoryInputs() const;
    [[nodiscard]] const std::vector<RenderTextureResource*>& getStorageImageOutputs() const;
    [[nodiscard]] const std::vector<RenderTextureResource*>& getStorageImageInputs() const;
    [[nodiscard]] const std::vector<RenderBufferResource*>& getStorageOutputs() const;
    [[nodiscard]] const std::vector<RenderBufferResource*>& getStorageInputs() const;
    [[nodiscard]] const std::vector<RenderBufferResource*>& getTransferOutputs() const;
    [[nodiscard]] const std::vector<RenderBufferResource*>& getMappedBuffers() const;
    [[nodiscard]] RenderTextureResource* getDepthStencilInput() const;
    [[nodiscard]] RenderTextureResource* getDepthStencilOutput() const;
    [[nodiscard]] const std::vector<AccessedTextureResource>& getGenericTextureInputs() const;
    [[nodiscard]] const std::vector<AccessedBufferResource>& getGenericBufferInputs() const;
    const std::unordered_set<PassId>& getProxyPasses() const;

    bool getAutoBeingRendering() const;

    RenderPassBuilder& setIndex(size_t index);
    [[nodiscard]] size_t getIndex() const;

    void setDepthStencilLayout(rhi::ImageLayout layout);
    [[nodiscard]] rhi::ImageLayout getDepthStencilLayout() const;

  private:
    RenderGraphBuilder& graph; // NOLINT - Builder should never move.
    PassId id;
    size_t index = ~0u;
    QueueType queue;
    std::string name;
    bool autoBeginRendering;

    RenderPassInterface* passInterface = nullptr;
    PassExecuteCb buildCb = nullptr;
    std::function<bool(rhi::DepthClearValue*)> getClearDepthStencilCb = nullptr;
    std::function<bool(unsigned, rhi::ColorClearValue*)> getClearColorCb = nullptr;

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
    std::vector<RenderBufferResource*> mappedBuffers;
    std::vector<AccessedTextureResource> genericTexutre;
    std::vector<AccessedBufferResource> genericBuffers;
    RenderTextureResource* depthStencilInput = nullptr;
    RenderTextureResource* depthStencilOutput = nullptr;
    std::unordered_set<PassId> proxyPasses;

    rhi::ImageLayout depthStencilLayout = rhi::ImageLayout::Undefined;

    RenderBufferResource& addGenericBufferInput(const std::string& name, rhi::BufferUsage usage
#ifdef KT_VULKAN
                                                ,
                                                VkPipelineStageFlags2 stages, VkAccessFlags2 access
#endif
    );
  };
} // namespace kt

#undef KT_PIPELINE_STAGES