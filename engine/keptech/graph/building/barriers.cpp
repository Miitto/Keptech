#include "../builder.hpp"
#include "keptech/rhi/cmdBuf.hpp"
#include <ranges>

namespace kt {
  static constexpr Bitflag<QueueType> COMPUTE_QUEUES = QueueType::Compute | QueueType::AsyncCompute;

  void RenderGraphBuilder::buildRequirements() {
    passRequirements.clear();
    passRequirements.reserve(passStack.size());

    const auto getAccess = [&](Vec<Requirement>& req, PhysResourceId id, bool history) -> Requirement& {
      auto it = std::ranges::find_if(req, [&](const Requirement& b) { return b.resourceId == id && history == b.history; });
      if (it != req.end())
        return *it;

      req.push_back(Requirement{.resourceId = id, .layout = rhi::ImageLayout::Undefined, .access = {}, .stages = {}, .history = history});
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

        barrier.access |= input.access;
        barrier.stages |= input.stages;

        KT_REQUIRE(barrier.layout == rhi::ImageLayout::Undefined,
                   "Pass '{}': Buffer input '{}' expected to have undefined layout. Has {}. You have probably added this resource to this "
                   "pass multiple times.",
                   pass.getName(), input.buffer->getName(), barrier.layout);
        barrier.layout = input.layout;
      }

      for (auto& input : pass.getGenericTextureInputs()) {
        auto& req = getInvalidAccess(input.texture->getPhysicalId(), false);
        req.access |= input.access;
        req.stages |= input.stages;
        KT_REQUIRE(req.layout == rhi::ImageLayout::Undefined,
                   "Pass '{}': Texture input '{}' expected to have undefined layout. Has {}. You have probably added this resource to this "
                   "pass multiple times.",
                   pass.getName(), input.texture->getName(), req.layout);
        req.layout = input.layout;
      }

      for (auto& input : pass.getHistoryInputs()) {
        auto& req = getInvalidAccess(input->getPhysicalId(), true);
        req.access |= rhi::AccessFlags::ShaderSampledRead;

        if (!COMPUTE_QUEUES.intersects(pass.getQueue())) {
          req.stages |= rhi::PipelineStageFlags::FragmentShader;
        } else {
          req.stages |= rhi::PipelineStageFlags::ComputeShader;
        }

        KT_REQUIRE(req.layout == rhi::ImageLayout::Undefined,
                   "Pass '{}': History input '{}' expected to have undefined layout. Has {}. You have probably added this resource to this "
                   "pass multiple times.",
                   pass.getName(), input->getName(), req.layout);
        req.layout = rhi::ImageLayout::ShaderReadOnly;
      }

      for (auto* input : pass.getStorageImageInputs()) {
        if (!input)
          continue;
        auto& req = getInvalidAccess(input->getPhysicalId(), false);
        req.access |= rhi::AccessFlags::ShaderStorageRead | rhi::AccessFlags::ShaderStorageWrite;

        if (!COMPUTE_QUEUES.intersects(pass.getQueue())) {
          req.stages |= rhi::PipelineStageFlags::FragmentShader;
        } else {
          req.stages |= rhi::PipelineStageFlags::ComputeShader;
        }

        KT_REQUIRE(
            req.layout == rhi::ImageLayout::Undefined,
            "Pass '{}': Storage image input '{}' expected to have undefined layout. Has {}. You have probably added this resource to "
            "this pass multiple times.",
            pass.getName(), input->getName(), req.layout);

        req.layout = rhi::ImageLayout::General;
      }

      for (auto* input : pass.getStorageInputs()) {
        if (!input)
          continue;
        auto& req = getInvalidAccess(input->getPhysicalId(), false);
        req.access |= rhi::AccessFlags::ShaderStorageRead | rhi::AccessFlags::ShaderStorageWrite;

        if (!COMPUTE_QUEUES.intersects(pass.getQueue())) {
          req.stages |= rhi::PipelineStageFlags::FragmentShader;
        } else {
          req.stages |= rhi::PipelineStageFlags::ComputeShader;
        }

        KT_REQUIRE(req.layout == rhi::ImageLayout::Undefined,
                   "Pass '{}': Storage input '{}' expected to have undefined layout. Has {}. You have probably added this resource to this "
                   "pass multiple times.",
                   pass.getName(), input->getName(), req.layout);
        req.layout = rhi::ImageLayout::General;
      }

      for (auto* input : pass.getColorInputs()) {
        if (!input)
          continue;

        KT_REQUIRE(!COMPUTE_QUEUES.intersects(pass.getQueue()), "Pass '{}': Color inputs cannot be used in a compute pass", pass.getName(),
                   input->getName());

        auto& req = getInvalidAccess(input->getPhysicalId(), false);
        req.access |= rhi::AccessFlags::ColorAttachmentRead | rhi::AccessFlags::ColorAttachmentWrite;
        req.stages |= rhi::PipelineStageFlags::ColorAttachmentOutput;

        // Also used as an input attachment for programmable blending.
        if (req.layout == rhi::ImageLayout::ShaderReadOnly) {
          KT_WARN("Pass '{}': Color input '{}' is used as a shader read-only input and as a color attachment. The layout of the texture "
                  "for this pass has been set to general. This is not guarenteed to work.",
                  pass.getName(), input->getName());
          req.layout = rhi::ImageLayout::General;
        } else {
          KT_REQUIRE(req.layout == rhi::ImageLayout::Undefined,
                     "Pass '{}': Color input '{}' expected to have undefined layout. Has {}. You have probably added this resource to this "
                     "pass multiple times.",
                     pass.getName(), input->getName(), req.layout);
          req.layout = rhi::ImageLayout::RenderTarget;
        }
      }

      for (auto* output : pass.getColorOutputs()) {
        KT_REQUIRE(!COMPUTE_QUEUES.intersects(pass.getQueue()), "Pass '{}': Color outputs cannot be used in a compute pass", pass.getName(),
                   output->getName());

        auto& req = getFlushAccess(output->getPhysicalId());
        req.access |= rhi::AccessFlags::ColorAttachmentRead | rhi::AccessFlags::ColorAttachmentWrite;
        req.stages |= rhi::PipelineStageFlags::ColorAttachmentOutput;

        if (req.layout == rhi::ImageLayout::ShaderReadOnly || req.layout == rhi::ImageLayout::General) {
          KT_WARN("Pass '{}': Color output '{}' is used as a shader read-only input and as a color attachment. The layout of the texture "
                  "for this pass has been set to general. This is not guarenteed to work.",
                  pass.getName(), output->getName());
          req.layout = rhi::ImageLayout::General;
        } else {
          KT_REQUIRE(
              req.layout == rhi::ImageLayout::Undefined,
              "Pass '{}': Color output '{}' expected to have undefined layout. Has {}. You have probably added this resource to this "
              "pass multiple times.",
              pass.getName(), output->getName(), req.layout);
          req.layout = rhi::ImageLayout::RenderTarget;
        }
      }

      for (auto* output : pass.getStorageImageOutputs()) {
        auto& req = getFlushAccess(output->getPhysicalId());
        req.access |= rhi::AccessFlags::ShaderStorageWrite;

        if (!COMPUTE_QUEUES.intersects(pass.getQueue())) {
          req.stages |= rhi::PipelineStageFlags::FragmentShader;
        } else {
          req.stages |= rhi::PipelineStageFlags::ComputeShader;
        }

        KT_REQUIRE(req.layout == rhi::ImageLayout::Undefined,
                   "Pass '{}': Storage image output '{}' expected to have undefined layout. Has {}. You have probably added this "
                   "resource to this "
                   "pass multiple times.",
                   pass.getName(), output->getName(), req.layout);
        req.layout = rhi::ImageLayout::General;
      }

      for (auto* output : pass.getStorageOutputs()) {
        auto& req = getFlushAccess(output->getPhysicalId());
        req.access |= rhi::AccessFlags::ShaderStorageWrite;

        if (!COMPUTE_QUEUES.intersects(pass.getQueue())) {
          req.stages |= rhi::PipelineStageFlags::FragmentShader;
        } else {
          req.stages |= rhi::PipelineStageFlags::ComputeShader;
        }

        KT_REQUIRE(
            req.layout == rhi::ImageLayout::Undefined,
            "Pass '{}': Storage output '{}' expected to have undefined layout. Has {}. You have probably added this resource to this "
            "pass multiple times.",
            pass.getName(), output->getName(), req.layout);
        req.layout = rhi::ImageLayout::General;
      }

      for (auto* output : pass.getTransferOutputs()) {
        auto& req = getFlushAccess(output->getPhysicalId());
        req.access |= rhi::AccessFlags::TransferWrite;
        req.stages |= rhi::PipelineStageFlags::Copy;
        KT_REQUIRE(req.layout == rhi::ImageLayout::Undefined,
                   "Pass '{}': Transfer output '{}' expected to have undefined layout. Has {}. You have probably added this resource to "
                   "this pass multiple times.",
                   pass.getName(), output->getName(), req.layout);
        req.layout = rhi::ImageLayout::General;
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

        if (dstReq.layout == rhi::ImageLayout::ShaderReadOnly) {
          KT_DEBUG("Pass '{}': Depth-stencil texture '{}' is used as a shader read-only input and as a depth stencil attachment."
                   "The layout of the texture for this pass has been set to general. You should probably avoid this, or enable the "
                   "VK_KHR_unified_image_layout extension.",
                   pass.getName(), dsInput->getName(), dsOutput->getName());
          dstReq.layout = rhi::ImageLayout::General;
          pass.setDepthStencilLayout(rhi::ImageLayout::General);
        } else {
          KT_REQUIRE(dstReq.layout == rhi::ImageLayout::Undefined,
                     "Pass '{}': Depth-stencil output '{}' expected to have undefined layout. Has {}. You have probably added this "
                     "resource to this pass multiple times.",
                     pass.getName(), dsOutput->getName(), dstReq.layout);
          dstReq.layout = rhi::ImageLayout::DepthStencilTarget;
          pass.setDepthStencilLayout(rhi::ImageLayout::DepthStencilTarget);
        }

        dstReq.access |= rhi::AccessFlags::DepthStencilAttachmentRead | rhi::AccessFlags::DepthStencilAttachmentWrite;
        dstReq.stages |= rhi::PipelineStageFlags::EarlyFragmentTests | rhi::PipelineStageFlags::LateFragmentTests;

        srcReq.access |= rhi::AccessFlags::DepthStencilAttachmentRead;
        srcReq.stages |= rhi::PipelineStageFlags::EarlyFragmentTests | rhi::PipelineStageFlags::LateFragmentTests;
        srcReq.layout = rhi::ImageLayout::DepthStencilTarget;
      } else if (dsInput) {
        auto& dstReq = getInvalidAccess(dsInput->getPhysicalId(), false);

        if (dstReq.layout == rhi::ImageLayout::ShaderReadOnly) {
          KT_DEBUG("Pass '{}': Depth-stencil texture '{}' is used as a shader read-only input and as an input depth stencil attachment."
                   "The layout of the texture for this pass has been set to depth stencil read only optimal.",
                   pass.getName(), dsInput->getName());
          dstReq.layout = rhi::ImageLayout::DepthStencilReadOnly;
          pass.setDepthStencilLayout(rhi::ImageLayout::DepthStencilReadOnly);
        } else {
          KT_REQUIRE(dstReq.layout == rhi::ImageLayout::Undefined,
                     "Pass '{}': Depth-stencil input '{}' expected to have undefined layout. Has {}. You have probably added this "
                     "resource to this pass multiple times.",
                     pass.getName(), dsInput->getName(), dstReq.layout);
          dstReq.layout = rhi::ImageLayout::DepthStencilTarget;
          pass.setDepthStencilLayout(rhi::ImageLayout::DepthStencilTarget);
        }

        dstReq.access |= rhi::AccessFlags::DepthStencilAttachmentRead | rhi::AccessFlags::DepthStencilAttachmentWrite;
        dstReq.stages |= rhi::PipelineStageFlags::EarlyFragmentTests | rhi::PipelineStageFlags::LateFragmentTests;
      } else if (dsOutput) {
        auto& srcReq = getFlushAccess(dsOutput->getPhysicalId());

        if (srcReq.layout == rhi::ImageLayout::ShaderReadOnly) {
          KT_DEBUG("Pass '{}': Depth-stencil texture '{}' is used as a shader read-only input and as an output depth stencil attachment."
                   "The layout of the texture for this pass has been set to general. You should probably avoid this, or enable the "
                   "VK_KHR_unified_image_layout extension.",
                   pass.getName(), dsOutput->getName());
          srcReq.layout = rhi::ImageLayout::General;
          pass.setDepthStencilLayout(rhi::ImageLayout::General);
        } else {
          KT_REQUIRE(srcReq.layout == rhi::ImageLayout::Undefined,
                     "Pass '{}': Depth-stencil output '{}' expected to have undefined layout. Has {}. You have probably added this "
                     "resource to this pass multiple times.",
                     pass.getName(), dsOutput->getName(), srcReq.layout);
          srcReq.layout = rhi::ImageLayout::DepthStencilTarget;
          pass.setDepthStencilLayout(rhi::ImageLayout::DepthStencilTarget);
        }

        srcReq.access |= rhi::AccessFlags::DepthStencilAttachmentRead | rhi::AccessFlags::DepthStencilAttachmentWrite;
        srcReq.stages |= rhi::PipelineStageFlags::EarlyFragmentTests | rhi::PipelineStageFlags::LateFragmentTests;
      }

      passRequirements.push_back(std::move(reqs));
    }
  }

  namespace {
    struct ResInfo {
      rhi::ImageLayout layout{rhi::ImageLayout::Undefined};
      Bitflag<rhi::AccessFlags> access{};
      Bitflag<rhi::PipelineStageFlags> stages{};
      QueueType queue{0};
      size_t lastUsedInPass = ~0u;
    };

    bool needsHandoff(QueueType currentQueue, QueueType lastQueue) {
      auto bits = currentQueue | lastQueue;

      return bits.has(QueueType::AsyncCompute) && bits.clear(QueueType::AsyncCompute).intersects(QueueType::Graphics | QueueType::Compute);
    }

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

    Vec<ResInfo> resInfos;
    resInfos.resize(physicalResourceInfos.size());

    // Find the last layout/access/stage for each resource.
    for (const auto& [idx, passId] : passStack | std::views::enumerate) {
      auto& reqs = passRequirements[static_cast<size_t>(idx)];

      for (auto& req : reqs.invalidate) {
        auto& res = physicalResourceInfos[req.resourceId];
        auto& resInfo = resInfos[req.resourceId];

        if (res.isLayoutSensitive()) {
          resInfo.layout = req.layout;
        }
        resInfo.access = req.access;
        resInfo.stages = req.stages;
      }

      for (auto& req : reqs.flush) {
        auto& res = physicalResourceInfos[req.resourceId];
        auto& resInfo = resInfos[req.resourceId];

        if (res.isLayoutSensitive()) {
          resInfo.layout = req.layout;
        }
        resInfo.access = req.access;
        resInfo.stages = req.stages;
      }
    }

    auto backbufferPhysId = resources[resourceNameToId.at(backbufferSource)]->getPhysicalId();
    auto& backbufferResInfo = resInfos[backbufferPhysId];

    rhi::ImageLayout optimalBlitSrc = rhi::CommandBuffer::getOptimalBlitSrcLayout();

    backbufferResInfo.layout = optimalBlitSrc;

    for (const auto& [idx, resInfo] : resInfos | std::views::enumerate) {
      auto& res = physicalResourceInfos[static_cast<size_t>(idx)];
      if (res.isLayoutSensitive()) {
        finalLayouts.push_back({.resourceId = static_cast<size_t>(idx), .newLayout = resInfo.layout});
      }
    }

    size_t mergedBarriers = 0;

    for (const auto& [idx, passId] : passStack | std::views::enumerate) {
      auto& pass = *passes[passId];
      auto& reqs = passRequirements[static_cast<size_t>(idx)];

      PrePostBarriers barriers;

      auto makeBarrier = [&](const Requirement& req) {
        auto& res = physicalResourceInfos[req.resourceId];
        auto& resInfo = resInfos[req.resourceId];

        bool hOff = needsHandoff(pass.getQueue(), resInfo.queue);
        bool madeBarrier = false;

        if (res.isLayoutSensitive()) {

          ImageBarrier barrier{
              .resourceId = req.resourceId,
              .srcStages = resInfo.stages,
              .dstStages = req.stages,
              .srcAccess = resInfo.access,
              .dstAccess = req.access,
              .oldLayout = resInfo.layout,
              .newLayout = req.layout,
              .history = req.history,
          };

          // Where possible we can merge read-only barriers together.
          if (!hOff && resInfo.lastUsedInPass != ~0u) {
            constexpr Bitflag<rhi::ImageLayout> readOnlyLayouts = rhi::ImageLayout::ShaderReadOnly |
                                                                  rhi::ImageLayout::DepthStencilReadOnly | rhi::ImageLayout::TransferSrc |
                                                                  rhi::ImageLayout::GenericRead;
            if (readOnlyLayouts.has(resInfo.layout) && readOnlyLayouts.has(req.layout)) {
              if (!resInfo.stages.intersects(req.stages) || !resInfo.access.intersects(req.access) ||
                  !(resInfo.layout == req.layout || resInfo.layout == rhi::ImageLayout::GenericRead)) {
                auto b = std::ranges::find_if(passBarriers[resInfo.lastUsedInPass].pre.image,
                                              [&](const ImageBarrier& b) { return b.resourceId == req.resourceId; });
#ifndef NDEBUG
                if (b == passBarriers[resInfo.lastUsedInPass].pre.image.end()) {
                  KT_CRITICAL(
                      "Could not find barrier to merge for resource {} in pass {}. This should never happen. Pass has barriers for:",
                      req.resourceId, resInfo.lastUsedInPass);
                  for (auto& ba : passBarriers[resInfo.lastUsedInPass].pre.image) {
                    KT_CRITICAL("  Resource {}", ba.resourceId);
                  }
                  KT_ABORT("Failed to find barrier to merge");
                }
#endif
                b->newLayout = req.layout == resInfo.layout ? req.layout : rhi::ImageLayout::GenericRead;
                b->dstStages |= req.stages;
                b->dstAccess |= req.access;
#if KT_LOG_LEVEL <= SPDLOG_LEVEL_TRACE
                auto& resource = physicalResourceInfos[req.resourceId];
                KT_TRACE("Merged barrier for resource {} in pass {}.", resource.name, resInfo.lastUsedInPass);
#endif
                ++mergedBarriers;
              }
            } else {
              if (resInfo.layout != req.layout || resInfo.access != req.access || resInfo.stages != req.stages) {
                madeBarrier = true;
                barriers.pre.image.push_back(barrier);
              }
            }
          } else {
            if (resInfo.layout != req.layout || resInfo.access != req.access || resInfo.stages != req.stages) {
              madeBarrier = true;
              barriers.pre.image.push_back(barrier);
            }
          }
          if (checkHandoff(pass, barrier, resInfo)) {
            // Make the pass just before this one initiate the handoff
            passBarriers[resInfo.lastUsedInPass].post.image.push_back(barrier);
            barriers.needsWaitFor = resInfo.lastUsedInPass;
          }

          resInfo.layout = req.layout;
          resInfo.stages = req.stages;
          resInfo.access = req.access;
        } else {
          BufferBarrier barrier{
              .resourceId = req.resourceId,
              .srcStages = resInfo.stages,
              .dstStages = req.stages,
              .srcAccess = resInfo.access,
              .dstAccess = req.access,
          };
          resInfo.access = req.access;
          resInfo.stages = req.stages;

          if (checkHandoff(pass, barrier, resInfo)) {
            // Make the pass just before this one initiate the handoff
            passBarriers[resInfo.lastUsedInPass].post.buffer.push_back(barrier);
            barriers.needsWaitFor = resInfo.lastUsedInPass;
          }
          madeBarrier = true;
          barriers.pre.buffer.push_back(barrier);
        }

        if (madeBarrier)
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

    KT_DEBUG("Merged {} barriers across {} passes", mergedBarriers, passStack.size());

    // Need it in transfer src for the blit to swapchain.
    ImageBarrier barrier{
        .resourceId = backbufferPhysId,
        .srcStages = backbufferResInfo.stages,
        .dstStages = rhi::PipelineStageFlags::Blit,
        .srcAccess = backbufferResInfo.access,
        .dstAccess = rhi::AccessFlags::TransferRead,
        .oldLayout = backbufferResInfo.layout,
        .newLayout = optimalBlitSrc,
    };
    KT_ASSERT(backbufferResInfo.lastUsedInPass < passBarriers.size(),
              "Backbuffer resource was never used in any pass. This should have been caught earlier.");
    passBarriers[backbufferResInfo.lastUsedInPass].post.image.push_back(barrier);
  }
} // namespace kt