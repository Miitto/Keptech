#pragma once

#include "keptech/vulkan/wrappers/buffer.hpp"
#include "keptech/vulkan/wrappers/image.hpp"
#include "pass.hpp"
#include <vector>

namespace kt::vkh {
  class BakedRenderPass {
  public:
  private:
    std::string name;
    QueueType queue;

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

    std::vector<VkImageMemoryBarrier2> imageBarriers;
    std::vector<VkBufferMemoryBarrier2> bufferBarriers;
    VkDependencyInfo dependencyInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = nullptr,
        .dependencyFlags = 0,
    };
  };

  class RenderGraph {
  public:
  private:
    std::vector<BakedRenderPass> bakedPasses;

    std::vector<ResourceInfo> physicalResourceInfos;
    std::vector<std::unique_ptr<VkImageView>> physicalAttachments;
    std::vector<std::unique_ptr<Buffer>> physicalBuffers;
    std::vector<std::unique_ptr<Image>> physicalImageAttachments;
    std::vector<bool> physicalImageHasHistory;
  };
} // namespace kt::vkh