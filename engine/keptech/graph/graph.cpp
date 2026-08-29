#include "graph.hpp"
#include "keptech/components/transform.hpp"
#include "keptech/core/scene.hpp"
#include "keptech/core/version.h"
#include "keptech/rhi/bufferCreateInfo.hpp"
#include "keptech/rhi/cmdBuf.hpp"
#include "keptech/rhi/imageLayout.hpp"
#include "keptech/rhi/imageRef.hpp"
#include "keptech/rhi/profile.hpp"
#include "keptech/rhi/rhi.hpp"
#include "passInterface.hpp"
#include <imgui/imgui.h>
#include <vector>

namespace kt {
  using namespace rhi;

  RenderGraph* RenderGraph::activeGraph = nullptr;

  void RenderGraph::execute() {
    KT_PROFILE_FUNCTION

    updateDescriptors();

    auto& rhi = RHI::get();

    uint8_t frameIndex = rhi.getFrameIndex();

    buffersToDrop[frameIndex].clear();

    components::Transform::recalcAllTransforms(Scene::active().getEcs());

    rhi.startFrame();

    for (auto& pass : passes) {
      pass.prepare();
    }

    auto graphicsCmds = rhi.allocateGraphicsCommandBuffers(static_cast<uint32_t>(graphicsQueuePassCount) + 1);
    auto computeCmds = rhi.allocateComputeCommandBuffers(static_cast<uint32_t>(computeQueuePassCount));

    uint32_t graphicsCmdIndex = 0;
    uint32_t computeCmdIndex = 0;

    uint64_t startWaitFor = rhi.getTimelineValue();

    uint32_t passIdx = 0;
    for (const auto& [idxT, group] : passGroups | std::views::enumerate) {
      uint64_t idx = static_cast<uint64_t>(idxT);
      switch (group.queue) {
      case QueueType::Compute: // Compute has been compacted into the graphics queue, so this should never happen, but just to be safe.
      case QueueType::Graphics: {
        auto& cmd = graphicsCmds[graphicsCmdIndex];
        cmd.label(fmt::format("RenderGraph Graphics Pass Group {}", idx));
        for (uint32_t i = 0; i < group.count; ++i) {
          auto& descriptorSet = passDescriptors[passIdx].sets[frameIndex];
          auto& pass = passes[passIdx++];
          passBarriers(cmd, pass.getBarriers().pre);
          glm::uvec2 framebufferSize =
              pass.getQueue() == QueueType::Graphics ? resources.images[pass.getExtentSourceId()].getExtent() : glm::uvec2{0, 0};
          pass.execute(cmd, descriptorSet, framebufferSize);
          passBarriers(cmd, pass.getBarriers().post);
        }

        cmd.end();
        rhi.submitGraphicsCmd(cmd, startWaitFor + group.waitFor + 1, startWaitFor + idx + 1);
        graphicsCmdIndex++;
      } break;
      case QueueType::AsyncCompute: {
        auto& cmd = computeCmds[computeCmdIndex];
        cmd.label(fmt::format("RenderGraph Async Compute Pass Group {}", idx));
        for (uint32_t i = 0; i < group.count; ++i) {
          auto& descriptorSet = passDescriptors[passIdx].sets[frameIndex];
          auto& pass = passes[passIdx++];
          passBarriers(cmd, pass.getBarriers().pre);
          pass.execute(cmd, descriptorSet, {});
          passBarriers(cmd, pass.getBarriers().post);
        }
        cmd.end();
        rhi.submitComputeCmd(cmd, startWaitFor + group.waitFor + 1, startWaitFor + idx + 1);
        computeCmdIndex++;

      } break;
      case QueueType::Cpu: {
        passIdx += group.count; // CPU passes don't have any execute work, so just skip them.
      } break;
      }
    }

    debugUi();

    auto& cmd = graphicsCmds.back();
    auto& backSource = getBackbufferImage();
    auto swp = rhi.getSwapchainImage();

    rhi::ImageLayout optimalBlitSrc = rhi::CommandBuffer::getOptimalBlitSrcLayout();

    cmd.blitImage(backSource, optimalBlitSrc, optimalBlitSrc, swp, ImageLayout::Present, ImageLayout::RenderTarget);

    rhi.endFrame(cmd);
  }

  void RenderGraph::debugUi() {
    auto camera = Scene::active().getActiveCamera();
    if (camera.isValid()) {
      ImGui::Begin("Debug View");
      auto& camT = camera.getComponents<components::Transform>();
      auto camPos = camT.getGlobal()[3];

      ImGui::Text("Camera Position: %.2f, %.2f, %.2f", static_cast<double>(camPos.x), static_cast<double>(camPos.y),
                  static_cast<double>(camPos.z));

#ifndef KT_DISABLE_STATS
      auto& stats = RHI::get().getStats();
      ImGui::SeparatorText("Stats");
      ImGui::Text("Pipeline Switches: %llu", stats.pipelineSwitches);
      ImGui::Text("Draw Calls: %llu", stats.drawCalls);
      ImGui::Text("Dispatch Calls: %llu", stats.dispatchCalls);
      ImGui::Text("Render Passes: %llu", stats.renderPasses);
#endif
      ImGui::End();
    }

    constexpr const char* ktInfo = "KepTech v" KT_VERSION_STRING " (" KT_RHI_STRING ")";

    auto viewportSize = ImGui::GetMainViewport()->Size;
    // Needs to be always because of window resize
    ImGui::SetNextWindowPos({viewportSize.x, .0f}, ImGuiCond_None, {1.0f, 0.f});
    ImGui::SetNextWindowBgAlpha(0.f);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {2.f, 2.f});

    ImGui::Begin("KepTech Info", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoNav);
    ImGui::TextUnformatted(ktInfo);
    ImGui::End();
    ImGui::PopStyleVar(2);
  }

  void RenderGraph::destroy() {
    auto& renderer = RHI::get();

    renderer.waitIdle();

    for (auto& pass : passes) {
      pass.shutdown();
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

  void RenderGraph::passBarriers(rhi::CommandBuffer& cmd, const Barriers& barriers) {
    if (!barriers.image.empty()) {
      std::vector<CommandBuffer::ImageLayoutTransition> transitions;
      transitions.reserve(barriers.image.size());
      for (const auto& barrier : barriers.image) {
        auto& img = resources.images[barrier.resourceId];
        transitions.push_back(
            CommandBuffer::ImageLayoutTransition{.imageRef = img, .oldLayout = barrier.oldLayout, .newLayout = barrier.newLayout});
      }
      cmd.transitionImages(transitions);
    }

    // TODO: Buffer barriers
  }

  void RenderGraph::updateDescriptors() {
    // TODO: Update descriptors for buffers that have been realloc'd and images that have been resized.

    auto& rhi = RHI::get();
    auto frameIndex = rhi.getFrameIndex();

    std::vector<std::vector<rhi::DescriptorWriteInfo>> writes;
    writes.resize(passes.size());

    for (auto bufIdx : buffersToUpdate[frameIndex]) {
      auto& buf = resources.buffers[bufIdx];
      auto& usedInPass = resources.bufferUsedInPass[bufIdx];
      for (auto& used : usedInPass) {
        writes[used.passIndex].push_back(rhi::DescriptorWriteInfo{
            .binding = used.binding,
            .arrayIndex = 0,
            .type = used.descriptorType,
            .buffer = buf,
        });
      }
    }

    for (auto imageIdx : imagesToUpdate[frameIndex]) {
      auto& img = resources.images[imageIdx];
      auto& usedInPass = resources.imageUsedInPass[imageIdx];
      for (auto& used : usedInPass) {
        writes[used.passIndex].push_back(rhi::DescriptorWriteInfo{
            .binding = used.binding,
            .arrayIndex = 0,
            .type = used.descriptorType,
            .image = img,
        });
      }
    }

    for (size_t passIdx = 0; passIdx < passes.size(); ++passIdx) {
      if (!writes[passIdx].empty()) {
        for (size_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
          passDescriptors[passIdx].sets[frame].write(passDescriptors[passIdx].layout, writes[passIdx]);
        }
      }
    }

    buffersToUpdate[frameIndex].clear();
    imagesToUpdate[frameIndex].clear();
  }

  RenderGraph::RenderGraph(std::vector<PassGroup>&& passGroups, std::vector<RenderPass>&& passes, Resources&& resources,
                           std::vector<ImageTransition>&& initialTransitions, std::vector<Descriptors>&& descriptors)
      : passGroups(std::move(passGroups)), passes(std::move(passes)), resources(std::move(resources)),
        passDescriptors(std::move(descriptors)), initialTransitions(std::move(initialTransitions)) {
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
    auto& buf = resources.buffers[index + RHI::get().getFrameIndex()];
    return buf;
  }

  const Buffer& RenderGraph::reallocateBuffer(size_t index, size_t newSize, bool copyOldData) {
    KT_ASSERT(index < resources.buffers.size(), "Buffer index {} is out of bounds (size: {})", index, resources.buffers.size());
    auto& buf = resources.buffers[index];
    auto newBufRes = Buffer::create({
        newSize,
        buf.getUsage(),
        buf.getType(),
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

    buffersToDrop[RHI::get().getLastFrameIndex()].push_back(std::move(buf));
    resources.buffers[index] = std::move(newBufRes.value());
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      buffersToUpdate[i].push_back(index);
    }
    updateDescriptors();

    return buffersToDrop[RHI::get().getLastFrameIndex()].back();
  }

  const Buffer& RenderGraph::reallocatePerFrameBuffer(size_t gindex, size_t newSize, bool copyOldData) {
    KT_ASSERT(gindex < resources.buffers.size(), "Buffer index {} is out of bounds (size: {})", gindex, resources.buffers.size());
    KT_REQUIRE(resources.buffers[gindex].isMapped(), "Buffer at index {} is not a per-frame buffer.", gindex);

    size_t index = gindex + RHI::get().getFrameIndex();

    auto& buf = resources.buffers[index];

    auto newBufRes = Buffer::create({
        newSize,
        buf.getUsage(),
        buf.getType(),
        buf.getName().c_str(),
    });
    if (!newBufRes) {
      KT_ABORT("Failed to reallocate per-frame buffer '{}': {}", buf.getName(), newBufRes.error());
    }

    if (copyOldData) {
      std::memcpy(newBufRes.value().mapping(), buf.mapping(), std::min(buf.size(), newSize));
    }

    buffersToDrop[RHI::get().getLastFrameIndex()].push_back(std::move(buf));
    resources.buffers[index] = std::move(newBufRes.value());
    buffersToUpdate[RHI::get().getFrameIndex()].push_back(index);

    updateDescriptors();

    return buffersToDrop[RHI::get().getLastFrameIndex()].back();
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
  void RenderPass::setup(rhi::DescriptorLayout& layout) {
    if (passInterface)
      passInterface->setup(*graph, layout);
  }
  void RenderPass::prepare() {
    if (passInterface)
      passInterface->prepare(*graph);
  }
  void RenderPass::execute(CommandBuffer& cmd, rhi::DescriptorSet& descriptorSet, glm::uvec2 framebufferSize) {
    if (passInterface) {
      passInterface->execute(*graph, cmd, descriptorSet, framebufferSize);
    } else if (buildCb) {
      buildCb(*graph, cmd, descriptorSet, framebufferSize);
    }
  }
  void RenderPass::shutdown() {
    if (passInterface)
      passInterface->shutdown(*graph);
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

  void RenderGraph::setUserData(const std::string& key, void* data) { userData[key] = data; }

  void* RenderGraph::getUserData(const std::string& key) const {
    auto it = userData.find(key);
    if (it != userData.end()) {
      return it->second;
    }
    return nullptr;
  }

  const std::vector<rhi::Image>& RenderGraph::getImages() const { return resources.images; }

  void RenderGraph::onResolutionChanged(const glm::uvec2& newResolution) {
    KT_PROFILE_FUNCTION

    KT_TRACE("RenderGraph::onResolutionChanged() - New Resolution: {}x{}", newResolution.x, newResolution.y);

    std::vector<rhi::CommandBuffer::ImageLayoutTransition> transitions;

    for (auto& resImage : resources.resolutionRelativeImages) {
      auto& img = resources.images[resImage.index];

      glm::uvec3 newExtent = {static_cast<float>(newResolution.x) * resImage.ratio.x,
                              static_cast<float>(newResolution.y) * resImage.ratio.y, 1};

      auto newImageRes = img.resize(newExtent);
      if (!newImageRes) {
        KT_ABORT("Failed to create resolution relative image '{}': {}", img.getName(), newImageRes.error());
      }

      RHI::get().submitImageToDrop(img);
      resources.images[resImage.index] = std::move(newImageRes.value());

      for (auto& transition : initialTransitions) {
        if (static_cast<size_t>(transition.resourceId) == resImage.index) {
          transitions.push_back(CommandBuffer::ImageLayoutTransition{
              .imageRef = resources.images[resImage.index], .oldLayout = rhi::ImageLayout::Undefined, .newLayout = transition.newLayout});
        }
      }

      for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        imagesToUpdate[i].push_back(resImage.index);
      }
    }

    if (!transitions.empty()) {
      auto cmds = RHI::get().allocateGraphicsCommandBuffers(1);
      auto& cmd = cmds.front();
      cmd.transitionImages(transitions);
      cmd.end();
      RHI::get().submitGraphicsCmd(cmd, 0, RHI::get().getTimelineValue() + 1);
    }
  }

  void RenderGraph::onSwapchainSizeChanged(const glm::uvec2& newSize) {
    KT_PROFILE_FUNCTION

    KT_TRACE("RenderGraph::onSwapchainSizeChanged() - New Size: {}x{}", newSize.x, newSize.y);

    std::vector<rhi::CommandBuffer::ImageLayoutTransition> transitions;

    for (auto& resImage : resources.swapchainRelativeImages) {
      auto& img = resources.images[resImage.index];

      glm::uvec3 newExtent = {static_cast<float>(newSize.x) * resImage.ratio.x, static_cast<float>(newSize.y) * resImage.ratio.y, 1};

      auto newImageRes = img.resize(newExtent);
      if (!newImageRes) {
        KT_ABORT("Failed to create swapchain relative image '{}': {}", img.getName(), newImageRes.error());
      }

      RHI::get().submitImageToDrop(img);
      resources.images[resImage.index] = std::move(newImageRes.value());

      for (auto& transition : initialTransitions) {
        if (static_cast<size_t>(transition.resourceId) == resImage.index) {
          transitions.push_back(CommandBuffer::ImageLayoutTransition{
              .imageRef = resources.images[resImage.index], .oldLayout = rhi::ImageLayout::Undefined, .newLayout = transition.newLayout});
        }
      }

      for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        imagesToUpdate[i].push_back(resImage.index);
      }
    }

    if (!transitions.empty()) {
      auto cmds = RHI::get().allocateGraphicsCommandBuffers(1);
      auto& cmd = cmds.front();
      cmd.transitionImages(transitions);
      cmd.end();
      RHI::get().submitGraphicsCmd(cmd, 0, RHI::get().getTimelineValue() + 1);
    }
  }

  RenderGraph& RenderGraph::getActiveGraph() {
    KT_ASSERT(activeGraph, "No active render graph");
    return *activeGraph;
  }

} // namespace kt