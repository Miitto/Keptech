#include "../builder.hpp"

#include "../graph.hpp"
#include "keptech/core/kt-logger.hpp"
#ifdef KT_VULKAN
#include "keptech/rhi/helpers/formatting.hpp"
#endif
#include "../renderResources.hpp"
#include <spdlog/fmt/bundled/ranges.h>
#include <vector>

namespace kt {
  bool RenderGraphBuilder::hasBufferResource(const std::string& name) const {
    return resourceNameToId.contains(name) && resources[resourceNameToId.at(name)]->getType() == RenderResource::Type::Buffer;
  }

  bool RenderGraphBuilder::hasTextureResource(const std::string& name) const {
    return resourceNameToId.contains(name) && resources[resourceNameToId.at(name)]->getType() == RenderResource::Type::Texture;
  }

  RenderTextureResource& RenderGraphBuilder::getTextureResource(const std::string& name) {
    auto it = resourceNameToId.find(name);
    if (it != resourceNameToId.end()) {
      KT_ASSERT(resources[it->second]->getType() == RenderResource::Type::Texture, "Resource with name '{}' is not a texture", name);
      return static_cast<RenderTextureResource&>(*resources[it->second]); // NOLINT
    }

    ResourceId id{resources.size()};
    auto res = std::make_unique<RenderTextureResource>(id);
    res->setName(name);
    resources.push_back(std::move(res));
    resourceNameToId[name] = id;

    return static_cast<RenderTextureResource&>(*resources.back()); // NOLINT
  }

  RenderBufferResource& RenderGraphBuilder::getBufferResource(const std::string& name) {
    auto it = resourceNameToId.find(name);
    if (it != resourceNameToId.end()) {
      KT_ASSERT(resources[it->second]->getType() == RenderResource::Type::Buffer, "Resource with name '{}' is not a buffer", name);
      return static_cast<RenderBufferResource&>(*resources[it->second]); // NOLINT
    }

    ResourceId id{resources.size()};
    auto res = std::make_unique<RenderBufferResource>(id);
    res->setName(name);
    resources.push_back(std::move(res));
    resourceNameToId[name] = id;

    return static_cast<RenderBufferResource&>(*resources.back()); // NOLINT
  }

  RenderPassBuilder& RenderGraphBuilder::addPass(const std::string& name, QueueType queueType, bool autoBeginRendering) {
    KT_TRACE("Adding pass '{}'", name);
    KT_ASSERT(!name.empty(), "Pass name cannot be empty");
    auto it = passNameToId.find(name);
    if (it != passNameToId.end()) {
      auto& pass = *passes[it->second];
      KT_REQUIRE(pass.getQueue() == queueType, "Pass '{}' already exists with a different queue type. Existing: {}, New: {}", name,
                 pass.getQueue(), queueType);
      KT_REQUIRE(pass.getAutoBeingRendering() == autoBeginRendering,
                 "Pass '{}' already exists with a different auto-begin rendering setting. Existing: {}, New: {}", name,
                 pass.getAutoBeingRendering(), autoBeginRendering);
      return pass;
    }

    PassId id{passes.size()};
    auto pass = std::make_unique<RenderPassBuilder>(*this, id, queueType, autoBeginRendering);
    pass->setName(name);
    passes.push_back(std::move(pass));
    passNameToId[name] = id;

    return *passes.back();
  }

  RenderPassBuilder* RenderGraphBuilder::findPass(const std::string& name) {
    auto it = passNameToId.find(name);
    if (it != passNameToId.end()) {
      return passes[it->second].get();
    }
    return nullptr;
  }

  bool RenderGraphBuilder::dependsOnPass(PassId dst, PassId src) const {
    if (dst == src)
      return true;

    for (auto& dep : passDependencies[dst]) {
      if (dependsOnPass(dep, src))
        return true;
    }

    return false;
  }

  ResourceInfo RenderGraphBuilder::getResourceInfo(RenderTextureResource& resource) const {
    auto& info = resource.getAttachmentInfo();
    ResourceInfo resInfo{
        .name = resource.getName(),
        .format = info.format,
        .sizeType = info.sizeType,
        .ratioSize = info.size,
        .size = {info.size.x, info.size.y, 1},
        .layers = info.layers,
        .mipLevels = info.mipLevels,
        .samples = info.samples,
        .persistent = info.persistent,
        .queues = resource.getUsedQueues(),
        .imageUsage = resource.getImageUsage(),
    };

    switch (info.sizeType) {
    case AttachmentSize::Absolute:
      resInfo.size.x = std::max(1u, static_cast<uint32_t>(info.size.x));
      resInfo.size.y = std::max(1u, static_cast<uint32_t>(info.size.y));
      resInfo.size.z = std::max(1u, static_cast<uint32_t>(info.size.z));
      break;
    case AttachmentSize::SwapchainRelative:
      resInfo.size.x = std::max(1u, static_cast<uint32_t>(std::ceil(info.size.x * static_cast<float>(swapchainSize.x))));
      resInfo.size.y = std::max(1u, static_cast<uint32_t>(std::ceil(info.size.y * static_cast<float>(swapchainSize.y))));
      resInfo.size.z = std::max(1u, static_cast<uint32_t>(info.size.z));
      break;
    case AttachmentSize::ResolutionRelative:
      resInfo.size.x = std::max(1u, static_cast<uint32_t>(std::ceil(info.size.x * static_cast<float>(renderResolution.x))));
      resInfo.size.y = std::max(1u, static_cast<uint32_t>(std::ceil(info.size.y * static_cast<float>(renderResolution.y))));
      resInfo.size.z = std::max(1u, static_cast<uint32_t>(info.size.z));
      break;
    };

    if (resInfo.format == rhi::ImageFormat::Undefined)
      resInfo.format = swapchainFormat;

    const auto numLevels = [](uint32_t width, uint32_t height, uint32_t depth) {
      return static_cast<uint32_t>(std::floor(std::log2(std::max({width, height, depth})))) + 1;
    };

    resInfo.mipLevels =
        std::min(numLevels(resInfo.size.x, resInfo.size.y, resInfo.size.z), resInfo.mipLevels == 0 ? ~0u : resInfo.mipLevels);

    return resInfo;
  }

  ResourceInfo RenderGraphBuilder::getResourceInfo(RenderBufferResource& resource) const {
    auto& info = resource.getBufferInfo();
    ResourceInfo resInfo{
        .name = resource.getName(),
        .bufferInfo = info,
        .persistent = info.persistent,
    };
#ifdef KT_VULKAN
    resInfo.bufferInfo.usage |= resource.getBufferUsage();
#endif
    return resInfo;
  }
} // namespace kt