#include "../builder.hpp"
#include "keptech/maths/maths.hpp"
#include "keptech/rhi/buffer.hpp"
#include "keptech/rhi/cmdBuf.hpp"
#include "keptech/rhi/image.hpp"
#include "keptech/rhi/rhi.hpp"
#include <ranges>

namespace kt {
  void RenderGraphBuilder::deduplicateResources() {
    PhysResourceId physId{0};

#pragma region Lambda Helpers
    auto hasHistory = [&](PhysResourceId id) {
      if (static_cast<size_t>(id) >= physicalImageHasHistory.size())
        return false;
      bool history = physicalImageHasHistory[id];
      return history;
    };

    auto postBufferId = [&](bool host) {
      physicalResourceInfos.back().lifetime.blocked = true;
      if (host) {
        for (size_t i = 0; i < (MAX_FRAMES_IN_FLIGHT - 1); ++i) {
          physicalResourceInfos.push_back(physicalResourceInfos.back());
        }
        physId += (MAX_FRAMES_IN_FLIGHT - 1);
      }
    };

    auto updateImage = [&](PhysResourceId id, Bitflag<QueueType> queues, Bitflag<rhi::ImageUsage> usage, bool history) {
      auto& info = physicalResourceInfos[id];
      info.queues |= queues;
      info.imageUsage |= usage;
      if (history) {
        auto& historyInfo = physicalResourceInfos[static_cast<size_t>(id) + 1];
        historyInfo.queues |= queues;
        historyInfo.imageUsage |= usage;
      }
    };

    auto updateBuffer = [&](PhysResourceId id, Bitflag<QueueType> queues, Bitflag<rhi::BufferUsage> usage, bool host) {
      for (size_t i = 0; i < (host ? MAX_FRAMES_IN_FLIGHT : 1); ++i) {
        physicalResourceInfos[id + i].queues |= queues;
        physicalResourceInfos[id + i].bufferInfo.usage |= usage;

        physicalResourceInfos[id + i].lifetime.blocked |= host;
      }
    };
#pragma endregion

    for (auto& pass : passes) {
      for (auto& input : pass->getGenericBufferInputs()) {
        if (input.buffer->getBufferUsage().has(rhi::BufferUsage::Uniform)) {
          auto info = input.buffer->getBufferInfo();
          if (info.size % 256 != 0) {
#ifdef KT_DX12
            KT_WARN("DX12 requires uniform buffer sizes to be a multiple of 256 bytes. Buffer '{}' has size {}. Padding to 256 bytes.",
                    input.buffer->getName(), info.size);
            info.size = roundToAlignment(info.size, 256);
            input.buffer->setBufferInfo(info);
#else
            KT_WARN("Buffer '{}' has size {} which is not a multiple of 256 bytes. This may cause issues on some platforms.",
                    input.buffer->getName(), info.size);
#endif
          }
        }
      }
    }

    for (const auto passPtr : passStack) {
      auto& pass = *passPtr;
      for (auto& input : pass.getGenericTextureInputs()) {
        if (!input.texture->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*input.texture));
          input.texture->setPhysicalId(physId++);
        } else {
          updateImage(input.texture->getPhysicalId(), input.texture->getUsedQueues(), input.texture->getImageUsage(), false);
        }

        physicalResourceInfos[input.texture->getPhysicalId()].lifetime.readFrom(pass.getIndex());
      }

      for (auto input : pass.getHistoryInputs()) {
        if (!input->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*input));
          physicalResourceInfos.push_back(physicalResourceInfos.back()); // Duplicate for history.
          input->setPhysicalId(physId++);
          ++physId;
        } else {
          // If the image already has info, but is not set as history we need to duplicate it and insert it in place
          bool hasHistoryAlready = hasHistory(input->getPhysicalId());
          if (!hasHistoryAlready) {
            physicalResourceInfos.insert(physicalResourceInfos.begin() +
                                             static_cast<ptrdiff_t>(static_cast<size_t>(input->getPhysicalId())) + 1,
                                         physicalResourceInfos[static_cast<size_t>(input->getPhysicalId())]); // Duplicate for history.
            ++physId;

            // Then correct the affected IDs
            for (auto& resource : resources) {
              if (resource->getPhysicalId().used() &&
                  static_cast<size_t>(resource->getPhysicalId()) > static_cast<size_t>(input->getPhysicalId())) {
                auto id = resource->getPhysicalId();
                ++id;
                resource->setPhysicalId(id);
              }
            }
          }

          updateImage(input->getPhysicalId(), input->getUsedQueues(), input->getImageUsage(), true);
        }

        if (physicalImageHasHistory.size() < static_cast<size_t>(input->getPhysicalId()) + 1)
          physicalImageHasHistory.resize(static_cast<size_t>(input->getPhysicalId()) + 1, false);

        physicalImageHasHistory[static_cast<size_t>(input->getPhysicalId())] = true;

        // History images cannot be combined.
        for (size_t i = 0; i < (MAX_FRAMES_IN_FLIGHT - 1); ++i) {
          physicalResourceInfos[static_cast<size_t>(input->getPhysicalId()) + i].lifetime.blocked = true;
        }
      }

      for (auto& input : pass.getGenericBufferInputs()) {
        bool host = input.buffer->getBufferInfo().isHostAccessible();
        if (!input.buffer->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*input.buffer));
          input.buffer->setPhysicalId(physId++);
          postBufferId(host);
        } else {
          updateBuffer(input.buffer->getPhysicalId(), input.buffer->getUsedQueues(), input.buffer->getBufferUsage(), host);
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
          updateImage(input.getPhysicalId(), input.getUsedQueues(), input.getImageUsage(), hasHistory(input.getPhysicalId()));
        }

        KT_REQUIRE(!pass.getColorOutputs()[jdx]->getPhysicalId().used(),
                   "Pass '{}': Cannot alias color output '{}'. Physical ID already claimed.", pass.getName(),
                   pass.getColorOutputs()[jdx]->getName());
        KT_DEBUG("Pass '{}': Aliasing color output '{}' to input '{}'.", pass.getName(), pass.getColorOutputs()[jdx]->getName(),
                 input.getName());
        pass.getColorOutputs()[jdx]->setPhysicalId(input.getPhysicalId());

        physicalResourceInfos[input.getPhysicalId()].lifetime.readFrom(pass.getIndex());
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
          updateImage(input.getPhysicalId(), input.getUsedQueues(), input.getImageUsage(), hasHistory(input.getPhysicalId()));
        }

        KT_REQUIRE(!pass.getStorageImageOutputs()[jdx]->getPhysicalId().used(),
                   "Pass '{}': Cannot alias storage image output '{}'. Physical ID already claimed.", pass.getName(),
                   pass.getStorageImageOutputs()[jdx]->getName());
        KT_DEBUG("Pass '{}': Aliasing storage image output '{}' to input '{}'.", pass.getName(),
                 pass.getStorageImageOutputs()[jdx]->getName(), input.getName());
        pass.getStorageImageOutputs()[jdx]->setPhysicalId(input.getPhysicalId());

        physicalResourceInfos[input.getPhysicalId()].lifetime.readFrom(pass.getIndex());
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
          updateBuffer(input.getPhysicalId(), input.getUsedQueues(), input.getBufferUsage(), host);
        }

        KT_REQUIRE(!pass.getStorageOutputs()[jdx]->getPhysicalId().used(),
                   "Pass '{}': Cannot alias storage output '{}'. Physical ID already claimed.", pass.getName(),
                   pass.getStorageOutputs()[jdx]->getName());
        KT_DEBUG("Pass '{}': Aliasing storage output '{}' to input '{}'.", pass.getName(), pass.getStorageOutputs()[jdx]->getName(),
                 input.getName());
        pass.getStorageOutputs()[jdx]->setPhysicalId(input.getPhysicalId());
      }

      for (auto* output : pass.getColorOutputs()) {
        if (renderTargetsBlitable) {
          output->addImageUsage(rhi::CommandBuffer::getBlitSrcUsage());
        }
        if (!output->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*output));
          output->setPhysicalId(physId++);
        } else {
          updateImage(output->getPhysicalId(), output->getUsedQueues(), output->getImageUsage(), hasHistory(output->getPhysicalId()));
        }

        physicalResourceInfos[output->getPhysicalId()].lifetime.writtenFrom(pass.getIndex());
      }

      for (auto* output : pass.getStorageImageOutputs()) {
        if (renderTargetsBlitable) {
          output->addImageUsage(rhi::CommandBuffer::getBlitSrcUsage());
        }
        if (!output->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*output));
          output->setPhysicalId(physId++);
        } else {
          updateImage(output->getPhysicalId(), output->getUsedQueues(), output->getImageUsage(), hasHistory(output->getPhysicalId()));
        }
        physicalResourceInfos[output->getPhysicalId()].lifetime.writtenFrom(pass.getIndex());
      }

      for (auto* output : pass.getStorageOutputs()) {
        bool host = output->getBufferInfo().isHostAccessible();
        if (!output->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*output));
          output->setPhysicalId(physId++);
          postBufferId(host);
        } else {
          updateBuffer(output->getPhysicalId(), output->getUsedQueues(), output->getBufferUsage(), host);
        }
      }

      for (auto* mapped : pass.getMappedBuffers()) {
        bool host = mapped->getBufferInfo().isHostAccessible();
        if (!host) {
          KT_WARN("Pass '{}': Mapped buffer '{}' is not host accessible. Mapped buffers should be host accessible.", pass.getName(),
                  mapped->getName());
        }
        if (!mapped->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*mapped));
          mapped->setPhysicalId(physId++);
          postBufferId(host);
        } else {
          updateBuffer(mapped->getPhysicalId(), mapped->getUsedQueues(), mapped->getBufferUsage(), host);
        }
      }

      for (auto* output : pass.getTransferOutputs()) {
        bool host = output->getBufferInfo().isHostAccessible();
        if (!output->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*output));
          output->setPhysicalId(physId++);
          postBufferId(host);
        } else {
          updateBuffer(output->getPhysicalId(), output->getUsedQueues(), output->getBufferUsage(), host);
        }
      }

      auto* dsInput = pass.getDepthStencilInput();
      auto* dsOutput = pass.getDepthStencilOutput();
      if (dsInput) {
        if (!dsInput->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*dsInput));
          dsInput->setPhysicalId(physId++);
        } else {
          updateImage(dsInput->getPhysicalId(), dsInput->getUsedQueues(), dsInput->getImageUsage(), hasHistory(dsInput->getPhysicalId()));
        }
        physicalResourceInfos[dsInput->getPhysicalId()].lifetime.readFrom(pass.getIndex());
        // If both, combine dsOutput and dsInput.
        if (dsOutput) {
          KT_REQUIRE(!dsOutput->getPhysicalId().used(), "Pass '{}': Cannot alias depth-stencil output '{}'. Physical ID already claimed.",
                     pass.getName(), dsOutput->getName());
          KT_DEBUG("Pass '{}': Aliasing depth-stencil output '{}' to input '{}'.", pass.getName(), dsOutput->getName(), dsInput->getName());
          dsOutput->setPhysicalId(dsInput->getPhysicalId());

          updateImage(dsInput->getPhysicalId(), dsOutput->getUsedQueues(), dsOutput->getImageUsage(), hasHistory(dsInput->getPhysicalId()));

          physicalResourceInfos[dsOutput->getPhysicalId()].lifetime.writtenFrom(pass.getIndex());
        }
      } else if (dsOutput) {
        if (!dsOutput->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*dsOutput));
          dsOutput->setPhysicalId(physId++);
        } else {
          updateImage(dsOutput->getPhysicalId(), dsOutput->getUsedQueues(), dsOutput->getImageUsage(),
                      hasHistory(dsOutput->getPhysicalId()));
        }

        physicalResourceInfos[dsOutput->getPhysicalId()].lifetime.writtenFrom(pass.getIndex());
      }
    }

    {
      auto backbufferIt = resourceNameToId.find(backbufferSource);
      KT_REQUIRE(backbufferIt != resourceNameToId.end(), "Backbuffer source '{}' not found in render graph", backbufferSource);
      auto& backbufferVirt = *resources[backbufferIt->second];
      auto& backbufferPhys = physicalResourceInfos[backbufferVirt.getPhysicalId()];

      backbufferPhys.imageUsage |= rhi::CommandBuffer::getBlitSrcUsage();
    }

    physicalImageHasHistory.clear();
    physicalImageHasHistory.resize(physicalResourceInfos.size());

    for (auto passPtr : passStack) {
      auto& pass = *passPtr;

      for (auto& history : pass.getHistoryInputs()) {
        KT_REQUIRE(history->getPhysicalId().used(), "Pass '{}': History input '{}' is used, but is never written to.", pass.getName(),
                   history->getName());
        physicalImageHasHistory[history->getPhysicalId()] = true;
      }
    }
  }

  void RenderGraphBuilder::combineResources() {
    if (settings.dontCombineResources) {
      KT_DEBUG("Resource combining is disabled by settings. Skipping resource combining.");
      return;
    }

#if KT_LOG_LEVEL >= KT_LOG_LEVEL_DEBUG
    size_t combinedCount = physicalResourceInfos.size();
    size_t savedBytes = 0;
#endif

    for (size_t i = 0; i < physicalResourceInfos.size(); ++i) {
      auto& resA = physicalResourceInfos[i];
      if (resA.lifetime.blocked)
        continue;

      // Attempt to combine with future resources.
      for (size_t j = i + 1; j < physicalResourceInfos.size(); ++j) {
        auto& resB = physicalResourceInfos[j];
        if (resB.lifetime.blocked)
          continue;

        bool formatsMatch = resA.format == resB.format;
        bool sizesMatch = resA.size == resB.size && resA.ratioSize == resB.ratioSize && resA.sizeType == resB.sizeType;
        bool lifetimesCompatible = resA.lifetime.canCombineWith(resB.lifetime);

        if (formatsMatch && sizesMatch && lifetimesCompatible) {
          KT_DEBUG("Combining images '{}' and '{}'", resA.name, resB.name);

          for (auto& resource : resources) {
            if (resource->getPhysicalId() == PhysResourceId(j)) {
              resource->setPhysicalId(PhysResourceId(i));
            } else if (resource->getPhysicalId() > PhysResourceId(j)) {
              resource->setPhysicalId(resource->getPhysicalId() - 1ull);
            }
          }

          resA.queues |= resB.queues;
          resA.imageUsage |= resB.imageUsage;
          resA.mipLevels = std::max(resA.mipLevels, resB.mipLevels);
          resA.layers = std::max(resA.layers, resB.layers);
          resA.lifetime.mergeWith(resB.lifetime);

#if KT_LOG_LEVEL >= KT_LOG_LEVEL_DEBUG
          ++combinedCount;
          savedBytes += rhi::RHI::get().getByteSize(resB.format, resB.size, resB.mipLevels, resB.layers);
#endif
          combinedPhysicalResourceNames[PhysResourceId(i)].push_back(resB.name);

          physicalResourceInfos.erase(physicalResourceInfos.begin() + static_cast<ptrdiff_t>(j));
          --j;
        }
      }
    }

#if KT_LOG_LEVEL >= KT_LOG_LEVEL_DEBUG
    const char* memSuffix = "B";
    double savedBytesDouble = static_cast<double>(savedBytes);
    if (savedBytes > 1024) {
      savedBytesDouble /= 1024;
      memSuffix = "KB";
    }
    if (savedBytes > 1024) {
      savedBytesDouble /= 1024;
      memSuffix = "MB";
    }

    KT_DEBUG("Combined {} resources, saving {:.2f}{} of memory.", combinedCount - physicalResourceInfos.size(), savedBytesDouble,
             memSuffix);
#endif
  }

  Resources RenderGraphBuilder::buildResources() {
    auto& rhi = rhi::RHI::get();
    auto cmd = rhi.allocateGraphicsCommandBuffers(1)[0];

    Vec<rhi::Buffer> buffers;
    Vec<rhi::Image> images;
    std::unordered_map<std::string, size_t> nameToBuffer;
    std::unordered_map<std::string, size_t> nameToImage;
    Vec<RelativeImage> swapchainRelativeImages;
    Vec<RelativeImage> resolutionRelativeImages;
    Vec<bool> imageHistory;

    for (const auto& [idx, res] : physicalResourceInfos | std::views::enumerate) {
      if (res.isBufferLike()) {
        auto result = rhi::Buffer::create({res.bufferInfo.size, res.bufferInfo.usage, res.bufferInfo.type, res.name.c_str()});
        KT_REQUIRE(result.isOk(), "Failed to create buffer for resource '{}': {}", res.name, result.error());
        auto& buf = result.value();
        buffers.push_back(std::move(buf));
        if (!nameToBuffer.contains(res.name)) // Mapped buffers share a name, we want the index to point to the first one, not the last
          nameToBuffer[res.name] = buffers.size() - 1;

        if (combinedPhysicalResourceNames.contains(PhysResourceId(static_cast<size_t>(idx)))) {
          auto& combinedNames = combinedPhysicalResourceNames[PhysResourceId(static_cast<size_t>(idx))];
          for (const auto& combinedName : combinedNames) {
            if (!nameToBuffer.contains(combinedName))
              nameToBuffer[combinedName] = buffers.size() - 1;
          }
        }
        KT_DEBUG("Created buffer {} in slot {}", res.name, buffers.size() - 1);
      } else {
        auto result =
            rhi::Image::create({rhi::ImageDim::e2D, res.format, res.size, res.imageUsage, res.mipLevels, res.layers, res.name.c_str()});
        KT_REQUIRE(result.isOk(), "Failed to create image for resource '{}': {}", res.name, result.error());
        auto& img = result.value();
        images.push_back(std::move(img));
        if (physicalImageHasHistory[static_cast<size_t>(idx)]) {
          imageHistory.push_back(true);

          if (res.imageUsage.has(rhi::ImageUsage::RenderTarget)) {
            cmd.transitionImage(images.back(), rhi::ImageLayout::Undefined, rhi::ImageLayout::RenderTarget);
            cmd.clearColorImage(images.back(), {0, 0, 0, 1});
            cmd.transitionImage(images.back(), rhi::ImageLayout::RenderTarget, rhi::ImageLayout::Undefined);
          } else if (res.imageUsage.has(rhi::ImageUsage::DepthStencil)) {
            cmd.transitionImage(images.back(), rhi::ImageLayout::Undefined, rhi::ImageLayout::DepthStencilTarget);
            cmd.clearDepthStencilImage(images.back(), 1.0f, 0);
            cmd.transitionImage(images.back(), rhi::ImageLayout::DepthStencilTarget, rhi::ImageLayout::Undefined);
          }
        } else {
          imageHistory.push_back(false);
        }
        if (!nameToImage.contains(res.name))
          nameToImage[res.name] = images.size() - 1;

        if (combinedPhysicalResourceNames.contains(PhysResourceId(static_cast<size_t>(idx)))) {
          const auto& combinedNames = combinedPhysicalResourceNames[PhysResourceId(static_cast<size_t>(idx))];
          for (const auto& combinedName : combinedNames) {
            if (!nameToImage.contains(combinedName))
              nameToImage[combinedName] = images.size() - 1;
          }
        }

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

    cmd.end();

    rhi.submitGraphicsCmd(cmd);

    return Resources{
        .images = std::move(images),
        .buffers = std::move(buffers),
        .nameToImage = std::move(nameToImage),
        .nameToBuffer = std::move(nameToBuffer),
        .imageHasHistory = std::move(imageHistory),
        .swapchainRelativeImages = std::move(swapchainRelativeImages),
        .resolutionRelativeImages = std::move(resolutionRelativeImages),
    };
  }
} // namespace kt