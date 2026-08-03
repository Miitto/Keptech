#include "builder.hpp"

#include "graph.hpp"
#include "keptech/core/kt-logger.hpp"
#include "keptech/render/helpers/formatting.hpp"
#include "keptech/render/renderer.hpp"
#include "keptech/render/wrappers/buffer.hpp"
#include "keptech/render/wrappers/bufferCreateInfo.hpp"
#include "keptech/render/wrappers/imageCreateInfo.hpp"
#include "renderResources.hpp"
#include <algorithm>
#include <ranges>
#include <spdlog/fmt/bundled/ranges.h>
#include <vector>

template <> struct fmt::formatter<kt::rdr::QueueHandoff> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(const kt::rdr::QueueHandoff& handoff, FormatContext& ctx) const {
    switch (handoff) {
    case kt::rdr::QueueHandoff::No:
      return fmt::formatter<std::string_view>::format("No", ctx);
    case kt::rdr::QueueHandoff::ToCompute:
      return fmt::formatter<std::string_view>::format("ToCompute", ctx);
    case kt::rdr::QueueHandoff::FromCompute:
      return fmt::formatter<std::string_view>::format("FromCompute", ctx);
    }
  }
};

namespace kt::rdr {
  static constexpr Bitflag<QueueType> COMPUTE_QUEUES = QueueType::Compute | QueueType::AsyncCompute;

  namespace {
    QueueType getRawQueue(QueueType queue) {
      if (queue == QueueType::Compute)
        return QueueType::Compute;
      return queue;
    }
  } // namespace

  void RenderGraphBuilder::bake(const Renderer& renderer) {
    KT_REQUIRE(swapchainSize.x > 0 && swapchainSize.y > 0, "Swapchain size must be set before baking the render graph.");
    KT_REQUIRE(renderResolution.x > 0 && renderResolution.y > 0, "Render resolution must be set before baking the render graph.");
    KT_REQUIRE(swapchainFormat != KT_FORMAT_UNDEFINED, "Swapchain format must be set before baking the render graph.");
    KT_REQUIRE(!passes.empty(), "No passes have been added to the render graph. At least one pass must be added before baking.");

    for (auto& pass : passes) {
      pass->setupDependencies(renderer);
    }

    validatePasses();

    auto it = resourceNameToId.find(backbufferSource);
    KT_REQUIRE(it != resourceNameToId.end(), "Backbuffer source '{}' not found in render graph", backbufferSource);

    passStack.clear();
    passDependencies.clear();
    passDependencies.resize(passes.size());

    auto& backbuffer = *resources[it->second];

    KT_REQUIRE(!backbuffer.getWritePasses().empty(), "Backbuffer source '{}' must be written by at least one pass", backbufferSource);

    passStack.append_range(backbuffer.getWritePasses());

    auto tmp = passStack;
    for (auto& pushed : tmp) {
      auto& pass = *passes[pushed];
      traverseDependencies(pass, 0);
    }

    std::ranges::reverse(passStack);

    filterPasses(passStack);

    reorderPasses(passStack);

    buildPhysicalResources();
    buildRequirements();
    buildBarriers();
  }

  namespace {

    std::vector<PassGroup> buildPassGroups(const std::vector<RenderPass>& passes) {
      QueueType queue = getRawQueue(passes.front().getQueue());
      std::vector<PassGroup> passGroups;
      PassGroup currentGroup{.queue = queue};
      for (const auto& pass : passes) {
        // If we changed queue, or pass in a different queue depends on a resource from this pass, or we depend on a resource from a pass in
        // a different queue, we need to start a new group.
        if (getRawQueue(pass.getQueue()) != currentGroup.queue || !pass.getBarriers().post.image.empty() ||
            !pass.getBarriers().post.buffer.empty() || pass.getBarriers().needsWaitFor != ~0u) {
          passGroups.push_back(currentGroup);
          currentGroup = {.queue = getRawQueue(pass.getQueue())};

          if (pass.getBarriers().needsWaitFor != ~0u) {
            size_t maxGroup = 0;
            for (const auto& [i, p] : passGroups | std::views::enumerate) {
              maxGroup += p.count;
              if (pass.getBarriers().needsWaitFor < maxGroup) {
                currentGroup.waitFor = static_cast<size_t>(i);
                break;
              }
            }
          }
        }
        ++currentGroup.count;
      }
      passGroups.push_back(currentGroup); // Should always be none 0?

      return passGroups;
    }
  } // namespace

  RenderGraph RenderGraphBuilder::build(Renderer& renderer) {
    auto& members = renderer.getMembers();

    auto builtResources = buildResources();

#ifdef KT_VULKAN
    auto descriptorPool = buildDescriptorPool(renderer, builtResources);
    auto passDescriptors = buildDescriptors(renderer, builtResources, descriptorPool);
#endif

    auto bakedPasses = bakePasses(builtResources);
    auto passGroups = buildPassGroups(bakedPasses);

    RenderGraph res{
        std::move(passGroups),
        std::move(bakedPasses),
        std::move(builtResources)
#ifdef KT_VULKAN
            ,
        descriptorPool,
        std::move(passDescriptors),
#endif
    };

    res.setBackbufferSource(backbufferSource);

    KT_DEBUG("Render graph built successfully with {} passes, {} images, and {} buffers", res.passes.size(), res.resources.images.size(),
             res.resources.buffers.size());

    for (const auto& [idx, pass] : res.passes | std::views::enumerate) {
      pass.setup(renderer
#ifdef KT_VULKAN
                 ,
                 res.passDescriptors[static_cast<size_t>(idx)].layout
#endif
      );
    }

    return res;
  }

#ifdef KT_VULKAN
  VkDescriptorPool RenderGraphBuilder::buildDescriptorPool(const Renderer& renderer, const Resources& resources) {
    struct DescriptorCounts {
      size_t textures = 0;
      size_t storageImages = 0;
      size_t uniformBuffers = 0;
      size_t storageBuffers = 0;
    } counts;

    for (const auto& passId : passStack) {
      auto& pass = passes[passId];
      counts.textures += pass->getGenericTextureInputs().size();
      counts.storageImages += pass->getStorageImageOutputs().size();
      for (const auto& buffer : pass->getGenericBufferInputs()) {
        if (buffer.buffer) {
          if (buffer.access & VK_ACCESS_UNIFORM_READ_BIT) {
            ++counts.uniformBuffers;
          } else if (buffer.access & VK_ACCESS_2_SHADER_STORAGE_READ_BIT) {
            ++counts.storageBuffers;
          }
        }
      }
      counts.storageBuffers += pass->getStorageOutputs().size();
    }

    std::vector<VkDescriptorPoolSize> poolSizes = {
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                             .descriptorCount = static_cast<uint32_t>(counts.textures * MAX_FRAMES_IN_FLIGHT)},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                             .descriptorCount = static_cast<uint32_t>(counts.storageImages * MAX_FRAMES_IN_FLIGHT)},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                             .descriptorCount = static_cast<uint32_t>(counts.uniformBuffers * MAX_FRAMES_IN_FLIGHT)},
        VkDescriptorPoolSize{.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                             .descriptorCount = static_cast<uint32_t>(counts.storageBuffers * MAX_FRAMES_IN_FLIGHT)}};

    poolSizes.erase(
        std::remove_if(poolSizes.begin(), poolSizes.end(), [](const VkDescriptorPoolSize& size) { return size.descriptorCount == 0; }),
        poolSizes.end());

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
        .maxSets = static_cast<uint32_t>(passes.size() * MAX_FRAMES_IN_FLIGHT),
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data(),
    };
    VkDescriptorPool descriptorPool = nullptr;
    KT_REQUIRE(vkCreateDescriptorPool(members.vkcore.device, &poolInfo, nullptr, &descriptorPool) == VK_SUCCESS,
               "Failed to create descriptor pool for render graph.");

    return descriptorPool;
  }

  std::vector<Descriptors> RenderGraphBuilder::buildDescriptors(const Renderer& renderer, const Resources& resources,
                                                                VkDescriptorPool descriptorPool) {

    std::vector<Descriptors> passDescriptors;
    passDescriptors.reserve(passStack.size());

    builtResources.imageUsedInPass.resize(builtResources.images.size());
    builtResources.bufferUsedInPass.resize(builtResources.buffers.size());

    for (const auto& [idxL, passId] : passStack | std::views::enumerate) {
      size_t idx = static_cast<size_t>(idxL);
      auto& pass = passes[passId];

      struct PerFrameWrite {
        size_t writeIndex;
        size_t bufferIndex;
      };

      std::vector<VkDescriptorImageInfo> imageInfos;
      std::vector<VkDescriptorBufferInfo> bufferInfos;
      std::vector<VkWriteDescriptorSet> writes;
      std::vector<PerFrameWrite> perFrameWrites;

      size_t imgMaxCount = pass->getGenericTextureInputs().size() + pass->getStorageImageOutputs().size();
      size_t bufMaxCount = (pass->getGenericBufferInputs().size() + pass->getStorageOutputs().size()) *
                           MAX_FRAMES_IN_FLIGHT; // Reserve max bound of every buffer being CPU mapped (and hence duplicated).
      size_t writeMaxCount = imgMaxCount + bufMaxCount;

      imageInfos.reserve(imgMaxCount);
      bufferInfos.reserve(bufMaxCount); // Reserve max bound of every buffer being CPU mapped.
      writes.reserve(writeMaxCount);

      auto getImage = [&](const std::string& name) -> Image& { return builtResources.images[builtResources.nameToImage[name]]; };
      auto getBuffer = [&](const std::string& name, size_t offset = 0) -> Buffer& {
        return builtResources.buffers[builtResources.nameToBuffer[name] + offset];
      };

      std::vector<VkDescriptorBindingFlags> bindingFlags;
      std::vector<VkDescriptorSetLayoutBinding> bindings;
      for (const auto& texture : pass->getGenericTextureInputs()) {
        if (texture.texture && texture.texture->getPhysicalId().used()) {
          uint32_t binding = static_cast<uint32_t>(bindings.size());
          KT_DEBUG("Pass '{}' has texture input '{}' at binding {}", pass->getName(), texture.texture->getName(), binding);
          bindings.push_back(VkDescriptorSetLayoutBinding{
              .binding = static_cast<uint32_t>(binding),
              .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
              .descriptorCount = 1,
              .stageFlags = 0,
              .pImmutableSamplers = nullptr,
          });
          switch (pass->getQueue()) {
          case QueueType::Graphics:
            bindings.back().stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
          case QueueType::Compute:
          case QueueType::AsyncCompute:
            bindings.back().stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            break;
          }
          bindingFlags.push_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);

          imageInfos.push_back(VkDescriptorImageInfo{
              .sampler = VK_NULL_HANDLE,
              .imageView = getImage(texture.texture->getName()),
              .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
          });
          writes.push_back(VkWriteDescriptorSet{
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstBinding = static_cast<uint32_t>(bindings.size() - 1),
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
              .pImageInfo = &imageInfos.back(),
          });
          if (texture.texture->getPhysicalId().used())
            builtResources.imageUsedInPass[texture.texture->getPhysicalId()].push_back(
                UsedInPass{.passIndex = idx,
                           .binding = binding,
                           .layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                           .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE});
        }
      }
      for (const auto& buffer : pass->getGenericBufferInputs()) {
        if (buffer.buffer && buffer.buffer->getPhysicalId().used()) {
          uint32_t binding = static_cast<uint32_t>(bindings.size());
          KT_DEBUG("Pass '{}' has {} buffer input '{}' at binding {}", pass->getName(),
                   (buffer.access & VK_ACCESS_UNIFORM_READ_BIT) ? "uniform" : "storage", buffer.buffer->getName(), binding);
          bindings.push_back(VkDescriptorSetLayoutBinding{
              .binding = binding,
              .descriptorType =
                  (buffer.access & VK_ACCESS_UNIFORM_READ_BIT) ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags = 0,
              .pImmutableSamplers = nullptr,
          });
          switch (pass->getQueue()) {
          case QueueType::Graphics:
            bindings.back().stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
          case QueueType::Compute:
          case QueueType::AsyncCompute:
            bindings.back().stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            break;
          }
          bindingFlags.push_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);

          bufferInfos.push_back(VkDescriptorBufferInfo{
              .buffer = getBuffer(buffer.buffer->getName()),
              .offset = 0,
              .range = buffer.buffer->getBufferInfo().size,
          });
          writes.push_back(VkWriteDescriptorSet{
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstBinding = static_cast<uint32_t>(bindings.size() - 1),
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType =
                  (buffer.access & VK_ACCESS_UNIFORM_READ_BIT) ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .pBufferInfo = &bufferInfos.back(),
          });
          if (buffer.buffer->getBufferInfo().isHostAccessible()) {
            perFrameWrites.push_back({.writeIndex = writes.size() - 1, .bufferIndex = bufferInfos.size() - 1});
            for (size_t i = 1; i < MAX_FRAMES_IN_FLIGHT; ++i) {
              bufferInfos.push_back(VkDescriptorBufferInfo{
                  .buffer = getBuffer(buffer.buffer->getName(), i),
                  .offset = 0,
                  .range = buffer.buffer->getBufferInfo().size,
              });
            }
          }
          if (buffer.buffer->getPhysicalId().used())
            builtResources.bufferUsedInPass[buffer.buffer->getPhysicalId()].push_back(
                UsedInPass{.passIndex = idx, .binding = binding, .descriptorType = writes.back().descriptorType});
        }
      }

      for (const auto& image : pass->getStorageImageOutputs()) {
        if (image && image->getPhysicalId().used()) {
          uint32_t binding = static_cast<uint32_t>(bindings.size());
          KT_DEBUG("Pass '{}' has storage image output '{}' at binding {}", pass->getName(), image->getName(), binding);
          bindings.push_back(VkDescriptorSetLayoutBinding{
              .binding = binding,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .descriptorCount = 1,
              .stageFlags = 0,
              .pImmutableSamplers = nullptr,
          });
          switch (pass->getQueue()) {
          case QueueType::Graphics:
            bindings.back().stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
          case QueueType::Compute:
          case QueueType::AsyncCompute:
            bindings.back().stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            break;
          }
          bindingFlags.push_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);

          imageInfos.push_back(VkDescriptorImageInfo{
              .sampler = VK_NULL_HANDLE,
              .imageView = getImage(image->getName()),
              .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
          });
          writes.push_back(VkWriteDescriptorSet{
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstBinding = static_cast<uint32_t>(bindings.size() - 1),
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .pImageInfo = &imageInfos.back(),
          });
          if (image->getPhysicalId().used())
            builtResources.imageUsedInPass[image->getPhysicalId()].push_back(
                UsedInPass{.passIndex = idx,
                           .binding = binding,
                           .layout = VK_IMAGE_LAYOUT_GENERAL,
                           .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE});
        }
      }

      for (const auto& buffer : pass->getStorageOutputs()) {
        if (buffer && buffer->getPhysicalId().used()) {
          uint32_t binding = static_cast<uint32_t>(bindings.size());
          KT_DEBUG("Pass '{}' has storage buffer output '{}' at binding {}", pass->getName(), buffer->getName(), binding);
          bindings.push_back(VkDescriptorSetLayoutBinding{
              .binding = binding,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .descriptorCount = 1,
              .stageFlags = 0,
              .pImmutableSamplers = nullptr,
          });
          switch (pass->getQueue()) {
          case QueueType::Graphics:
            bindings.back().stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
          case QueueType::Compute:
          case QueueType::AsyncCompute:
            bindings.back().stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            break;
          }
          bindingFlags.push_back(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
          bufferInfos.push_back(VkDescriptorBufferInfo{
              .buffer = getBuffer(buffer->getName()),
              .offset = 0,
              .range = buffer->getBufferInfo().size,
          });
          writes.push_back(VkWriteDescriptorSet{
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstBinding = static_cast<uint32_t>(bindings.size() - 1),
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
              .pBufferInfo = &bufferInfos.back(),
          });
          if (buffer->getBufferInfo().isHostAccessible()) {
            perFrameWrites.push_back({.writeIndex = writes.size() - 1, .bufferIndex = bufferInfos.size() - 1});
            for (size_t i = 1; i < MAX_FRAMES_IN_FLIGHT; ++i) {
              bufferInfos.push_back(VkDescriptorBufferInfo{
                  .buffer = getBuffer(buffer->getName(), i),
                  .offset = 0,
                  .range = buffer->getBufferInfo().size,
              });
            }
          }
          if (buffer->getPhysicalId().used())
            builtResources.bufferUsedInPass[buffer->getPhysicalId()].push_back(
                UsedInPass{.passIndex = idx, .binding = binding, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER});
        }
      }

      VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
          .bindingCount = static_cast<uint32_t>(bindingFlags.size()),
          .pBindingFlags = bindingFlags.data(),
      };
      VkDescriptorSetLayoutCreateInfo layoutInfo{
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
          .pNext = &bindingFlagsInfo,
          .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
          .bindingCount = static_cast<uint32_t>(bindings.size()),
          .pBindings = bindings.data(),
      };
      VkDescriptorSetLayout layout = nullptr;
      KT_REQUIRE(vkCreateDescriptorSetLayout(members.vkcore.device, &layoutInfo, nullptr, &layout) == VK_SUCCESS,
                 "Failed to create descriptor set layout for pass '{}'", pass->getName());
      std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> sets{};
      std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, layout);
      VkDescriptorSetAllocateInfo allocInfo{
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
          .descriptorPool = descriptorPool,
          .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
          .pSetLayouts = layouts.data(),
      };
      KT_REQUIRE(vkAllocateDescriptorSets(members.vkcore.device, &allocInfo, sets.data()) == VK_SUCCESS,
                 "Failed to allocate descriptor sets for pass '{}'", pass->getName());

      for (const auto& [setIdx, set] : sets | std::views::enumerate) {
        for (auto& write : writes) {
          write.dstSet = set;
        }

        for (auto pfw : perFrameWrites) {
          writes[pfw.writeIndex].pBufferInfo = &bufferInfos[pfw.bufferIndex + static_cast<size_t>(setIdx)];
        }

        vkUpdateDescriptorSets(members.vkcore.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
      }

      passDescriptors.push_back(Descriptors{.layout = layout, .sets = sets});
    }
  }
#endif

  void RenderGraphBuilder::log() const {
    KT_DEBUG("Render Graph Log:");
    KT_DEBUG("  Passes (Used/Total): {}/{}", passStack.size(), passes.size());
    KT_DEBUG("  Resources (Phys/Virtual): {}/{}", physicalResourceInfos.size(), resources.size());
    KT_DEBUG("  Backbuffer Source: {}", backbufferSource);
    KT_DEBUG("");
    KT_DEBUG("  Pass Execution Order:");
    KT_DEBUG("    Idx: Name (Id)");
    for (const auto& [idx, passId] : passStack | std::views::enumerate) {
      const auto& pass = *passes[passId];
      KT_DEBUG("    {}: {} ({})", idx, pass.getName(), *passId);
    }
    KT_DEBUG("");
#if RENDERER_LOG_LEVEL <= SPDLOG_LEVEL_TRACE
    KT_TRACE("  Requirements:");
    for (const auto& [idx, passId] : passStack | std::views::enumerate) {
      const auto& pass = *passes[passId];
      const auto& reqs = passRequirements[idx];

      KT_TRACE("    {}: {} ({})", idx, pass.getName(), *passId);
      KT_TRACE("      Read Requirements: {}", reqs.invalidate.size());
      for (const auto& req : reqs.invalidate) {
        KT_TRACE("        Resource: {}{}", physicalResourceInfos[req.resourceId].name, req.history ? " (History)" : "");
        KT_TRACE("          Layout: {}", req.layout);
        KT_TRACE("          Access: {}", VkAccessFlags2Formatter(req.access));
        KT_TRACE("          Stages: {}", VkPipelineStageFlags2Formatter(req.stages));
      }
      KT_TRACE("");

      KT_TRACE("      Write Requirements: {}", reqs.flush.size());
      for (const auto& req : reqs.flush) {
        KT_TRACE("        Resource: {}{}", physicalResourceInfos[req.resourceId].name, req.history ? " (History)" : "");
        KT_TRACE("          Layout: {}", req.layout);
        KT_TRACE("          Access: {}", VkAccessFlags2Formatter(req.access));
        KT_TRACE("          Stages: {}", VkPipelineStageFlags2Formatter(req.stages));
      }
      KT_TRACE("");
    }
    KT_TRACE("  Barriers:");
    for (const auto& [idx, passId] : passStack | std::views::enumerate) {
      const auto& pass = *passes[passId];
      const auto& barriers = passBarriers[idx];

      KT_TRACE("    {}: {} ({})", idx, pass.getName(), *passId);
      KT_TRACE("      Pre-pass Image Barriers: {}", barriers.pre.image.size());
      for (const auto& barrier : barriers.pre.image) {
        const auto& res = physicalResourceInfos[barrier.resourceId];
        KT_TRACE("        Resource: {}", res.name);
        KT_TRACE("          Old Layout: {}", barrier.oldLayout);
        KT_TRACE("          New Layout: {}", barrier.newLayout);
        KT_TRACE("          Src Stages: {}", VkPipelineStageFlags2Formatter(barrier.srcStages));
        KT_TRACE("          Src Access: {}", VkAccessFlags2Formatter(barrier.srcAccess));
        KT_TRACE("          Dst Stages: {}", VkPipelineStageFlags2Formatter(barrier.dstStages));
        KT_TRACE("          Dst Access: {}", VkAccessFlags2Formatter(barrier.dstAccess));
        KT_TRACE("          Handoff: {}", barrier.handoff);
      }
      KT_TRACE("");

      KT_TRACE("      Pre-pass Buffer Barriers: {}", barriers.pre.buffer.size());
      for (const auto& barrier : barriers.pre.buffer) {
        const auto& res = physicalResourceInfos[barrier.resourceId];
        KT_TRACE("        Resource: {}", res.name);
        KT_TRACE("          Src Stages: {}", VkPipelineStageFlags2Formatter(barrier.srcStages));
        KT_TRACE("          Src Access: {}", VkAccessFlags2Formatter(barrier.srcAccess));
        KT_TRACE("          Dst Stages: {}", VkPipelineStageFlags2Formatter(barrier.dstStages));
        KT_TRACE("          Dst Access: {}", VkAccessFlags2Formatter(barrier.dstAccess));
        KT_TRACE("          Handoff: {}", barrier.handoff);
      }
      KT_TRACE("");

      KT_TRACE("      Post-pass Image Barriers: {}", barriers.post.image.size());
      for (const auto& barrier : barriers.post.image) {
        const auto& res = physicalResourceInfos[barrier.resourceId];
        KT_TRACE("        Resource: {}", res.name);
        KT_TRACE("          Old Layout: {}", barrier.oldLayout);
        KT_TRACE("          New Layout: {}", barrier.newLayout);
        KT_TRACE("          Src Stages: {}", VkPipelineStageFlags2Formatter(barrier.srcStages));
        KT_TRACE("          Src Access: {}", VkAccessFlags2Formatter(barrier.srcAccess));
        KT_TRACE("          Dst Stages: {}", VkPipelineStageFlags2Formatter(barrier.dstStages));
        KT_TRACE("          Dst Access: {}", VkAccessFlags2Formatter(barrier.dstAccess));
        KT_TRACE("          Handoff: {}", barrier.handoff);
      }
      KT_TRACE("");

      KT_TRACE("      Post-pass Buffer Barriers: {}", barriers.post.buffer.size());
      for (const auto& barrier : barriers.post.buffer) {
        const auto& res = physicalResourceInfos[barrier.resourceId];
        KT_TRACE("        Resource: {}", res.name);
        KT_TRACE("          Src Stages: {}", VkPipelineStageFlags2Formatter(barrier.srcStages));
        KT_TRACE("          Src Access: {}", VkAccessFlags2Formatter(barrier.srcAccess));
        KT_TRACE("          Dst Stages: {}", VkPipelineStageFlags2Formatter(barrier.dstStages));
        KT_TRACE("          Dst Access: {}", VkAccessFlags2Formatter(barrier.dstAccess));
        KT_TRACE("          Handoff: {}", barrier.handoff);
      }
      KT_TRACE("");
    }
#endif
  }

  void RenderGraphBuilder::validatePasses() const {
    for (const auto& passPtr : passes) {
      const auto& pass = *passPtr;

      KT_REQUIRE(pass.getColorOutputs().size() == pass.getColorInputs().size(), "Pass '{}': Size of color inputs and outputs must match",
                 pass.getName());

      KT_REQUIRE(pass.getStorageImageOutputs().size() == pass.getStorageImageInputs().size(),
                 "Pass '{}': Size of storage image inputs and outputs must match", pass.getName());

      KT_REQUIRE(pass.getStorageInputs().size() == pass.getStorageOutputs().size(),
                 "Pass '{}': Size of storage inputs and outputs must match", pass.getName());

      for (const auto& [idx, output] : pass.getColorOutputs() | std::views::enumerate) {
        auto* input = pass.getColorInputs()[static_cast<size_t>(idx)];
        if (!input)
          continue;

        KT_REQUIRE(
            input->getAttachmentInfo().format != KT_FORMAT_UNDEFINED,
            "Pass '{}': Color input '{}' must have a defined format. You may have forgotten to add this image as an output elsewhere.",
            pass.getName(), input->getName());

        KT_REQUIRE(output->getAttachmentInfo().format == input->getAttachmentInfo().format,
                   "Pass '{}': Color output '{}' and input '{}' must have the same format. {} vs {}", pass.getName(), output->getName(),
                   input->getName(), output->getAttachmentInfo().format, input->getAttachmentInfo().format);
        KT_REQUIRE(output->getAttachmentInfo().samples == input->getAttachmentInfo().samples,
                   "Pass '{}': Color output '{}' and input '{}' must have the same sample count", pass.getName(), output->getName(),
                   input->getName());
        KT_REQUIRE(output->getAttachmentInfo().layers == input->getAttachmentInfo().layers,
                   "Pass '{}': Color output '{}' and input '{}' must have the same layer count", pass.getName(), output->getName(),
                   input->getName());
        KT_REQUIRE(output->getAttachmentInfo().mipLevels == input->getAttachmentInfo().mipLevels,
                   "Pass '{}': Color output '{}' and input '{}' must have the same mip level count", pass.getName(), output->getName(),
                   input->getName());
        KT_REQUIRE(output->getAttachmentInfo().sizeType == input->getAttachmentInfo().sizeType,
                   "Pass '{}': Color output '{}' and input '{}' must have the same size type", pass.getName(), output->getName(),
                   input->getName());
        KT_REQUIRE(output->getAttachmentInfo().size == input->getAttachmentInfo().size,
                   "Pass '{}': Color output '{}' and input '{}' must have the same size", pass.getName(), output->getName(),
                   input->getName());
      }

      for (const auto& [idx, output] : pass.getStorageImageOutputs() | std::views::enumerate) {
        auto* input = pass.getStorageImageInputs()[static_cast<size_t>(idx)];
        if (!input)
          continue;

        KT_REQUIRE(output->getAttachmentInfo().format == input->getAttachmentInfo().format,
                   "Pass '{}': Storage image output '{}' and input '{}' must have the same format", pass.getName(), output->getName(),
                   input->getName());
        KT_REQUIRE(output->getAttachmentInfo().samples == input->getAttachmentInfo().samples,
                   "Pass '{}': Storage image output '{}' and input '{}' must have the same sample count", pass.getName(), output->getName(),
                   input->getName());
        KT_REQUIRE(output->getAttachmentInfo().layers == input->getAttachmentInfo().layers,
                   "Pass '{}': Storage image output '{}' and input '{}' must have the same layer count", pass.getName(), output->getName(),
                   input->getName());
        KT_REQUIRE(output->getAttachmentInfo().mipLevels == input->getAttachmentInfo().mipLevels,
                   "Pass '{}': Storage image output '{}' and input '{}' must have the same mip level count", pass.getName(),
                   output->getName(), input->getName());
        KT_REQUIRE(output->getAttachmentInfo().sizeType == input->getAttachmentInfo().sizeType,
                   "Pass '{}': Storage image output '{}' and input '{}' must have the same size type", pass.getName(), output->getName(),
                   input->getName());
        KT_REQUIRE(output->getAttachmentInfo().size == input->getAttachmentInfo().size,
                   "Pass '{}': Storage image output '{}' and input '{}' must have the same size", pass.getName(), output->getName(),
                   input->getName());
      }

      for (const auto& [idx, output] : pass.getStorageOutputs() | std::views::enumerate) {
        auto* input = pass.getStorageInputs()[static_cast<size_t>(idx)];
        if (!input)
          continue;

        KT_REQUIRE(output->getBufferInfo().size == input->getBufferInfo().size,
                   "Pass '{}': Storage output '{}' and input '{}' must have the same size", pass.getName(), output->getName(),
                   input->getName());
#ifdef KT_VULKAN
        KT_REQUIRE(output->getBufferInfo().usage == input->getBufferInfo().usage,
                   "Pass '{}': Storage output '{}' and input '{}' must have the same usage flags", pass.getName(), output->getName(),
                   input->getName());
#endif
      }

      if (pass.getDepthStencilInput() && pass.getDepthStencilOutput()) {
        auto* input = pass.getDepthStencilInput();
        auto* output = pass.getDepthStencilOutput();

        KT_REQUIRE(input->getAttachmentInfo().format == output->getAttachmentInfo().format,
                   "Pass '{}': Depth-stencil input '{}' and output '{}' must have the same format", pass.getName(), input->getName(),
                   output->getName());
        KT_REQUIRE(input->getAttachmentInfo().samples == output->getAttachmentInfo().samples,
                   "Pass '{}': Depth-stencil input '{}' and output '{}' must have the same sample count", pass.getName(), input->getName(),
                   output->getName());
        KT_REQUIRE(input->getAttachmentInfo().layers == output->getAttachmentInfo().layers,
                   "Pass '{}': Depth-stencil input '{}' and output '{}' must have the same layer count", pass.getName(), input->getName(),
                   output->getName());
        KT_REQUIRE(input->getAttachmentInfo().mipLevels == output->getAttachmentInfo().mipLevels,
                   "Pass '{}': Depth-stencil input '{}' and output '{}' must have the same mip level count", pass.getName(),
                   input->getName(), output->getName());
        KT_REQUIRE(input->getAttachmentInfo().sizeType == output->getAttachmentInfo().sizeType,
                   "Pass '{}': Depth-stencil input '{}' and output '{}' must have the same size type", pass.getName(), input->getName(),
                   output->getName());
        KT_REQUIRE(input->getAttachmentInfo().size == output->getAttachmentInfo().size,
                   "Pass '{}': Depth-stencil input '{}' and output '{}' must have the same size", pass.getName(), input->getName(),
                   output->getName());
      }
    }
  }

  void RenderGraphBuilder::traverseDependencies(const RenderPassBuilder& pass, size_t stackCount) {
    if (pass.getDepthStencilInput()) {
      dependPassesRecursive(pass, pass.getDepthStencilInput()->getWritePasses(), stackCount, false, true);
    }

    for (auto* input : pass.getColorInputs()) {
      if (input)
        dependPassesRecursive(pass, input->getWritePasses(), stackCount, false, false);
    }

    for (auto& input : pass.getGenericTextureInputs()) {
      dependPassesRecursive(pass, input.texture->getWritePasses(), stackCount, false, false);
    }

    for (auto* input : pass.getStorageImageInputs()) {
      if (input) {
        dependPassesRecursive(pass, input->getWritePasses(), stackCount, true, false);
        dependPassesRecursive(pass, input->getReadPasses(), stackCount, true, true);
      }
    }

    for (auto* input : pass.getStorageInputs()) {
      if (input) {
        dependPassesRecursive(pass, input->getWritePasses(), stackCount, true, false);
        dependPassesRecursive(pass, input->getReadPasses(), stackCount, true, true);
      }
    }

    for (auto& input : pass.getGenericBufferInputs()) {
      auto mappedIt = std::find(pass.getMappedBuffers().begin(), pass.getMappedBuffers().end(), input.buffer);
      dependPassesRecursive(
          pass, input.buffer->getWritePasses(), stackCount, true,
          mappedIt != pass.getMappedBuffers().end()); // Ignore self if this buffer is mapped, as it will be written to by the pass itself.
    }

    dependPassesRecursive(pass, pass.getProxyPasses(), stackCount, true, false);
  }

  void RenderGraphBuilder::dependPassesRecursive(const RenderPassBuilder& self, const std::unordered_set<PassId>& writtenPasses,
                                                 size_t stackCount, bool noCheck, bool ignoreSelf) {
    KT_REQUIRE(noCheck || !writtenPasses.empty(), "Pass '{}': No passes found that write to the input resource", self.getName());

    KT_REQUIRE(stackCount <= passes.size(), "Pass '{}': Circular dependency detected in render graph", self.getName());

    for (auto& pass : writtenPasses)
      if (pass != self.getId())
        passDependencies[self.getId()].insert(pass);

    ++stackCount;

    for (auto& pass : writtenPasses) {

      if (ignoreSelf && pass == self.getId())
        continue;
      else
        KT_REQUIRE(pass != self.getId(), "Pass '{}' depends on itself", self.getName());

      passStack.push_back(pass);
      auto& depPass = *passes[pass];
      traverseDependencies(depPass, stackCount);
    }
  }

  void RenderGraphBuilder::filterPasses(std::vector<PassId>& passOrder) {
    std::unordered_set<PassId> seen;

    auto [first, last] = std::ranges::remove_if(passOrder, [&](PassId id) {
      if (seen.contains(id))
        return true;

      seen.insert(id);
      return false;
    });
    passOrder.erase(first, last);
  }

  void RenderGraphBuilder::reorderPasses(std::vector<PassId>& passOrder) {
    if (passOrder.size() <= 2)
      return;

    std::vector<PassId> unscheduled;
    unscheduled.reserve(passOrder.size());
    std::swap(passOrder, unscheduled);

    const auto& schedule = [&](size_t i) {
      passOrder.push_back(unscheduled[i]);
      std::move(unscheduled.begin() + static_cast<std::vector<PassId>::iterator::difference_type>(i) + 1, unscheduled.end(),
                unscheduled.begin() + static_cast<std::vector<PassId>::iterator::difference_type>(i));
      unscheduled.pop_back();
    };

    schedule(PassId(0));

    while (!unscheduled.empty()) {
      size_t best = 0;
      size_t bestOverlap = 0;

      for (const auto& [idxL, pass] : unscheduled | std::views::enumerate) {
        size_t overlap = 0;
        size_t idx = static_cast<size_t>(idxL);

        for (auto& candidate : passOrder | std::views::reverse) {
          if (dependsOnPass(pass, candidate))
            break;

          ++overlap;
        }

        if (overlap < bestOverlap)
          continue;

        bool possible = true;

        for (size_t j = 0; j < idx; ++j) {
          if (dependsOnPass(pass, unscheduled[j])) {
            possible = false;
            break;
          }
        }

        if (!possible)
          continue;

        best = idx;
        bestOverlap = overlap;
      }

      schedule(best);
    }
  }

  void RenderGraphBuilder::buildPhysicalResources() {
    PhysResourceId physId{0};

    auto postBufferId = [&](bool host) {
      if (host) {
        for (size_t i = 0; i < (MAX_FRAMES_IN_FLIGHT - 1); ++i) {
          physicalResourceInfos.push_back(physicalResourceInfos.back());
        }
        physId += (MAX_FRAMES_IN_FLIGHT - 1);
      }
    };

    auto updateBuffer = [&](PhysResourceId id, Bitflag<QueueType> queues,
#ifdef KT_VULKAN
                            VkBufferUsageFlags usage,
#endif
                            bool host) {
      for (size_t i = 0; i < (host ? MAX_FRAMES_IN_FLIGHT : 1); ++i) {
        physicalResourceInfos[id + i].queues |= queues;
#ifdef KT_VULKAN
        physicalResourceInfos[id + i].bufferInfo.usage |= usage;
#endif
      }
    };

    for (const auto& [idx, passId] : passStack | std::views::enumerate) {
      auto& pass = *passes[passId];
      for (auto& input : pass.getGenericTextureInputs()) {
        if (!input.texture->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*input.texture));
          input.texture->setPhysicalId(physId++);
        } else {
          physicalResourceInfos[input.texture->getPhysicalId()].queues |= input.texture->getUsedQueues();
#ifdef KT_VULKAN
          physicalResourceInfos[input.texture->getPhysicalId()].imageUsage |= input.texture->getImageUsage();
#endif
        }
      }

      for (auto& input : pass.getGenericBufferInputs()) {
        bool host = input.buffer->getBufferInfo().isHostAccessible();
        if (!input.buffer->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*input.buffer));
          input.buffer->setPhysicalId(physId++);
          postBufferId(host);
        } else {
          updateBuffer(input.buffer->getPhysicalId(), input.buffer->getUsedQueues(),
#ifdef KT_VULKAN
                       input.buffer->getBufferUsage(),
#endif
                       host);
        }
      }

      for (const auto& [jdxL, inputPtr] : pass.getColorInputs() | std::views::enumerate) {
        if (!inputPtr)
          continue;
        size_t jdx = static_cast<size_t>(jdxL);
        auto& input = *inputPtr;

        if (!input.getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(input));
          input.setPhysicalId(physId++);
        } else {
          physicalResourceInfos[input.getPhysicalId()].queues |= input.getUsedQueues();
#ifdef KT_VULKAN
          physicalResourceInfos[input.getPhysicalId()].imageUsage |= input.getImageUsage();
#endif
        }

        KT_REQUIRE(!pass.getColorOutputs()[jdx]->getPhysicalId().used(),
                   "Pass '{}': Cannot alias color output '{}'. Physical ID already claimed.", pass.getName(),
                   pass.getColorOutputs()[jdx]->getName());
        KT_DEBUG("Pass '{}': Aliasing color output '{}' to input '{}'.", pass.getName(), pass.getColorOutputs()[jdx]->getName(),
                 input.getName());
        pass.getColorOutputs()[jdx]->setPhysicalId(input.getPhysicalId());
      }

      for (const auto& [jdxL, inputPtr] : pass.getStorageImageInputs() | std::views::enumerate) {
        if (!inputPtr)
          continue;
        size_t jdx = static_cast<size_t>(jdxL);
        auto& input = *inputPtr;

        if (!input.getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(input));
          input.setPhysicalId(physId++);
        } else {
          physicalResourceInfos[input.getPhysicalId()].queues |= input.getUsedQueues();
#ifdef KT_VULKAN
          physicalResourceInfos[input.getPhysicalId()].imageUsage |= input.getImageUsage();
#endif
        }

        KT_REQUIRE(!pass.getStorageImageOutputs()[jdx]->getPhysicalId().used(),
                   "Pass '{}': Cannot alias storage image output '{}'. Physical ID already claimed.", pass.getName(),
                   pass.getStorageImageOutputs()[jdx]->getName());
        KT_DEBUG("Pass '{}': Aliasing storage image output '{}' to input '{}'.", pass.getName(),
                 pass.getStorageImageOutputs()[jdx]->getName(), input.getName());
        pass.getStorageImageOutputs()[jdx]->setPhysicalId(input.getPhysicalId());
      }

      for (const auto& [jdxL, inputPtr] : pass.getStorageInputs() | std::views::enumerate) {
        if (!inputPtr)
          continue;
        auto& input = *inputPtr;
        size_t jdx = static_cast<size_t>(jdxL);

        bool host = input.getBufferInfo().isHostAccessible();

        if (!input.getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(input));
          input.setPhysicalId(physId++);
          postBufferId(host);
        } else {
          updateBuffer(input.getPhysicalId(), input.getUsedQueues(),
#ifdef KT_VULKAN
                       input.getBufferUsage(),
#endif
                       host);
        }

        KT_REQUIRE(!pass.getStorageOutputs()[jdx]->getPhysicalId().used(),
                   "Pass '{}': Cannot alias storage output '{}'. Physical ID already claimed.", pass.getName(),
                   pass.getStorageOutputs()[jdx]->getName());
        KT_DEBUG("Pass '{}': Aliasing storage output '{}' to input '{}'.", pass.getName(), pass.getStorageOutputs()[jdx]->getName(),
                 input.getName());
        pass.getStorageOutputs()[jdx]->setPhysicalId(input.getPhysicalId());
      }

      for (auto* output : pass.getColorOutputs()) {
        if (!output->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*output));
          output->setPhysicalId(physId++);
        } else {
          physicalResourceInfos[output->getPhysicalId()].queues |= output->getUsedQueues();
#ifdef KT_VULKAN
          physicalResourceInfos[output->getPhysicalId()].imageUsage |= output->getImageUsage();
#endif
        }
      }

      for (auto* output : pass.getStorageImageOutputs()) {
        if (!output->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*output));
          output->setPhysicalId(physId++);
        } else {
          physicalResourceInfos[output->getPhysicalId()].queues |= output->getUsedQueues();
#ifdef KT_VULKAN
          physicalResourceInfos[output->getPhysicalId()].imageUsage |= output->getImageUsage();
#endif
        }
      }

      for (auto* output : pass.getStorageOutputs()) {
        bool host = output->getBufferInfo().isHostAccessible();
        if (!output->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*output));
          output->setPhysicalId(physId++);
          postBufferId(host);
        } else {
          updateBuffer(output->getPhysicalId(), output->getUsedQueues(),
#ifdef KT_VULKAN
                       output->getBufferUsage(),
#endif
                       host);
        }
      }

      for (auto* mapped : pass.getMappedBuffers()) {
        bool host = mapped->getBufferInfo().isHostAccessible();
        if (!mapped->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*mapped));
          mapped->setPhysicalId(physId++);
          postBufferId(host);
        } else {
          updateBuffer(mapped->getPhysicalId(), mapped->getUsedQueues(),
#ifdef KT_VULKAN
                       mapped->getBufferUsage(),
#endif
                       host);
        }
      }

      for (auto* output : pass.getTransferOutputs()) {
        bool host = output->getBufferInfo().isHostAccessible();
        if (!output->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*output));
          output->setPhysicalId(physId++);
          postBufferId(host);
        } else {
          updateBuffer(output->getPhysicalId(), output->getUsedQueues(),
#ifdef KT_VULKAN
                       output->getBufferUsage(),
#endif
                       host);
        }
      }

      auto* dsInput = pass.getDepthStencilInput();
      auto* dsOutput = pass.getDepthStencilOutput();
      if (dsInput) {
        if (!dsInput->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*dsInput));
          dsInput->setPhysicalId(physId++);
        } else {
          physicalResourceInfos[dsInput->getPhysicalId()].queues |= dsInput->getUsedQueues();
#ifdef KT_VULKAN
          physicalResourceInfos[dsInput->getPhysicalId()].imageUsage |= dsInput->getImageUsage();
#endif
        }

        if (dsOutput) {
          KT_REQUIRE(!dsOutput->getPhysicalId().used(), "Pass '{}': Cannot alias depth-stencil output '{}'. Physical ID already claimed.",
                     pass.getName(), dsOutput->getName());
          KT_DEBUG("Pass '{}': Aliasing depth-stencil output '{}' to input '{}'.", pass.getName(), dsOutput->getName(), dsInput->getName());
          dsOutput->setPhysicalId(dsInput->getPhysicalId());

          physicalResourceInfos[dsInput->getPhysicalId()].queues |= dsOutput->getUsedQueues();
#ifdef KT_VULKAN
          physicalResourceInfos[dsInput->getPhysicalId()].imageUsage |= dsOutput->getImageUsage();
#endif
        }
      } else if (dsOutput) {
        if (!dsOutput->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*dsOutput));
          dsOutput->setPhysicalId(physId++);
        } else {
          physicalResourceInfos[dsOutput->getPhysicalId()].queues |= dsOutput->getUsedQueues();
#ifdef KT_VULKAN
          physicalResourceInfos[dsOutput->getPhysicalId()].imageUsage |= dsOutput->getImageUsage();
#endif
        }
      }
    }

    {
      auto backbufferIt = resourceNameToId.find(backbufferSource);
      KT_REQUIRE(backbufferIt != resourceNameToId.end(), "Backbuffer source '{}' not found in render graph", backbufferSource);
#ifdef KT_VULKAN
      auto& backbufferVirt = *resources[backbufferIt->second];
      auto& backbufferPhys = physicalResourceInfos[backbufferVirt.getPhysicalId()];

      backbufferPhys.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
#endif
    }

    physicalImageHasHistory.clear();
    physicalImageHasHistory.resize(physicalResourceInfos.size());

    for (auto& passId : passStack) {
      auto& pass = *passes[passId];

      for (auto& history : pass.getHistoryInputs()) {
        KT_REQUIRE(history->getPhysicalId().used(), "Pass '{}': History input '{}' is used, but is never written to.", pass.getName(),
                   history->getName());
        physicalImageHasHistory[history->getPhysicalId()] = true;
      }
    }
  }

  void RenderGraphBuilder::buildRequirements() {
    passRequirements.clear();
    passRequirements.reserve(passStack.size());

    const auto getAccess = [&](std::vector<Requirement>& req, PhysResourceId id, bool history) -> Requirement& {
      auto it = std::ranges::find_if(req, [&](const Requirement& b) { return b.resourceId == id && history == b.history; });
      if (it != req.end())
        return *it;

      req.push_back({.resourceId = id,
                     .layout = KT_IMAGE_LAYOUT_UNDEFINED,
#ifdef KT_VULKAN
                     .access = 0,
                     .stages = 0,
#endif
                     .history = history});
      return req.back();
    };

    for (auto& passId : passStack) {
      auto& pass = *passes[passId];
      Requirements reqs;

      const auto getInvalidAccess = [&](PhysResourceId id, bool history) -> Requirement& {
        return getAccess(reqs.invalidate, id, history);
      };

      const auto getFlushAccess = [&](PhysResourceId id) -> Requirement& { return getAccess(reqs.flush, id, false); };

      for (auto& input : pass.getGenericBufferInputs()) {
        auto& barrier = getInvalidAccess(input.buffer->getPhysicalId(), false);
#ifdef KT_VULKAN
        barrier.access |= input.access;
        barrier.stages |= input.stages;
#endif
        KT_REQUIRE(barrier.layout == KT_IMAGE_LAYOUT_UNDEFINED,
                   "Pass '{}': Buffer input '{}' expected to have undefined layout. Has {}. You have probably added this resource to this "
                   "pass multiple times.",
                   pass.getName(), input.buffer->getName(), barrier.layout);
        barrier.layout = input.layout;
      }

      for (auto& input : pass.getGenericTextureInputs()) {
        auto& req = getInvalidAccess(input.texture->getPhysicalId(), false);
#ifdef KT_VULKAN
        req.access |= input.access;
        req.stages |= input.stages;
#endif
        KT_REQUIRE(req.layout == KT_IMAGE_LAYOUT_UNDEFINED,
                   "Pass '{}': Texture input '{}' expected to have undefined layout. Has {}. You have probably added this resource to this "
                   "pass multiple times.",
                   pass.getName(), input.texture->getName(), req.layout);
        req.layout = input.layout;
      }

      for (auto& input : pass.getHistoryInputs()) {
        auto& req = getInvalidAccess(input->getPhysicalId(), true);
#ifdef KT_VULKAN
        req.access |= VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;

        if (!COMPUTE_QUEUES.intersects(pass.getQueue())) {
          req.stages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        } else {
          req.stages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }
#endif

        KT_REQUIRE(req.layout == KT_IMAGE_LAYOUT_UNDEFINED,
                   "Pass '{}': History input '{}' expected to have undefined layout. Has {}. You have probably added this resource to this "
                   "pass multiple times.",
                   pass.getName(), input->getName(), req.layout);
        req.layout = KT_IMAGE_LAYOUT_SHADER_READ;
      }

      for (auto* input : pass.getStorageImageInputs()) {
        if (!input)
          continue;
        auto& req = getInvalidAccess(input->getPhysicalId(), false);
#ifdef KT_VULKAN
        req.access |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

        if (!COMPUTE_QUEUES.intersects(pass.getQueue())) {
          req.stages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        } else {
          req.stages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }
#endif

        KT_REQUIRE(
            req.layout == KT_IMAGE_LAYOUT_UNDEFINED,
            "Pass '{}': Storage image input '{}' expected to have undefined layout. Has {}. You have probably added this resource to "
            "this pass multiple times.",
            pass.getName(), input->getName(), req.layout);

        req.layout = KT_IMAGE_LAYOUT_SHADER_STORAGE_READ_WRITE;
      }

      for (auto* input : pass.getStorageInputs()) {
        if (!input)
          continue;
        auto& req = getInvalidAccess(input->getPhysicalId(), false);
#ifdef KT_VULKAN
        req.access |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

        if (!COMPUTE_QUEUES.intersects(pass.getQueue())) {
          req.stages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        } else {
          req.stages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }
#endif

        KT_REQUIRE(req.layout == KT_IMAGE_LAYOUT_UNDEFINED,
                   "Pass '{}': Storage input '{}' expected to have undefined layout. Has {}. You have probably added this resource to this "
                   "pass multiple times.",
                   pass.getName(), input->getName(), req.layout);
        req.layout = KT_IMAGE_LAYOUT_SHADER_STORAGE_READ_WRITE;
      }

      for (auto* input : pass.getColorInputs()) {
        if (!input)
          continue;

        KT_REQUIRE(!COMPUTE_QUEUES.intersects(pass.getQueue()), "Pass '{}': Color inputs cannot be used in a compute pass", pass.getName(),
                   input->getName());

        auto& req = getInvalidAccess(input->getPhysicalId(), false);
#ifdef KT_VULKAN
        req.access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        req.stages |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
#endif

        // Also used as an input attachment for programmable blending.
        if (req.layout == KT_IMAGE_LAYOUT_SHADER_READ) {
          KT_WARN("Pass '{}': Color input '{}' is used as a shader read-only input and as a color attachment. The layout of the texture "
                  "for this pass has been set to general. This is not guarenteed to work.",
                  pass.getName(), input->getName());
          req.layout = KT_IMAGE_LAYOUT_SHADER_READ_ATTACHMENT;
        } else {
          KT_REQUIRE(req.layout == KT_IMAGE_LAYOUT_UNDEFINED,
                     "Pass '{}': Color input '{}' expected to have undefined layout. Has {}. You have probably added this resource to this "
                     "pass multiple times.",
                     pass.getName(), input->getName(), req.layout);
          req.layout = KT_IMAGE_LAYOUT_COLOR_ATTACHMENT;
        }
      }

      for (auto* output : pass.getColorOutputs()) {
        KT_REQUIRE(!COMPUTE_QUEUES.intersects(pass.getQueue()), "Pass '{}': Color outputs cannot be used in a compute pass", pass.getName(),
                   output->getName());

        auto& req = getFlushAccess(output->getPhysicalId());
#ifdef KT_VULKAN
        req.access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        req.stages |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
#endif

        if (req.layout == KT_IMAGE_LAYOUT_SHADER_READ || req.layout == KT_IMAGE_LAYOUT_SHADER_READ_ATTACHMENT) {
          KT_WARN("Pass '{}': Color output '{}' is used as a shader read-only input and as a color attachment. The layout of the texture "
                  "for this pass has been set to general. This is not guarenteed to work.",
                  pass.getName(), output->getName());
          req.layout = KT_IMAGE_LAYOUT_SHADER_READ_ATTACHMENT;
        } else {
          KT_REQUIRE(
              req.layout == KT_IMAGE_LAYOUT_UNDEFINED,
              "Pass '{}': Color output '{}' expected to have undefined layout. Has {}. You have probably added this resource to this "
              "pass multiple times.",
              pass.getName(), output->getName(), req.layout);
          req.layout = KT_IMAGE_LAYOUT_COLOR_ATTACHMENT;
        }
      }

      for (auto* output : pass.getStorageImageOutputs()) {
        auto& req = getFlushAccess(output->getPhysicalId());
#ifdef KT_VULKAN
        req.access |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

        if (!COMPUTE_QUEUES.intersects(pass.getQueue())) {
          req.stages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        } else {
          req.stages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }
#endif

        KT_REQUIRE(req.layout == KT_IMAGE_LAYOUT_UNDEFINED,
                   "Pass '{}': Storage image output '{}' expected to have undefined layout. Has {}. You have probably added this "
                   "resource to this "
                   "pass multiple times.",
                   pass.getName(), output->getName(), req.layout);
        req.layout = KT_IMAGE_LAYOUT_SHADER_STORAGE_READ_WRITE;
      }

      for (auto* output : pass.getStorageOutputs()) {
        auto& req = getFlushAccess(output->getPhysicalId());
#ifdef KT_VULKAN
        req.access |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

        if (!COMPUTE_QUEUES.intersects(pass.getQueue())) {
          req.stages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        } else {
          req.stages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }
#endif

        KT_REQUIRE(
            req.layout == KT_IMAGE_LAYOUT_UNDEFINED,
            "Pass '{}': Storage output '{}' expected to have undefined layout. Has {}. You have probably added this resource to this "
            "pass multiple times.",
            pass.getName(), output->getName(), req.layout);
        req.layout = KT_IMAGE_LAYOUT_SHADER_STORAGE_READ_WRITE;
      }

      for (auto* output : pass.getTransferOutputs()) {
        auto& req = getFlushAccess(output->getPhysicalId());
#ifdef KT_VULKAN
        req.access |= VK_ACCESS_TRANSFER_WRITE_BIT;
        req.stages |= VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT;
#endif
        KT_REQUIRE(req.layout == KT_IMAGE_LAYOUT_UNDEFINED,
                   "Pass '{}': Transfer output '{}' expected to have undefined layout. Has {}. You have probably added this resource to "
                   "this pass multiple times.",
                   pass.getName(), output->getName(), req.layout);
        req.layout = KT_IMAGE_LAYOUT_SHADER_STORAGE_READ_WRITE;
      }

      auto* dsInput = pass.getDepthStencilInput();
      auto* dsOutput = pass.getDepthStencilOutput();

      if (dsInput || dsOutput) {
        KT_REQUIRE(!COMPUTE_QUEUES.intersects(pass.getQueue()), "Pass '{}': Depth-stencil inputs/outputs cannot be used in a compute pass",
                   pass.getName(), dsInput ? dsInput->getName() : dsOutput->getName());
      }

      if (dsInput && dsOutput) {
        auto& dstReq = getInvalidAccess(dsInput->getPhysicalId(), false);
        auto& srcReq = getFlushAccess(dsOutput->getPhysicalId());

        if (dstReq.layout == KT_IMAGE_LAYOUT_SHADER_READ) {
          KT_DEBUG("Pass '{}': Depth-stencil texture '{}' is used as a shader read-only input and as a depth stencil attachment."
                   "The layout of the texture for this pass has been set to general. You should probably avoid this, or enable the "
                   "VK_KHR_unified_image_layout extension.",
                   pass.getName(), dsInput->getName(), dsOutput->getName());
          dstReq.layout = KT_IMAGE_LAYOUT_SHADER_READ_DEPTH_ATTACHMENT;
          pass.setDepthStencilLayout(KT_IMAGE_LAYOUT_SHADER_READ_DEPTH_ATTACHMENT);
        } else {
          KT_REQUIRE(dstReq.layout == KT_IMAGE_LAYOUT_UNDEFINED,
                     "Pass '{}': Depth-stencil output '{}' expected to have undefined layout. Has {}. You have probably added this "
                     "resource to this pass multiple times.",
                     pass.getName(), dsOutput->getName(), dstReq.layout);
          dstReq.layout = KT_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT;
          pass.setDepthStencilLayout(KT_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT);
        }

#ifdef KT_VULKAN
        dstReq.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dstReq.stages |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;

        srcReq.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        srcReq.stages |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
#endif
        srcReq.layout = KT_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT;
      } else if (dsInput) {
        auto& dstReq = getInvalidAccess(dsInput->getPhysicalId(), false);

        if (dstReq.layout == KT_IMAGE_LAYOUT_SHADER_READ) {
          KT_DEBUG("Pass '{}': Depth-stencil texture '{}' is used as a shader read-only input and as an input depth stencil attachment."
                   "The layout of the texture for this pass has been set to depth stencil read only optimal.",
                   pass.getName(), dsInput->getName());
          dstReq.layout = KT_IMAGE_LAYOUT_SHADER_READ_DEPTH_READ_ATTACHMENT;
          pass.setDepthStencilLayout(KT_IMAGE_LAYOUT_SHADER_READ_DEPTH_READ_ATTACHMENT);
        } else {
          KT_REQUIRE(dstReq.layout == KT_IMAGE_LAYOUT_UNDEFINED,
                     "Pass '{}': Depth-stencil input '{}' expected to have undefined layout. Has {}. You have probably added this "
                     "resource to this pass multiple times.",
                     pass.getName(), dsInput->getName(), dstReq.layout);
          dstReq.layout = KT_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT;
          pass.setDepthStencilLayout(KT_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT);
        }

#ifdef KT_VULKAN
        dstReq.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dstReq.stages |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
#endif
      } else if (dsOutput) {
        auto& srcReq = getFlushAccess(dsOutput->getPhysicalId());

        if (srcReq.layout == KT_IMAGE_LAYOUT_SHADER_READ) {
          KT_DEBUG("Pass '{}': Depth-stencil texture '{}' is used as a shader read-only input and as an output depth stencil attachment."
                   "The layout of the texture for this pass has been set to general. You should probably avoid this, or enable the "
                   "VK_KHR_unified_image_layout extension.",
                   pass.getName(), dsOutput->getName());
          srcReq.layout = KT_IMAGE_LAYOUT_SHADER_READ_DEPTH_ATTACHMENT;
          pass.setDepthStencilLayout(KT_IMAGE_LAYOUT_SHADER_READ_DEPTH_ATTACHMENT);
        } else {
          KT_REQUIRE(srcReq.layout == KT_IMAGE_LAYOUT_UNDEFINED,
                     "Pass '{}': Depth-stencil output '{}' expected to have undefined layout. Has {}. You have probably added this "
                     "resource to this pass multiple times.",
                     pass.getName(), dsOutput->getName(), srcReq.layout);
          srcReq.layout = KT_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT;
          pass.setDepthStencilLayout(KT_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT);
        }

#ifdef KT_VULKAN
        srcReq.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        srcReq.stages |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
#endif
      }

      passRequirements.push_back(std::move(reqs));
    }
  }

  namespace {
    struct ResInfo {
      ImageLayout layout{KT_IMAGE_LAYOUT_UNDEFINED};
#ifdef KT_VULKAN
      VkAccessFlags2 access{VK_ACCESS_2_NONE};
      VkPipelineStageFlags2 stages{VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT};
#endif
      QueueType queue{0};
      size_t lastUsedInPass = ~0u;
    };

    template <typename T> bool checkHandoff(const RenderPassBuilder& pass, T& barrier, ResInfo& resInfo) {
      if (resInfo.queue == static_cast<QueueType>(0)) {
        resInfo.queue = pass.getQueue();
      } else if (pass.getQueue() == QueueType::AsyncCompute && resInfo.queue != QueueType::AsyncCompute) {
        barrier.handoff = QueueHandoff::ToCompute;
        resInfo.queue = QueueType::AsyncCompute;
        return true;
      } else if (pass.getQueue() != QueueType::AsyncCompute && resInfo.queue == QueueType::AsyncCompute) {
        barrier.handoff = QueueHandoff::FromCompute;
        resInfo.queue = pass.getQueue();
        return true;
      }
      return false;
    }
  } // namespace

  void RenderGraphBuilder::buildBarriers() {
    // TODO: Handle history resources properly. Currently will be wiped at frame start like any other.

    passBarriers.clear();
    passBarriers.reserve(passStack.size());

    std::vector<ResInfo> resInfos;
    resInfos.resize(physicalResourceInfos.size());

    for (const auto& [idx, passId] : passStack | std::views::enumerate) {
      auto& pass = *passes[passId];
      auto& reqs = passRequirements[static_cast<size_t>(idx)];

      PrePostBarriers barriers;

      auto makeBarrier = [&](const Requirement& req) {
        auto& res = physicalResourceInfos[req.resourceId];
        auto& resInfo = resInfos[req.resourceId];

        if (res.isLayoutSensitive()) {
          ImageBarrier barrier{
              .resourceId = req.resourceId,
#ifdef KT_VULKAN
              .srcStages = resInfo.stages,
              .dstStages = req.stages,
              .srcAccess = resInfo.access,
              .dstAccess = req.access,
#endif
              .oldLayout = resInfo.layout,
              .newLayout = req.layout,
          };

          resInfo.layout = req.layout;
#ifdef KT_VULKAN
          resInfo.stages = req.stages;
          resInfo.access = req.access;
#endif
          if (checkHandoff(pass, barrier, resInfo)) {
            // Make the pass just before this one initiate the handoff
            passBarriers[resInfo.lastUsedInPass].post.image.push_back(barrier);
            barriers.needsWaitFor = resInfo.lastUsedInPass;
          }
          barriers.pre.image.push_back(barrier);
        } else {
          BufferBarrier barrier{
              .resourceId = req.resourceId,
#ifdef KT_VULKAN
              .srcStages = resInfo.stages,
              .dstStages = req.stages,
              .srcAccess = resInfo.access,
              .dstAccess = req.access,
#endif
          };
#if KT_VULKAN
          resInfo.access = req.access;
          resInfo.stages = req.stages;
#endif

          if (checkHandoff(pass, barrier, resInfo)) {
            // Make the pass just before this one initiate the handoff
            passBarriers[resInfo.lastUsedInPass].post.buffer.push_back(barrier);
            barriers.needsWaitFor = resInfo.lastUsedInPass;
          }
          barriers.pre.buffer.push_back(barrier);
        }

        resInfo.lastUsedInPass = static_cast<size_t>(idx);
      };

      for (auto& req : reqs.invalidate) {
        makeBarrier(req);
      }
      for (auto& req : reqs.flush) {
        makeBarrier(req);
      }
      passBarriers.push_back(std::move(barriers));
    }
  }

  Resources RenderGraphBuilder::buildResources() {
    std::vector<Buffer> buffers;
    std::vector<Image> images;
    std::unordered_map<std::string, size_t> nameToBuffer;
    std::unordered_map<std::string, size_t> nameToImage;
    std::vector<RelativeImage> swapchainRelativeImages;
    std::vector<RelativeImage> resolutionRelativeImages;
    for (const auto& [idx, res] : physicalResourceInfos | std::views::enumerate) {
      if (res.isBufferLike()) {
        auto result = Buffer::create({res.bufferInfo.size,
#ifdef KT_VULKAN
                                      res.bufferInfo.usage,
#endif
                                      res.bufferInfo.mappingMode, res.bufferInfo.memoryUsage, res.name.c_str()});
        KT_REQUIRE(result.isOk(), "Failed to create buffer for resource '{}': {}", res.name, result.error());
        auto& buf = result.value();
        buffers.push_back(std::move(buf));
        if (!nameToBuffer.contains(res.name)) // Mapped buffers share a name, we want the index to point to the first one, not the last
          nameToBuffer[res.name] = buffers.size() - 1;
        KT_DEBUG("Created buffer {} in slot {}", res.name, buffers.size() - 1);
      } else {
        auto result = Image::create({ImageDim::e2D, res.format, res.size,
#ifdef KT_VULKAN
                                     res.imageUsage,
#endif
                                     res.mipLevels, res.layers, res.name.c_str()});
        KT_REQUIRE(result.isOk(), "Failed to create image for resource '{}': {}", res.name, result.error());
        auto& img = result.value();
        images.push_back(std::move(img));
        nameToImage[res.name] = images.size() - 1;
        KT_DEBUG("Created image {} in slot {}", res.name, images.size() - 1);
        switch (res.sizeType) {
        case AttachmentSize::SwapchainRelative:
          swapchainRelativeImages.push_back({.index = images.size() - 1, .ratio = res.ratioSize});
          break;
        case AttachmentSize::ResolutionRelative:
          resolutionRelativeImages.push_back({.index = images.size() - 1, .ratio = res.ratioSize});
          break;
        case AttachmentSize::Absolute:
          break;
        }
      }
    }

    return Resources{
        .images = std::move(images),
        .buffers = std::move(buffers),
        .nameToImage = std::move(nameToImage),
        .nameToBuffer = std::move(nameToBuffer),
        .physicalImageHasHistory = std::move(physicalImageHasHistory),
        .swapchainRelativeImages = std::move(swapchainRelativeImages),
        .resolutionRelativeImages = std::move(resolutionRelativeImages),
    };
  }

  std::vector<RenderPass> RenderGraphBuilder::bakePasses(const Resources& createdResources) {
    std::vector<RenderPass> bakedPasses;
    bakedPasses.reserve(passStack.size());
    for (const auto& [idx, passId] : passStack | std::views::enumerate) {
      auto& pass = *passes[passId];
      auto& barriers = passBarriers[static_cast<size_t>(idx)];

      for (auto& barrier : barriers.pre.image) {
        barrier.resourceId = createdResources.nameToImage.at(physicalResourceInfos[barrier.resourceId].name);
      }

      for (auto& barrier : barriers.post.image) {
        barrier.resourceId = createdResources.nameToImage.at(physicalResourceInfos[barrier.resourceId].name);
      }

      for (auto& barrier : barriers.pre.buffer) {
        barrier.resourceId = createdResources.nameToBuffer.at(physicalResourceInfos[barrier.resourceId].name);
      }

      for (auto& barrier : barriers.post.buffer) {
        barrier.resourceId = createdResources.nameToBuffer.at(physicalResourceInfos[barrier.resourceId].name);
      }

      PhysResourceId extentSourceId{};

      auto getResId = [&](const PhysResourceId id) {
        if (id.unused()) {
          return PhysResourceId{};
        }
        return PhysResourceId{createdResources.nameToImage.at(physicalResourceInfos[id].name)};
      };

      std::vector<RenderAttachment> colorAttachments;
      colorAttachments.reserve(pass.getColorOutputs().size());
      for (uint32_t i = 0; i < pass.getColorOutputs().size(); ++i) {
        auto* output = pass.getColorOutputs()[i];
        auto* input = pass.getColorInputs()[i];

        if (extentSourceId.unused() && output != nullptr) {
          KT_DEBUG("Pass '{}': Color output '{}' is the extent source", pass.getName(), output->getName());
          extentSourceId = getResId(output->getPhysicalId());
        } else if (extentSourceId.unused() && input != nullptr) {
          KT_DEBUG("Pass '{}': Color input '{}' is the extent source", pass.getName(), input->getName());
          extentSourceId = getResId(input->getPhysicalId());
        }

        // TODO: Check transient to set StoreOp to DontCare
        LoadOp loadOp = LoadOp::DontCare;
        StoreOp storeOp = StoreOp::Store;
        if (input) {
          loadOp = LoadOp::Load;
        } else if (pass.getClearColor(i, nullptr)) {
          loadOp = LoadOp::Clear;
        }

        colorAttachments.push_back({
            .resourceId = getResId(output->getPhysicalId()),
            .loadOp = loadOp,
            .storeOp = storeOp,
        });
      }

      RenderAttachment ds{
          .resourceId = {},
          .loadOp = LoadOp::DontCare,
          .storeOp = StoreOp::Store,
      };
      {
        auto* dsInput = pass.getDepthStencilInput();
        auto* dsOutput = pass.getDepthStencilOutput();

        if (extentSourceId.unused() && dsOutput != nullptr) {
          KT_DEBUG("Pass '{}': Depth-stencil output '{}' is the extent source", pass.getName(), dsOutput->getName());
          extentSourceId = getResId(dsOutput->getPhysicalId());
        } else if (extentSourceId.unused() && dsInput != nullptr) {
          KT_DEBUG("Pass '{}': Depth-stencil input '{}' is the extent source", pass.getName(), dsInput->getName());
          extentSourceId = getResId(dsInput->getPhysicalId());
        }

        ds.resourceId = getResId(dsOutput ? dsOutput->getPhysicalId() : (dsInput ? dsInput->getPhysicalId() : PhysResourceId{}));
        if (dsInput) {
          ds.loadOp = LoadOp::Load;
        } else if (pass.getClearDepthStencil(nullptr)) {
          ds.loadOp = LoadOp::Clear;
        }
      }

      RenderPass bakedPass(std::move(pass.getName()), pass.getQueue(), std::move(barriers), std::move(colorAttachments), ds,
                           pass.getAutoBeingRendering());
      bakedPass.setExtentSourceId(extentSourceId);
      bakedPass.passInterface = pass.getInterface();
      bakedPass.buildCb = std::move(pass.getBuildCallback());
      bakedPass.getClearDepthStencilCb = std::move(pass.getGetClearDepthStencilCallback());
      bakedPass.getClearColorCb = std::move(pass.getGetClearColorCallback());
      bakedPass.setDepthStencilLayout(pass.getDepthStencilLayout());

      bakedPasses.push_back(std::move(bakedPass));
    }

    return bakedPasses;
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
#ifdef KT_VULKAN
        .imageUsage = resource.getImageUsage(),
#endif
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

    if (resInfo.format == KT_FORMAT_UNDEFINED)
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
} // namespace kt::rdr
