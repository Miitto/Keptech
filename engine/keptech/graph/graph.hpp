#pragma once

#include "keptech/maths/frustum.hpp"
#include "keptech/rhi/buffer.hpp"
#include "keptech/rhi/constants.hpp"
#include "keptech/rhi/descriptorPool.hpp"
#include "keptech/rhi/image.hpp"
#include "keptech/rhi/loadStoreOps.hpp"
#include "pass.hpp"
#include <array>
#include <glm/ext/vector_uint2.hpp>
#include <string>
#include <vector>

namespace kt {
  namespace rhi {
    class RHI;
  }
  class RenderGraphBuilder;
  class RenderGraph;

  struct RenderAttachment {
    PhysResourceId resourceId{};
    rhi::LoadOp loadOp = rhi::LoadOp::DontCare;
    rhi::StoreOp storeOp = rhi::StoreOp::DontCare;

    operator bool() const { return resourceId.used(); }
  };

  class RenderPass {
  public:
    friend class RenderGraphBuilder;

    void setGraph(RenderGraph& g);
    void setInterface(RenderPassInterface* i);
    void setBuildCallback(PassExecuteCb&& cb);
    void setGetClearDepthStencilCallback(std::function<bool(rhi::DepthClearValue*)>&& cb);
    void setGetClearColorCallback(std::function<bool(uint32_t, rhi::ColorClearValue*)>&& cb);

    void setup(rhi::DescriptorLayout& layout);

    void prepare();

    void execute(rhi::CommandBuffer& cmd, rhi::DescriptorSet& descriptorSet, glm::uvec2 framebufferSize = {});

    void shutdown();

    bool getClearColor(uint32_t attachmentIndex, rhi::ColorClearValue* value = nullptr) const;

    bool getClearDepthStencil(rhi::DepthClearValue* value = nullptr) const;

    bool getAutoBeginRendering() const { return autoBeginRendering; }

    [[nodiscard]] const PrePostBarriers& getBarriers() const;
    [[nodiscard]] const std::string& getName() const;
    [[nodiscard]] QueueType getQueue() const;

    [[nodiscard]] const std::vector<RenderAttachment>& getColorAttachments() const;
    [[nodiscard]] const RenderAttachment& getDepthStencilAttachment() const;

    void setExtentSourceId(PhysResourceId id);
    [[nodiscard]] PhysResourceId getExtentSourceId() const;

    void setDepthStencilLayout(rhi::ImageLayout layout);
    [[nodiscard]] rhi::ImageLayout getDepthStencilLayout() const;

  private:
    RenderPass(std::string&& name, QueueType queue, PrePostBarriers&& barriers, std::vector<RenderAttachment>&& colorAttachments,
               RenderAttachment depthStencilAttachment, bool autoBeginRendering)
        : name(std::move(name)), queue(queue), autoBeginRendering(autoBeginRendering), barriers(std::move(barriers)),
          colorAttachments(std::move(colorAttachments)), depthStencilAttachment(depthStencilAttachment) {}

    RenderGraph* graph = nullptr;
    std::string name;
    QueueType queue;
    bool autoBeginRendering = true;

    PhysResourceId extentSourceId{};

    RenderPassInterface* passInterface = nullptr;
    PassExecuteCb buildCb = nullptr;
    std::function<bool(rhi::DepthClearValue*)> getClearDepthStencilCb = nullptr;
    std::function<bool(unsigned, rhi::ColorClearValue*)> getClearColorCb = nullptr;

    PrePostBarriers barriers;
    std::vector<RenderAttachment> colorAttachments;
    RenderAttachment depthStencilAttachment;
    rhi::ImageLayout depthStencilLayout = rhi::ImageLayout::Undefined;
  };

  class RenderGraph {
  public:
    // Friend so it is the only thing that can construct a RenderGraph.
    friend class RenderGraphBuilder;

    void execute();

    const maths::Frustum& getEngineCameraFrustum() const { return engineCameraFrustum; }

    [[nodiscard]] const std::vector<RenderPass>& getPasses() const;
    [[nodiscard]] const std::vector<bool>& getPhysicalImageHasHistory() const;

    /// Get the index of the image resource with the given name. Throws if the resource does not exist.
    /// The returned index is safe to store as it will remain constant even if the image is resized. Use getImage() to get the image at the
    /// index.
    [[nodiscard]] size_t getImageIndex(const std::string& name) const;

    /// Get the index of the buffer resource with the given name. Throws if the resource does not exist.
    /// The returned index is safe to store as it will remain constant even if the buffer is resized. Use getBuffer() to get the buffer at
    /// the index. CPU Mapped buffers are contiguous so to get the current frames buffer you can use the index returned by this function and
    /// add the current frame index to it.
    [[nodiscard]] size_t getBufferIndex(const std::string& name) const;

    /// Get the image at the given index. Do not store a reference to the image as it may become invalid if the image is resized. Use the
    /// index to get the image again if needed.
    [[nodiscard]] const rhi::Image& getImage(size_t index) const;

    /// Get the buffer at the given index. Do not store a reference to the buffer as it may become invalid if the buffer is resized. Use the
    /// index to get the buffer again if needed. Use getFrameBuffer() to get the buffer for the current frame, or since per-frame buffers
    /// are contiguous you can also add the current frame index to the index given to this method.
    [[nodiscard]] const rhi::Buffer& getBuffer(size_t index) const;

    /// Get the buffer at the given index for the current frame. Do not store a reference to the buffer as it may become invalid if the
    /// buffer is resized. Use the index to get the buffer again if needed.
    [[nodiscard]] const rhi::Buffer& getFrameBuffer(size_t index) const;

    /// Reallocates the buffer at the given index to the new size. This should not be used for CPU Mapped buffers that are allocated
    /// per-frame, use reallocatePerFrameBuffer() for those. If copyOldData is true, the old data will be copied to the new buffer.
    /// @warning This should only be called for the first pass to use this buffer in the frame, otherwise it will likely write to an in-use
    /// descriptor set.
    /// @returns A reference to the old buffer that was replaced. This buffer will be dropped at the end of the frame and should not be used
    /// after that.
    const rhi::Buffer& reallocateBuffer(size_t index, size_t newSize, bool copyOldData);

    /// Reallocates the per-frame buffer at the given index. This should only be used for CPU Mapped buffers that are allocated per-frame.
    /// @warning This should only be called for the first pass to use this buffer in the frame, otherwise it will likely write to an in-use
    /// descriptor set.
    /// @returns A reference to the old buffer that was replaced. This buffer will be dropped at the end of the frame and should not be used
    /// after that.
    const rhi::Buffer& reallocatePerFrameBuffer(size_t index, size_t newSize, bool copyOldData);

    void setUserData(const std::string& key, void* data);
    [[nodiscard]] void* getUserData(const std::string& key) const;
    template <typename T> void setUserData(const std::string& key, T* data) { setUserData(key, static_cast<void*>(data)); }
    template <typename T> [[nodiscard]] T* getUserData(const std::string& key) const { return static_cast<T*>(getUserData(key)); }

    void destroy();

    void setBackbufferSource(const std::string& name);

    [[nodiscard]] const rhi::Image& getBackbufferImage() const;

    void log() const;

    void onResolutionChanged(const glm::uvec2& newResolution);
    void onSwapchainSizeChanged(const glm::uvec2& newSize);

    static RenderGraph& getActiveGraph();

    [[nodiscard]] static RenderGraph* getActiveGraphPtr() { return activeGraph; }

    /// For internal use only.
    static void setActiveGraph(RenderGraph* graph) { activeGraph = graph; }

    const std::vector<rhi::Image>& getImages() const;
    rhi::ImageLayout getFinalLayout(size_t imageIndex) const;

  private:
    RenderGraph(std::vector<PassGroup>&& passGroups, std::vector<RenderPass>&& passes, Resources&& resources,
                std::vector<rhi::ImageLayout>&& finalLayouts, std::vector<Descriptors>&& descriptors);

    std::unordered_map<std::string, void*> userData;

    maths::Frustum engineCameraFrustum{};

    size_t graphicsQueuePassCount = 0;
    size_t computeQueuePassCount = 0;
    std::vector<PassGroup> passGroups;
    std::vector<RenderPass> passes;

    Resources resources;

    std::vector<Descriptors> passDescriptors;

    size_t backbufferSourceIndex = 0;

    std::vector<rhi::ImageLayout> finalLayouts;

    std::array<std::vector<rhi::Buffer>, MAX_FRAMES_IN_FLIGHT> buffersToDrop;
    std::array<std::vector<size_t>, MAX_FRAMES_IN_FLIGHT> imagesToUpdate;
    std::array<std::vector<size_t>, MAX_FRAMES_IN_FLIGHT> buffersToUpdate;

    void updateDescriptors();
    void passBarriers(rhi::CommandBuffer& cmd, const Barriers& barriers);

    void debugUi();

    static RenderGraph* activeGraph;
  };
} // namespace kt