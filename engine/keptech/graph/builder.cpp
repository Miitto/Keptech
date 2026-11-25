#include "builder.hpp"

#include "graph.hpp"
#include "keptech/core/kt-logger.hpp"
#include "keptech/rhi/buffer.hpp"
#include "keptech/rhi/bufferCreateInfo.hpp"
#include "keptech/rhi/bufferUsage.hpp"
#include "keptech/rhi/cmdBuf.hpp"
#include "keptech/rhi/descriptorInfo.hpp"
#include "keptech/rhi/descriptorSet.hpp"
#include "keptech/rhi/descriptorTypes.hpp"
#ifdef KT_VULKAN
#include "keptech/rhi/helpers/formatting.hpp"
#endif
#include "keptech/rhi/imageRef.hpp"
#include "keptech/rhi/rhi.hpp"
#include "renderResources.hpp"
#include <ranges>
#include <spdlog/fmt/bundled/ranges.h>
#include <vector>

namespace kt {
  using namespace rhi;

  namespace {
    QueueType getRawQueue(QueueType queue) {
      if (queue == QueueType::Compute)
        return QueueType::Graphics; // Normal compute queue is merged into graphics queue.
      return queue;
    }
  } // namespace

  void RenderGraphBuilder::bake() {
    KT_REQUIRE(swapchainSize.x > 0 && swapchainSize.y > 0, "Swapchain size must be set before baking the render graph.");
    KT_REQUIRE(renderResolution.x > 0 && renderResolution.y > 0, "Render resolution must be set before baking the render graph.");
    KT_REQUIRE(swapchainFormat != rhi::ImageFormat::Undefined, "Swapchain format must be set before baking the render graph.");
    KT_REQUIRE(!passes.empty(), "No passes have been added to the render graph. At least one pass must be added before baking.");

    for (auto& pass : passes) {
      pass->setupDependencies();
    }

    validatePasses();

    reorderPasses(passStack);

    buildPhysicalResources();
    buildRequirements();
    buildBarriers();
  }

  namespace {

    Vec<PassGroup> buildPassGroups(const Vec<RenderPass>& passes) {
      QueueType queue = getRawQueue(passes.front().getQueue());
      Vec<PassGroup> passGroups;
      PassGroup currentGroup{.queue = queue};
      for (const auto& pass : passes) {
        // If we changed queue, or pass in a different queue depends on a resource from this pass, or we depend on a resource from a pass in
        // a different queue, we need to start a new group.
        if (currentGroup.count > 0 && (getRawQueue(pass.getQueue()) != currentGroup.queue || !pass.getBarriers().post.image.empty() ||
                                       !pass.getBarriers().post.buffer.empty() || pass.getBarriers().needsWaitFor != ~0u)) {
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

  RenderGraph RenderGraphBuilder::build() {
    auto& rhi = RHI::get();
    auto builtResources = buildResources();

    auto passDescriptors = buildDescriptors(rhi, builtResources);

    auto bakedPasses = bakePasses(builtResources);
    auto passGroups = buildPassGroups(bakedPasses);

    auto cmds = rhi.allocateGraphicsCommandBuffers(1);
    auto& cmd = cmds.front();

    Vec<rhi::CommandBuffer::ImageLayoutTransition> its;
    Vec<rhi::ImageLayout> imageFinalLayouts;
    imageFinalLayouts.reserve(finalLayouts.size());
    for (auto& transition : finalLayouts) {
      transition.resourceId = builtResources.nameToImage[physicalResourceInfos[transition.resourceId].name];
      auto& img = builtResources.images[transition.resourceId];
      if (transition.newLayout != rhi::ImageLayout::Undefined)
        its.push_back(
            CommandBuffer::ImageLayoutTransition{.imageRef = img, .oldLayout = ImageLayout::Undefined, .newLayout = transition.newLayout});
      imageFinalLayouts.push_back(transition.newLayout);
    }
    cmd.transitionImages(its);

    cmd.end();

    rhi.submitGraphicsCmd(cmd);
    rhi.waitGraphicsIdle();

    RenderGraph res{
        std::move(passGroups), std::move(bakedPasses), std::move(builtResources), std::move(imageFinalLayouts), std::move(passDescriptors),
    };

    res.setBackbufferSource(backbufferSource);

    KT_DEBUG("Render graph built successfully with {} passes, {} images, and {} buffers", res.passes.size(), res.resources.images.size(),
             res.resources.buffers.size());

    for (const auto& [idx, pass] : res.passes | std::views::enumerate) {
      pass.setup(res.passDescriptors[static_cast<size_t>(idx)].layout);
    }

    return res;
  }

  Vec<Descriptors> RenderGraphBuilder::buildDescriptors(rhi::RHI& rhi, Resources& builtResources) {

    Vec<Descriptors> passDescriptors;
    passDescriptors.reserve(passStack.size());

    builtResources.imageUsedInPass.resize(builtResources.images.size());
    builtResources.bufferUsedInPass.resize(builtResources.buffers.size());

    for (const auto& [idxL, passId] : passStack | std::views::enumerate) {
      size_t idx = static_cast<size_t>(idxL);
      auto& pass = passes[passId];

      struct PerFrameWrite {
        size_t writeIndex;
        PhysResourceId bufferId;
      };

      Vec<rhi::DescriptorInfo> infos;
      Vec<rhi::DescriptorWriteInfo> writes;
      Vec<PerFrameWrite> perFrameWrites;
      Vec<PerFrameWrite> historyImageWrites;

      size_t imgMaxCount = pass->getGenericTextureInputs().size() + pass->getStorageImageOutputs().size();
      size_t bufMaxCount = (pass->getGenericBufferInputs().size() + pass->getStorageOutputs().size()) *
                           MAX_FRAMES_IN_FLIGHT; // Reserve max bound of every buffer being CPU mapped (and hence duplicated).
      size_t writeMaxCount = imgMaxCount + bufMaxCount;

      writes.reserve(writeMaxCount);

      auto getImageIndex = [&](const std::string& name) -> size_t { return builtResources.nameToImage[name]; };
      auto getBufferIndex = [&](const std::string& name) -> size_t { return builtResources.nameToBuffer[name]; };
      auto getImage = [&](const std::string& name) -> Image& { return builtResources.images[getImageIndex(name)]; };
      auto getBuffer = [&](const std::string& name, size_t offset = 0) -> Buffer& {
        return builtResources.buffers[getBufferIndex(name) + offset];
      };

      auto addBufferWrites = [&](rhi::BufferUsage usage, rhi::DescriptorType descriptorType) {
        for (const auto& buffer : pass->getGenericBufferInputs()) {
          if (buffer.buffer && buffer.buffer->getPhysicalId().used() && buffer.buffer->getBufferUsage().has(usage)) {
            uint32_t binding = static_cast<uint32_t>(writes.size());
            KT_DEBUG("Pass '{}' has {} buffer input '{}' at binding {}", pass->getName(),
                     buffer.buffer->getBufferUsage().has(rhi::BufferUsage::Uniform) ? "uniform" : "storage", buffer.buffer->getName(),
                     binding);

            KT_ASSERT(buffer.buffer->getBufferInfo().size > 0, "Pass {}: Buffer '{}' has size 0. Buffers must have a non-zero size.",
                      pass->getName(), buffer.buffer->getName());

            KT_ASSERT((!buffer.buffer->getBufferUsage().has(rhi::BufferUsage::Storage)) || buffer.stride > 0,
                      "Pass {}: Buffer '{}' has stride 0. Storage buffers must have a non-zero stride.", pass->getName(),
                      buffer.buffer->getName());

            writes.push_back(rhi::DescriptorWriteInfo{
                .binding = binding,
                .arrayIndex = 0,
                .type = descriptorType,
                .buffer = getBuffer(buffer.buffer->getName()),
                .offset = 0,
                .range = buffer.buffer->getBufferInfo().size,
                .stride = buffer.stride,
            });

            if (buffer.buffer->getBufferInfo().isHostAccessible()) {
              perFrameWrites.push_back(
                  PerFrameWrite{.writeIndex = writes.size() - 1, .bufferId = getBufferIndex(buffer.buffer->getName())});
            }
            builtResources.bufferUsedInPass[getBufferIndex(buffer.buffer->getName())].push_back(
                UsedInPass{.passIndex = idx, .binding = binding, .descriptorType = descriptorType});

            infos.push_back(rhi::DescriptorInfo{
                .type = descriptorType,
                .binding = binding,
                .count = 1,
            });
          }
        }
      };

      addBufferWrites(rhi::BufferUsage::Uniform, rhi::DescriptorType::UniformBuffer);
      addBufferWrites(rhi::BufferUsage::Storage, rhi::DescriptorType::StorageBuffer);

      for (const auto& texture : pass->getGenericTextureInputs()) {
        if (texture.texture && texture.texture->getPhysicalId().used()) {
          uint32_t binding = static_cast<uint32_t>(writes.size());
          KT_DEBUG("Pass '{}' has texture input '{}' at binding {}", pass->getName(), texture.texture->getName(), binding);
          writes.push_back(rhi::DescriptorWriteInfo{
              .binding = binding,
              .arrayIndex = 0,
              .type = rhi::DescriptorType::SampledImage,
              .image = getImage(texture.texture->getName()),
          });

          historyImageWrites.push_back(
              PerFrameWrite{.writeIndex = writes.size() - 1, .bufferId = getImageIndex(texture.texture->getName())});

          builtResources.imageUsedInPass[getImageIndex(texture.texture->getName())].push_back(UsedInPass{
              .passIndex = idx,
              .binding = binding,
              .descriptorType = rhi::DescriptorType::SampledImage,
              .layout = rhi::ImageLayout::ShaderReadOnly,
          });
          infos.push_back(rhi::DescriptorInfo{
              .type = rhi::DescriptorType::SampledImage,
              .binding = binding,
              .count = 1,
          });
        }
      }

      for (const auto texture : pass->getHistoryInputs()) {
        if (texture && texture->getPhysicalId().used()) {
          uint32_t binding = static_cast<uint32_t>(writes.size());
          KT_DEBUG("Pass '{}' has history texture input '{}' at binding {}", pass->getName(), texture->getName(), binding);
          writes.push_back(rhi::DescriptorWriteInfo{
              .binding = binding,
              .arrayIndex = 0,
              .type = rhi::DescriptorType::SampledImage,
              .image = getImage(texture->getName()),
          });

          builtResources.imageUsedInPass[getImageIndex(texture->getName())].push_back(UsedInPass{
              .passIndex = idx,
              .binding = binding,
              .descriptorType = rhi::DescriptorType::SampledImage,
              .layout = rhi::ImageLayout::ShaderReadOnly,
              .history = true,
          });
          infos.push_back(rhi::DescriptorInfo{
              .type = rhi::DescriptorType::SampledImage,
              .binding = binding,
              .count = 1,
          });
        }
      }

      for (const auto& buffer : pass->getStorageOutputs()) {
        if (buffer && buffer->getPhysicalId().used()) {
          uint32_t binding = static_cast<uint32_t>(writes.size());
          KT_DEBUG("Pass '{}' has storage buffer output '{}' at binding {}", pass->getName(), buffer->getName(), binding);
          writes.push_back(rhi::DescriptorWriteInfo{
              .binding = binding,
              .arrayIndex = 0,
              .type = rhi::DescriptorType::RWStorageBuffer,
              .buffer = getBuffer(buffer->getName()),
              .offset = 0,
              .range = buffer->getBufferInfo().size,
              .stride = buffer->getBufferInfo().stride,
          });

          if (buffer->getBufferInfo().isHostAccessible()) {
            perFrameWrites.push_back(PerFrameWrite{.writeIndex = writes.size() - 1, .bufferId = getBufferIndex(buffer->getName())});
          }
          builtResources.bufferUsedInPass[getBufferIndex(buffer->getName())].push_back(
              UsedInPass{.passIndex = idx, .binding = binding, .descriptorType = rhi::DescriptorType::RWStorageBuffer});

          infos.push_back(rhi::DescriptorInfo{
              .type = rhi::DescriptorType::RWStorageBuffer,
              .binding = binding,
              .count = 1,
          });
        }
      }

      for (const auto& image : pass->getStorageImageOutputs()) {
        if (image && image->getPhysicalId().used()) {
          uint32_t binding = static_cast<uint32_t>(writes.size());
          KT_DEBUG("Pass '{}' has storage image output '{}' at binding {}", pass->getName(), image->getName(), binding);

          writes.push_back(rhi::DescriptorWriteInfo{
              .binding = binding,
              .arrayIndex = 0,
              .type = rhi::DescriptorType::StorageImage,
              .image = getImage(image->getName()),
          });

          builtResources.imageUsedInPass[getImageIndex(image->getName())].push_back(
              UsedInPass{.passIndex = idx, .binding = binding, .descriptorType = rhi::DescriptorType::StorageImage});

          infos.push_back(rhi::DescriptorInfo{
              .type = rhi::DescriptorType::StorageImage,
              .binding = binding,
              .count = 1,
          });
        }
      }

      if (infos.empty()) {
        passDescriptors.push_back({});
        continue;
      }

      auto layout = rhi.createDescriptorLayout(infos);

      std::array<rhi::DescriptorSet, MAX_FRAMES_IN_FLIGHT> sets{};
      for (auto& set : sets) {
        set = rhi.allocateDescriptorSet(layout);
      }

      sets[0].write(layout, writes);

      for (size_t frame = 1; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
        for (const auto& perFrameWrite : perFrameWrites) {
          auto& write = writes[perFrameWrite.writeIndex];
          write.buffer = builtResources.buffers[perFrameWrite.bufferId + frame];
        }

        for (const auto& historyWrite : historyImageWrites) {
          auto& write = writes[historyWrite.writeIndex];
          write.image = builtResources.images[historyWrite.bufferId + (frame & 1)];
        }

        sets[frame].write(layout, writes);
      }

      passDescriptors.push_back({.layout = layout, .sets = sets});
    }

    return passDescriptors;
  }

  Vec<RenderPass> RenderGraphBuilder::bakePasses(const Resources& createdResources) {
    Vec<RenderPass> bakedPasses;
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

      Vec<RenderAttachment> colorAttachments;
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

} // namespace kt
