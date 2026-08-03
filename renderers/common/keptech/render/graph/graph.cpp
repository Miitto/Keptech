#include "graph.hpp"
#include "keptech/render/interface.hpp"
#include "keptech/render/profile.hpp"
#include "keptech/render/renderer.hpp"
#include "keptech/render/wrappers/bufferCreateInfo.hpp"
#include "keptech/render/wrappers/cmdBuf.hpp"
#include "keptech/render/wrappers/imageCreateInfo.hpp"
#include "passInterface.hpp"
#include <vector>

namespace kt::rdr {
  void RenderGraph::execute() {
    KT_PROFILE_FUNCTION

    updateDescriptors();

    auto& renderer = Renderer::get();

    uint8_t frameIndex = renderer.getFrameIndex();

    imagesToDrop[frameIndex].clear();
    buffersToDrop[frameIndex].clear();
    engineCameraFrustum = renderer.startFrame();

    auto cmdBuf = runPasses();

    renderer.endFrame(cmdBuf);
  }

  void RenderGraph::destroy() {
    auto& renderer = Renderer::get();

    renderer.waitIdle();

    for (auto& pass : passes) {
      pass.shutdown(renderer);
    }

    // Just incase the passes have enqueued work on the device that needs to be completed before destroying resources.
    renderer.waitIdle();

#ifdef KT_VULKAN
    for (auto& d : passDescriptors) {
      vkDestroyDescriptorSetLayout(m.vkcore.device, d.layout, nullptr);
    }
    vkDestroyDescriptorPool(m.vkcore.device, descriptorPool, nullptr);
#endif

    for (auto& img : resources.images)
      img.destroy();
    for (auto& buf : resources.buffers)
      buf.destroy();
  }

  void RenderGraph::setBackbufferSource(const std::string& name) {
    auto it = resources.nameToImage.find(name);
    KT_REQUIRE(it != resources.nameToImage.end(), "Backbuffer source '{}' not found in render graph", name);
    backbufferSourceIndex = it->second;
    KT_DEBUG("Backbuffer source set to '{}' (index {})", name, backbufferSourceIndex);
  }

  [[nodiscard]] const Image& RenderGraph::getBackbufferImage() const {
    KT_REQUIRE(backbufferSourceIndex < resources.images.size(), "Backbuffer source index {} is out of bounds (size: {})",
               backbufferSourceIndex, resources.images.size());
    return resources.images[backbufferSourceIndex];
  }

  void RenderGraph::log() const {
#define LOG(...) KT_DEBUG(__VA_ARGS__) // NOLINT

    LOG("RenderGraph:");
    LOG("  Pass Groups: {}", passGroups.size());
    for (const auto& [idx, group] : passGroups | std::views::enumerate) {
      LOG("    {}: Queue: {}, Count: {}, WaitFor: {}", idx, group.queue, group.count,
          group.waitFor == ~0ull ? "None" : std::to_string(group.waitFor));
    }
    LOG("");
  }

  RenderGraph::RenderGraph(std::vector<PassGroup>&& passGroups, std::vector<RenderPass>&& passes, Resources&& resources
#ifdef KT_VULKAN
                           ,
                           VkDescriptorPool descriptorPool, std::vector<Descriptors>&& descriptors
#endif
                           )
      : passGroups(std::move(passGroups)), passes(std::move(passes)), resources(std::move(resources))
#ifdef KT_VULKAN
        ,
        descriptorPool(descriptorPool), passDescriptors(std::move(descriptors))
#endif
  {
    for (const auto& group : this->passGroups) {
      if (group.queue == QueueType::Graphics) {
        graphicsQueuePassCount += group.count;
      } else if (group.queue == QueueType::AsyncCompute) {
        computeQueuePassCount += group.count;
      } // Shouldn't be any normal compute queue groups as they have been compacted into the graphics queue groups.
    }

    for (auto& pass : this->passes) {
      pass.setGraph(*this);
    }
  }

  [[nodiscard]] const std::vector<RenderPass>& RenderGraph::getPasses() const { return passes; }
  [[nodiscard]] const std::vector<bool>& RenderGraph::getPhysicalImageHasHistory() const { return resources.physicalImageHasHistory; }
  [[nodiscard]] size_t RenderGraph::getImageIndex(const std::string& name) const {
    auto it = resources.nameToImage.find(name);
    KT_ASSERT(it != resources.nameToImage.end(), "Image resource with name '{}' not found in render graph", name);
    return it->second;
  }
  [[nodiscard]] size_t RenderGraph::getBufferIndex(const std::string& name) const {
    auto it = resources.nameToBuffer.find(name);
    KT_ASSERT(it != resources.nameToBuffer.end(), "Buffer resource with name '{}' not found in render graph", name);
    return it->second;
  }
  [[nodiscard]] const Image& RenderGraph::getImage(size_t index) const {
    KT_ASSERT(index < resources.images.size(), "Image index {} is out of bounds (size: {})", index, resources.images.size());
    return resources.images[index];
  }
  [[nodiscard]] const Buffer& RenderGraph::getBuffer(size_t index) const {
    KT_ASSERT(index < resources.buffers.size(), "Buffer index {} is out of bounds (size: {})", index, resources.buffers.size());
    return resources.buffers[index];
  }
  const Buffer& RenderGraph::getFrameBuffer(size_t index) const {
    KT_ASSERT(index < resources.buffers.size(), "Buffer index {} is out of bounds (size: {})", index, resources.buffers.size());
    KT_ASSERT(resources.buffers[index].isMapped(), "Called getFrameBuffer on a buffer that is not per-frame.");
    auto& buf = resources.buffers[index + Renderer::get().getFrameIndex()];
    return buf;
  }

  const Buffer& RenderGraph::reallocateBuffer(size_t index, size_t newSize, bool copyOldData) {
    KT_ASSERT(index < resources.buffers.size(), "Buffer index {} is out of bounds (size: {})", index, resources.buffers.size());
    auto& buf = resources.buffers[index];
    auto newBufRes = Buffer::create({
        newSize,
#ifdef KT_VULKAN
        buf.getUsage(),
#endif
        buf.getMappingMode(),
        MemoryUsage::Auto,
        buf.getName().c_str(),
    });
    if (!newBufRes) {
      KT_ABORT("Failed to reallocate buffer '{}': {}", buf.getName(), newBufRes.error());
    }

    if (copyOldData) {
      KT_ASSERT(buf.isMapped(),
                "Cannot copy old data from buffer '{}' because it is not mapped. You will need to explicitly manage the data transfer.",
                buf.getName());
      std::memcpy(newBufRes.value().mapping(), buf.mapping(), std::min(buf.size(), newSize));
    }

    buffersToDrop[Renderer::get().getLastFrameIndex()].push_back(std::move(buf));
    resources.buffers[index] = std::move(newBufRes.value());
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      buffersToUpdate[i].push_back(index);
    }
    updateDescriptors();

    return buffersToDrop[Renderer::get().getLastFrameIndex()].back();
  }

  const Buffer& RenderGraph::reallocatePerFrameBuffer(size_t gindex, size_t newSize, bool copyOldData) {
    KT_ASSERT(gindex < resources.buffers.size(), "Buffer index {} is out of bounds (size: {})", gindex, resources.buffers.size());
    KT_REQUIRE(resources.buffers[gindex].isMapped(), "Buffer at index {} is not a per-frame buffer.", gindex);

    size_t index = gindex + Renderer::get().getFrameIndex();

    auto& buf = resources.buffers[index];

    auto newBufRes = Buffer::create({
        newSize,
#ifdef KT_VULKAN
        buf.getUsage(),
#endif
        buf.getMappingMode(),
        MemoryUsage::Auto,
        buf.getName().c_str(),
    });
    if (!newBufRes) {
      KT_ABORT("Failed to reallocate per-frame buffer '{}': {}", buf.getName(), newBufRes.error());
    }

    if (copyOldData) {
      std::memcpy(newBufRes.value().mapping(), buf.mapping(), std::min(buf.size(), newSize));
    }

    buffersToDrop[Renderer::get().getLastFrameIndex()].push_back(std::move(buf));
    resources.buffers[index] = std::move(newBufRes.value());
    buffersToUpdate[Renderer::get().getFrameIndex()].push_back(index);

    updateDescriptors();

    return buffersToDrop[Renderer::get().getLastFrameIndex()].back();
  }

  ImageLayout RenderPass::getDepthStencilLayout() const { return depthStencilLayout; }

  void RenderPass::setDepthStencilLayout(ImageLayout layout) { depthStencilLayout = layout; }

  PhysResourceId RenderPass::getExtentSourceId() const { return extentSourceId; }

  void RenderPass::setExtentSourceId(PhysResourceId id) { extentSourceId = id; }

  const RenderAttachment& RenderPass::getDepthStencilAttachment() const { return depthStencilAttachment; }
  void RenderPass::setGraph(RenderGraph& g) { graph = &g; }
  void RenderPass::setInterface(RenderPassInterface* i) { passInterface = i; }
  void RenderPass::setBuildCallback(PassExecuteCb&& cb) { buildCb = std::move(cb); }
  void RenderPass::setGetClearDepthStencilCallback(std::function<bool(DepthClearValue*)>&& cb) { getClearDepthStencilCb = std::move(cb); }
  void RenderPass::setGetClearColorCallback(std::function<bool(uint32_t, ColorClearValue*)>&& cb) { getClearColorCb = std::move(cb); }
  void RenderPass::setup(Renderer& renderer
#ifdef KT_VULKAN
                         ,
                         VkDescriptorSetLayout descriptorSetLayout
#endif
  ) {
    if (passInterface)
      passInterface->setup(*graph, renderer
#ifdef KT_VULKAN
                           ,
                           descriptorSetLayout
#endif
      );
  }
  void RenderPass::prepare(Renderer& renderer) {
    if (passInterface)
      passInterface->prepare(*graph, renderer);
  }
  void RenderPass::execute(const CommandBuffer& cmd,
#ifdef KT_VULKAN
                           VkDescriptorSet descriptorSet,
#endif
                           glm::uvec3 framebufferSize) {
    if (passInterface) {
      passInterface->execute(*graph, cmd,
#ifdef KT_VULKAN
                             descriptorSet,
#endif
                             framebufferSize);
    } else if (buildCb) {
      buildCb(cmd,
#ifdef KT_VULKAN
              descriptorSet,
#endif
              framebufferSize);
    }
  }
  void RenderPass::shutdown(Renderer& renderer) {
    if (passInterface)
      passInterface->shutdown(*graph, renderer);
  }
  bool RenderPass::getClearColor(uint32_t attachmentIndex, ColorClearValue* value) const {
    if (passInterface)
      return passInterface->getClearColor(attachmentIndex, value);
    else if (getClearColorCb)
      return getClearColorCb(attachmentIndex, value);

    return false;
  }
  bool RenderPass::getClearDepthStencil(DepthClearValue* value) const {
    if (passInterface)
      return passInterface->getClearDepthStencil(value);
    else if (getClearDepthStencilCb)
      return getClearDepthStencilCb(value);

    return false;
  }
  [[nodiscard]] const PrePostBarriers& RenderPass::getBarriers() const { return barriers; }
  [[nodiscard]] const std::string& RenderPass::getName() const { return name; }
  [[nodiscard]] QueueType RenderPass::getQueue() const { return queue; }
  [[nodiscard]] const std::vector<RenderAttachment>& RenderPass::getColorAttachments() const { return colorAttachments; }

  void RenderGraph::onResolutionChanged(const glm::uvec2& newResolution) {
    KT_PROFILE_FUNCTION

    KT_TRACE("RenderGraph::onResolutionChanged() - New Resolution: {}x{}", newResolution.x, newResolution.y);

    for (auto& resImage : resources.resolutionRelativeImages) {
      auto& img = resources.images[resImage.index];

      glm::uvec3 newExtent = {static_cast<float>(newResolution.x) * resImage.ratio.x,
                              static_cast<float>(newResolution.y) * resImage.ratio.y, 1};

      auto newImageRes = Image::create({ImageDim::e2D, img.format(), newExtent,
#ifdef KT_VULKAN
                                        img.getUsage(),
#endif
                                        img.mips(), img.layers(), img.getName().c_str()});
      if (!newImageRes) {
        KT_ABORT("Failed to create resolution relative image '{}': {}", img.getName(), newImageRes.error());
      }

      imagesToDrop[Renderer::get().getLastFrameIndex()].push_back(std::move(img));
      resources.images[resImage.index] = std::move(newImageRes.value());

      for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        imagesToUpdate[i].push_back(resImage.index);
      }
    }
  }

  void RenderGraph::onSwapchainSizeChanged(const glm::uvec2& newSize) {
    KT_PROFILE_FUNCTION

    KT_TRACE("RenderGraph::onSwapchainSizeChanged() - New Size: {}x{}", newSize.x, newSize.y);

    for (auto& resImage : resources.swapchainRelativeImages) {
      auto& img = resources.images[resImage.index];

      glm::uvec3 newExtent = {static_cast<float>(newSize.x) * resImage.ratio.x, static_cast<float>(newSize.y) * resImage.ratio.y, 1};

      auto newImageRes = Image::create({ImageDim::e2D, img.format(), newExtent,
#ifdef KT_VULKAN
                                        img.getUsage(),
#endif
                                        img.mips(), img.layers(), img.getName().c_str()});
      if (!newImageRes) {
        KT_ABORT("Failed to create swapchain relative image '{}': {}", img.getName(), newImageRes.error());
      }

      imagesToDrop[Renderer::get().getLastFrameIndex()].push_back(std::move(img));
      resources.images[resImage.index] = std::move(newImageRes.value());

      for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        imagesToUpdate[i].push_back(resImage.index);
      }
    }
  }

} // namespace kt::rdr