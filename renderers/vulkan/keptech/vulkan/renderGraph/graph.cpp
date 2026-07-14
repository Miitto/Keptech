#include "graph.hpp"

#include "helpers/transitions.hpp"
#include "keptech/vulkan/renderer.hpp"
#include "renderResources.hpp"
#include "vk-logger.hpp"
#include <algorithm>

namespace kt::vkh {
  static constexpr Bitflag<QueueType> COMPUTE_QUEUES = QueueType::Compute | QueueType::AsyncCompute;

  void RenderGraph::build() {
    for (auto& pass : passes)
      pass->setupDependencies();

    validatePasses();

    auto it = resourceNameToId.find(backbufferSource);
    VK_REQUIRE(it != resourceNameToId.end(), "Backbuffer source '{}' not found in render graph", backbufferSource);

    passStack.clear();
    passDependencies.clear();
    passDependencies.resize(passes.size());

    auto& backbuffer = *resources[it->second];

    VK_REQUIRE(!backbuffer.getWritePasses().empty(), "Backbuffer source '{}' must be written by at least one pass", backbufferSource);

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
    buildRenderPassInfo();
    buildBarriers();
  }

  void RenderGraph::validatePasses() const {
    for (const auto& passPtr : passes) {
      const auto& pass = *passPtr;

      VK_REQUIRE(pass.getColorOutputs().size() == pass.getColorInputs().size(), "Pass '{}': Size of color inputs and outputs must match",
                 pass.getName());

      VK_REQUIRE(pass.getStorageInputs().size() == pass.getStorageOutputs().size(),
                 "Pass '{}': Size of storage inputs and outputs must match", pass.getName());

      VK_REQUIRE(pass.getResolveOutputs().empty() || pass.getResolveOutputs().size() == pass.getColorOutputs().size(),
                 "Pass '{}': Must have a resolve output for each color output, if any", pass.getName());

      for (const auto& [idx, output] : pass.getStorageOutputs() | std::views::enumerate) {
        auto* input = pass.getStorageInputs()[idx];
        if (!input)
          continue;

        VK_REQUIRE(output->getBufferInfo().size == input->getBufferInfo().size,
                   "Pass '{}': Storage output '{}' and input '{}' must have the same size", pass.getName(), output->getName(),
                   input->getName());
        VK_REQUIRE(output->getBufferInfo().usage == input->getBufferInfo().usage,
                   "Pass '{}': Storage output '{}' and input '{}' must have the same usage flags", pass.getName(), output->getName(),
                   input->getName());
      }

      if (pass.getDepthStencilInput() && pass.getDepthStencilOutput()) {
        auto* input = pass.getDepthStencilInput();
        auto* output = pass.getDepthStencilOutput();

        VK_REQUIRE(input->getAttachmentInfo().format == output->getAttachmentInfo().format,
                   "Pass '{}': Depth-stencil input '{}' and output '{}' must have the same format", pass.getName(), input->getName(),
                   output->getName());
        VK_REQUIRE(input->getAttachmentInfo().samples == output->getAttachmentInfo().samples,
                   "Pass '{}': Depth-stencil input '{}' and output '{}' must have the same sample count", pass.getName(), input->getName(),
                   output->getName());
        VK_REQUIRE(input->getAttachmentInfo().layers == output->getAttachmentInfo().layers,
                   "Pass '{}': Depth-stencil input '{}' and output '{}' must have the same layer count", pass.getName(), input->getName(),
                   output->getName());
        VK_REQUIRE(input->getAttachmentInfo().mipLevels == output->getAttachmentInfo().mipLevels,
                   "Pass '{}': Depth-stencil input '{}' and output '{}' must have the same mip level count", pass.getName(),
                   input->getName(), output->getName());
        VK_REQUIRE(input->getAttachmentInfo().sizeType == output->getAttachmentInfo().sizeType,
                   "Pass '{}': Depth-stencil input '{}' and output '{}' must have the same size type", pass.getName(), input->getName(),
                   output->getName());
        VK_REQUIRE(input->getAttachmentInfo().size == output->getAttachmentInfo().size,
                   "Pass '{}': Depth-stencil input '{}' and output '{}' must have the same size", pass.getName(), input->getName(),
                   output->getName());
      }
    }
  }

  void RenderGraph::traverseDependencies(const RenderPass& pass, size_t stackCount) {
    if (pass.getDepthStencilInput()) {
      dependPassesRecursive(pass, pass.getDepthStencilInput()->getWritePasses(), stackCount, false, true, true);
    }

    for (auto* input : pass.getAttachmentInputs()) {
      bool selfDep = pass.getDepthStencilOutput() == input;
      if (std::ranges::find(pass.getColorOutputs(), input) != pass.getColorOutputs().end())
        selfDep = true;

      if (!selfDep)
        dependPassesRecursive(pass, input->getWritePasses(), stackCount, false, false, true);
    }

    for (auto* input : pass.getColorInputs()) {
      if (input)
        dependPassesRecursive(pass, input->getWritePasses(), stackCount, false, false, true);
    }

    for (auto& input : pass.getGenericTextureInputs()) {
      dependPassesRecursive(pass, input.texture->getWritePasses(), stackCount, false, false, false);
    }

    for (auto* input : pass.getStorageInputs()) {
      if (input) {
        dependPassesRecursive(pass, input->getWritePasses(), stackCount, true, false, false);
        dependPassesRecursive(pass, input->getReadPasses(), stackCount, true, true, false);
      }
    }

    for (auto& input : pass.getGenericBufferInputs()) {
      dependPassesRecursive(pass, input.buffer->getWritePasses(), stackCount, true, false, false);
    }
  }

  void RenderGraph::dependPassesRecursive(const RenderPass& self, const std::unordered_set<PassId>& writtenPasses, size_t stackCount,
                                          bool noCheck, bool ignoreSelf, bool mergeDeps) {
    VK_REQUIRE(noCheck || !writtenPasses.empty(), "Pass '{}': No passes found that write to the input resource", self.getName());

    VK_REQUIRE(stackCount <= passes.size(), "Pass '{}': Circular dependency detected in render graph", self.getName());

    for (auto& pass : writtenPasses)
      if (pass != self.getId())
        passDependencies[self.getId()].insert(pass);

    ++stackCount;

    for (auto& pass : writtenPasses) {

      if (ignoreSelf && pass == self.getId())
        continue;
      else
        VK_REQUIRE(pass != self.getId(), "Pass '{}' depends on itself", self.getName());

      passStack.push_back(pass);
      auto& depPass = *passes[pass];
      traverseDependencies(depPass, stackCount);
    }
  }

  void RenderGraph::filterPasses(std::vector<PassId>& passOrder) {
    std::unordered_set<PassId> seen;

    auto [first, last] = std::ranges::remove_if(passOrder, [&](PassId id) {
      if (seen.contains(id))
        return true;

      seen.insert(id);
      return false;
    });
    passOrder.erase(first, last);
  }

  void RenderGraph::reorderPasses(std::vector<PassId>& passOrder) {
    if (passOrder.size() <= 2)
      return;

    std::vector<PassId> unscheduled;
    unscheduled.reserve(passOrder.size());
    std::swap(passOrder, unscheduled);

    const auto& schedule = [&](PassId id) {
      passOrder.push_back(unscheduled[id]);
      std::move(unscheduled.begin() + *id + 1, unscheduled.end(), unscheduled.begin() + *id);
      unscheduled.pop_back();
    };

    schedule(PassId(0));

    while (!unscheduled.empty()) {
      PassId best{};
      size_t bestOverlap = 0;

      for (const auto& [idx, pass] : unscheduled | std::views::enumerate) {
        size_t overlap = 0;

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

        best = pass;
        bestOverlap = overlap;
      }

      schedule(best);
    }
  }

  void RenderGraph::buildPhysicalResources() {
    PhysResourceId physId{0};

    for (const auto& [idx, passId] : passStack | std::views::enumerate) {
      auto& pass = *passes[passId];
      for (auto& input : pass.getGenericTextureInputs()) {
        if (!input.texture->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*input.texture));
          input.texture->setPhysicalId(physId++);
        } else {
          physicalResourceInfos[input.texture->getPhysicalId()].queues |= input.texture->getUsedQueues();
          physicalResourceInfos[input.texture->getPhysicalId()].imageUsage |= input.texture->getImageUsage();
        }
      }

      for (auto& input : pass.getGenericBufferInputs()) {
        if (!input.buffer->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*input.buffer));
          input.buffer->setPhysicalId(physId++);
        } else {
          physicalResourceInfos[input.buffer->getPhysicalId()].queues |= input.buffer->getUsedQueues();
          physicalResourceInfos[input.buffer->getPhysicalId()].bufferInfo.usage |= input.buffer->getBufferUsage();
        }
      }

      for (const auto& [jdx, inputPtr] : pass.getColorInputs() | std::views::enumerate) {
        if (!inputPtr)
          continue;
        auto& input = *inputPtr;

        if (!input.getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(input));
          input.setPhysicalId(physId++);
        } else {
          physicalResourceInfos[input.getPhysicalId()].queues |= input.getUsedQueues();
          physicalResourceInfos[input.getPhysicalId()].imageUsage |= input.getImageUsage();
        }

        VK_REQUIRE(!pass.getColorOutputs()[jdx]->getPhysicalId().used(),
                   "Pass '{}': Cannot alias color output '{}'. Physical ID already claimed.", pass.getName(),
                   pass.getColorOutputs()[jdx]->getName());
        pass.getColorOutputs()[jdx]->setPhysicalId(input.getPhysicalId());
      }

      for (const auto& [jdx, inputPtr] : pass.getStorageInputs() | std::views::enumerate) {
        if (!inputPtr)
          continue;
        auto& input = *inputPtr;

        if (!input.getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(input));
          input.setPhysicalId(physId++);
        } else {
          physicalResourceInfos[input.getPhysicalId()].queues |= input.getUsedQueues();
          physicalResourceInfos[input.getPhysicalId()].bufferInfo.usage |= input.getBufferUsage();
        }

        VK_REQUIRE(!pass.getStorageOutputs()[jdx]->getPhysicalId().used(),
                   "Pass '{}': Cannot alias storage output '{}'. Physical ID already claimed.", pass.getName(),
                   pass.getStorageOutputs()[jdx]->getName());
        pass.getStorageOutputs()[jdx]->setPhysicalId(input.getPhysicalId());
      }

      for (auto* output : pass.getColorOutputs()) {
        if (!output->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*output));
          output->setPhysicalId(physId++);
        } else {
          physicalResourceInfos[output->getPhysicalId()].queues |= output->getUsedQueues();
          physicalResourceInfos[output->getPhysicalId()].imageUsage |= output->getImageUsage();
        }
      }

      for (auto* output : pass.getStorageOutputs()) {
        if (!output->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*output));
          output->setPhysicalId(physId++);
        } else {
          physicalResourceInfos[output->getPhysicalId()].queues |= output->getUsedQueues();
          physicalResourceInfos[output->getPhysicalId()].bufferInfo.usage |= output->getBufferUsage();
        }
      }

      for (auto* output : pass.getResolveOutputs()) {
        if (!output->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*output));
          output->setPhysicalId(physId++);
        } else {
          physicalResourceInfos[output->getPhysicalId()].queues |= output->getUsedQueues();
          physicalResourceInfos[output->getPhysicalId()].imageUsage |= output->getImageUsage();
        }
      }

      for (auto* output : pass.getTransferOutputs()) {
        if (!output->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*output));
          output->setPhysicalId(physId++);
        } else {
          physicalResourceInfos[output->getPhysicalId()].queues |= output->getUsedQueues();
          physicalResourceInfos[output->getPhysicalId()].bufferInfo.usage |= output->getBufferUsage();
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
          physicalResourceInfos[dsInput->getPhysicalId()].imageUsage |= dsInput->getImageUsage();
        }

        if (dsOutput) {
          VK_REQUIRE(!dsOutput->getPhysicalId().used(), "Pass '{}': Cannot alias depth-stencil output '{}'. Physical ID already claimed.",
                     pass.getName(), dsOutput->getName());
          dsOutput->setPhysicalId(dsInput->getPhysicalId());

          physicalResourceInfos[dsInput->getPhysicalId()].queues |= dsOutput->getUsedQueues();
          physicalResourceInfos[dsInput->getPhysicalId()].imageUsage |= dsOutput->getImageUsage();
        }
      } else if (dsOutput) {
        if (!dsOutput->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*dsOutput));
          dsOutput->setPhysicalId(physId++);
        } else {
          physicalResourceInfos[dsOutput->getPhysicalId()].queues |= dsOutput->getUsedQueues();
          physicalResourceInfos[dsOutput->getPhysicalId()].imageUsage |= dsOutput->getImageUsage();
        }
      }

      for (auto* input : pass.getAttachmentInputs()) {
        if (!input->getPhysicalId().used()) {
          physicalResourceInfos.push_back(getResourceInfo(*input));
          input->setPhysicalId(physId++);
        } else {
          physicalResourceInfos[input->getPhysicalId()].queues |= input->getUsedQueues();
          physicalResourceInfos[input->getPhysicalId()].imageUsage |= input->getImageUsage();
        }
      }
    }

    physicalImageHasHistory.clear();
    physicalImageHasHistory.resize(physicalResourceInfos.size());

    for (auto& passId : passStack) {
      auto& pass = *passes[passId];

      for (auto& history : pass.getHistoryInputs()) {
        VK_REQUIRE(history->getPhysicalId().used(), "Pass '{}': History input '{}' is used, but is never written to.", pass.getName(),
                   history->getName());
        physicalImageHasHistory[history->getPhysicalId()] = true;
      }
    }
  }

  void RenderGraph::buildRenderPassInfo() {
    for (auto passId : passStack) {
      auto& pass = *passes[passId];

      RenderPassInfo info{};

      auto& colors = info.colorOutputs;

      const auto addUniqueColor = [&](PhysResourceId id) {
        auto it = std::ranges::find(colors, id);
        if (it != colors.end())
          return std::make_pair(size_t(it - colors.begin()), false);

        colors.push_back(id);
        return std::make_pair(colors.size() - 1, true);
      };

      const auto addUniqueAttch = [&](PhysResourceId id) {
        if (id == info.depthStencilOutput)
          return std::make_pair(colors.size(), false);
        return addUniqueColor(id);
      };

      size_t numColorAttachments = pass.getColorOutputs().size();
      for (size_t i = 0; i < numColorAttachments; ++i) {
        auto res = addUniqueColor(pass.getColorOutputs()[i]->getPhysicalId());

        if (res.second) {
          bool hasInput = !pass.getColorInputs().empty() && pass.getColorInputs()[i] != nullptr;

          if (!hasInput) {
            if (pass.getClearColor(i)) {
              info.clearAttachmentMask |= (1u << res.first);
            }
          } else {
            info.loadAttachmentMask |= (1u << res.first);
          }
        }
      }

      auto* dsInput = pass.getDepthStencilInput();
      auto* dsOutput = pass.getDepthStencilOutput();

      const auto addUniqueDs = [&](PhysResourceId id) {
        VK_ASSERT(!info.depthStencilOutput.used() || info.depthStencilOutput == id,
                  "Pass '{}': Cannot alias depth-stencil output '{}'. Physical ID already claimed.", pass.getName(), dsOutput->getName());

        bool newAttch = !info.depthStencilOutput.used();
        info.depthStencilOutput = id;
        return std::make_pair(id, newAttch);
      };

      if (dsInput && dsOutput) {
        auto res = addUniqueDs(dsOutput->getPhysicalId());

        if (res.second)
          info.depthStencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

        info.depthStencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;

      } else if (dsOutput) {
        auto res = addUniqueDs(dsOutput->getPhysicalId());

        if (res.second && pass.getClearDepthStencil()) {
          info.depthStencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        } else if (res.second) {
          info.depthStencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        }

        info.depthStencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
      } else if (dsInput) {
        auto res = addUniqueDs(dsInput->getPhysicalId());

        if (res.second)
          info.depthStencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

        const auto checkPreserve = [&](const RenderResource& tex) {
          for (auto& readPassId : tex.getReadPasses()) {
            if (readPassId == pass.getId())
              continue;

            if (passes[readPassId]->getIndex() > pass.getIndex())
              return true;
          }
          return false;
        };

        bool preserve = checkPreserve(*dsInput);
        if (preserve) {
          info.depthStencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        } else {
          info.depthStencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        }
      }

      size_t numInputAttch = pass.getAttachmentInputs().size();
      for (size_t i = 0; i < numInputAttch; ++i) {
        auto res = addUniqueAttch(pass.getAttachmentInputs()[i]->getPhysicalId());

        if (res.second)
          info.loadAttachmentMask |= (1u << res.first);
      }
    }
  }

  void RenderGraph::buildBarriers() {
    passBarriers.clear();
    passBarriers.reserve(passStack.size());

    const auto getAccess = [&](std::vector<Barrier>& barriers, PhysResourceId id, bool history) -> Barrier& {
      auto it = std::ranges::find_if(barriers, [&](const Barrier& b) { return b.resourceId == id && history == b.history; });
      if (it != barriers.end())
        return *it;

      barriers.push_back({.resourceId = id, .layout = VK_IMAGE_LAYOUT_UNDEFINED, .access = 0, .stages = 0, .history = history});
      return barriers.back();
    };

    for (auto& passId : passStack) {
      auto& pass = *passes[passId];
      Barriers barriers;

      const auto getInvalidAccess = [&](PhysResourceId id, bool history) -> Barrier& {
        return getAccess(barriers.invalidate, id, history);
      };

      const auto getFlushAccess = [&](PhysResourceId id, bool history) -> Barrier& { return getAccess(barriers.flush, id, history); };

      for (auto& input : pass.getGenericBufferInputs()) {
        auto& barrier = getInvalidAccess(input.buffer->getPhysicalId(), false);
        barrier.access |= input.access;
        barrier.stages |= input.stages;
        VK_REQUIRE(barrier.layout == VK_IMAGE_LAYOUT_UNDEFINED, "Pass '{}': Buffer input '{}' expected to have undefined layout",
                   pass.getName(), input.buffer->getName());
        barrier.layout = input.layout;
      }

      for (auto& input : pass.getGenericTextureInputs()) {
        auto& barrier = getInvalidAccess(input.texture->getPhysicalId(), false);
        barrier.access |= input.access;
        barrier.stages |= input.stages;
        VK_REQUIRE(barrier.layout == VK_IMAGE_LAYOUT_UNDEFINED, "Pass '{}': Texture input '{}' expected to have undefined layout",
                   pass.getName(), input.texture->getName());
        barrier.layout = input.layout;
      }

      for (auto& input : pass.getHistoryInputs()) {
        auto& barrier = getInvalidAccess(input->getPhysicalId(), true);
        barrier.access |= VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;

        if (!pass.getQueues().intersects(COMPUTE_QUEUES)) {
          barrier.stages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        } else {
          barrier.stages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }

        VK_REQUIRE(barrier.layout == VK_IMAGE_LAYOUT_UNDEFINED, "Pass '{}': History input '{}' expected to have undefined layout",
                   pass.getName(), input->getName());
        barrier.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      }

      for (auto* input : pass.getAttachmentInputs()) {
        VK_REQUIRE(!pass.getQueues().intersects(COMPUTE_QUEUES), "Pass '{}': Attachment inputs cannot be used in a compute pass",
                   pass.getName(), input->getName());

        auto& barrier = getInvalidAccess(input->getPhysicalId(), false);
        barrier.access |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
        barrier.stages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

        switch (input->getAttachmentInfo().format) {
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
        case VK_FORMAT_S8_UINT:
          barrier.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
          barrier.stages |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
          break;
        default:
          barrier.access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
          barrier.stages |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
          break;
        }

        VK_REQUIRE(barrier.layout == VK_IMAGE_LAYOUT_UNDEFINED, "Pass '{}': Attachment input '{}' expected to have undefined layout",
                   pass.getName(), input->getName());
        barrier.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      }

      for (auto* input : pass.getStorageInputs()) {
        auto& barrier = getInvalidAccess(input->getPhysicalId(), false);
        barrier.access |= VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

        if (!pass.getQueues().intersects(COMPUTE_QUEUES)) {
          barrier.stages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        } else {
          barrier.stages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }

        VK_REQUIRE(barrier.layout == VK_IMAGE_LAYOUT_UNDEFINED, "Pass '{}': Storage input '{}' expected to have undefined layout",
                   pass.getName(), input->getName());
        barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
      }

      for (auto* input : pass.getColorInputs()) {
        if (!input)
          continue;

        VK_REQUIRE(!pass.getQueues().intersects(COMPUTE_QUEUES), "Pass '{}': Color inputs cannot be used in a compute pass", pass.getName(),
                   input->getName());

        auto& barrier = getInvalidAccess(input->getPhysicalId(), false);
        barrier.access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.stages |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

        // Also used as an input attachment for programmable blending.
        if (barrier.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
          barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
        } else {
          VK_REQUIRE(barrier.layout == VK_IMAGE_LAYOUT_UNDEFINED, "Pass '{}': Color input '{}' expected to have undefined layout",
                     pass.getName(), input->getName());
          barrier.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
      }

      for (auto* output : pass.getColorOutputs()) {
        VK_REQUIRE(!pass.getQueues().intersects(COMPUTE_QUEUES), "Pass '{}': Color outputs cannot be used in a compute pass",
                   pass.getName(), output->getName());

        auto barrier = getFlushAccess(output->getPhysicalId(), false);
        barrier.access |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.stages |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

        if (barrier.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL || barrier.layout == VK_IMAGE_LAYOUT_GENERAL) {
          barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
        } else {
          VK_REQUIRE(barrier.layout == VK_IMAGE_LAYOUT_UNDEFINED, "Pass '{}': Color output '{}' expected to have undefined layout",
                     pass.getName(), output->getName());
          barrier.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
      }

      for (auto* output : pass.getResolveOutputs()) {
        VK_REQUIRE(!pass.getQueues().intersects(COMPUTE_QUEUES), "Pass '{}': Resolve outputs cannot be used in a compute pass",
                   pass.getName(), output->getName());

        auto barrier = getFlushAccess(output->getPhysicalId(), false);
        barrier.access |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.stages |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        VK_REQUIRE(barrier.layout == VK_IMAGE_LAYOUT_UNDEFINED, "Pass '{}': Resolve output '{}' expected to have undefined layout",
                   pass.getName(), output->getName());
        barrier.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      }

      for (auto* output : pass.getStorageOutputs()) {
        auto barrier = getFlushAccess(output->getPhysicalId(), false);
        barrier.access |= VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;

        if (!pass.getQueues().intersects(COMPUTE_QUEUES)) {
          barrier.stages |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        } else {
          barrier.stages |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        }

        VK_REQUIRE(barrier.layout == VK_IMAGE_LAYOUT_UNDEFINED, "Pass '{}': Storage output '{}' expected to have undefined layout",
                   pass.getName(), output->getName());
        barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
      }

      for (auto* output : pass.getTransferOutputs()) {
        auto barrier = getFlushAccess(output->getPhysicalId(), false);
        barrier.access |= VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.stages |= VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_CLEAR_BIT;
        VK_REQUIRE(barrier.layout == VK_IMAGE_LAYOUT_UNDEFINED, "Pass '{}': Transfer output '{}' expected to have undefined layout",
                   pass.getName(), output->getName());
        barrier.layout = VK_IMAGE_LAYOUT_GENERAL;
      }

      auto* dsInput = pass.getDepthStencilInput();
      auto* dsOutput = pass.getDepthStencilOutput();

      if (dsInput || dsOutput) {
        VK_REQUIRE(!pass.getQueues().intersects(COMPUTE_QUEUES), "Pass '{}': Depth-stencil inputs/outputs cannot be used in a compute pass",
                   pass.getName(), dsInput ? dsInput->getName() : dsOutput->getName());
      }

      if (dsInput && dsOutput) {
        auto& dstBarrier = getInvalidAccess(dsInput->getPhysicalId(), false);
        auto& srcBarrier = getFlushAccess(dsOutput->getPhysicalId(), false);

        if (dstBarrier.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
          dstBarrier.layout = VK_IMAGE_LAYOUT_GENERAL;
        } else {
          VK_REQUIRE(dstBarrier.layout == VK_IMAGE_LAYOUT_UNDEFINED,
                     "Pass '{}': Depth-stencil output '{}' expected to have undefined layout", pass.getName(), dsOutput->getName());
          dstBarrier.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        dstBarrier.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dstBarrier.stages |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;

        srcBarrier.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        srcBarrier.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        srcBarrier.stages |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      } else if (dsInput) {
        auto& dstBarrier = getInvalidAccess(dsInput->getPhysicalId(), false);

        if (dstBarrier.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
          dstBarrier.layout = VK_IMAGE_LAYOUT_GENERAL;
        } else {
          VK_REQUIRE(dstBarrier.layout == VK_IMAGE_LAYOUT_UNDEFINED,
                     "Pass '{}': Depth-stencil input '{}' expected to have undefined layout", pass.getName(), dsInput->getName());
          dstBarrier.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        dstBarrier.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
        dstBarrier.stages |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      } else if (dsOutput) {
        auto& srcBarrier = getFlushAccess(dsOutput->getPhysicalId(), false);

        if (srcBarrier.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
          srcBarrier.layout = VK_IMAGE_LAYOUT_GENERAL;
        } else {
          VK_REQUIRE(srcBarrier.layout == VK_IMAGE_LAYOUT_UNDEFINED,
                     "Pass '{}': Depth-stencil output '{}' expected to have undefined layout", pass.getName(), dsOutput->getName());
          srcBarrier.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        srcBarrier.access |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        srcBarrier.stages |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
      }

      passBarriers.push_back(std::move(barriers));
    }
  }

  RenderTextureResource& RenderGraph::getTextureResource(const std::string& name) {
    auto it = resourceNameToId.find(name);
    if (it != resourceNameToId.end()) {
      VK_ASSERT(resources[it->second]->getType() == RenderResource::Type::Texture, "Resource with name '{}' is not a texture", name);
      return static_cast<RenderTextureResource&>(*resources[it->second]); // NOLINT
    }

    ResourceId id{resources.size()};
    auto res = std::make_unique<RenderTextureResource>(id);
    res->setName(name);
    resources.push_back(std::move(res));
    resourceNameToId[name] = id;

    return static_cast<RenderTextureResource&>(*resources.back()); // NOLINT
  }

  RenderBufferResource& RenderGraph::getBufferResource(const std::string& name) {
    auto it = resourceNameToId.find(name);
    if (it != resourceNameToId.end()) {
      VK_ASSERT(resources[it->second]->getType() == RenderResource::Type::Buffer, "Resource with name '{}' is not a buffer", name);
      return static_cast<RenderBufferResource&>(*resources[it->second]); // NOLINT
    }

    ResourceId id{resources.size()};
    auto res = std::make_unique<RenderBufferResource>(id);
    res->setName(name);
    resources.push_back(std::move(res));
    resourceNameToId[name] = id;

    return static_cast<RenderBufferResource&>(*resources.back()); // NOLINT
  }

  RenderPass& RenderGraph::addPass(const std::string& name, Bitflag<QueueType> queueTypes) {
    auto it = passNameToId.find(name);
    if (it != passNameToId.end()) {
      return *passes[it->second];
    }

    PassId id{passes.size()};
    auto pass = std::make_unique<RenderPass>(*this, id, queueTypes);
    pass->setName(name);
    passes.push_back(std::move(pass));
    passNameToId[name] = id;

    return *passes.back();
  }

  RenderPass* RenderGraph::findPass(const std::string& name) {
    auto it = passNameToId.find(name);
    if (it != passNameToId.end()) {
      return passes[it->second].get();
    }
    return nullptr;
  }

  bool RenderGraph::dependsOnPass(PassId dst, PassId src) const {
    if (dst == src)
      return true;

    for (auto& dep : passDependencies[dst]) {
      if (dependsOnPass(dep, src))
        return true;
    }

    return false;
  }

  ResourceInfo RenderGraph::getResourceInfo(RenderTextureResource& resource) const {
    auto& info = resource.getAttachmentInfo();
    ResourceInfo resInfo{
        .name = resource.getName(),
        .format = info.format,
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

    if (resInfo.format == VK_FORMAT_UNDEFINED)
      resInfo.format = swapchainFormat;

    const auto numLevels = [](uint32_t width, uint32_t height, uint32_t depth) {
      return static_cast<uint32_t>(std::floor(std::log2(std::max(std::max(width, height), depth)))) + 1;
    };

    resInfo.mipLevels =
        std::min(numLevels(resInfo.size.x, resInfo.size.y, resInfo.size.z), resInfo.mipLevels == 0 ? ~0u : resInfo.mipLevels);

    return resInfo;
  }

  ResourceInfo RenderGraph::getResourceInfo(RenderBufferResource& resource) const {
    auto& info = resource.getBufferInfo();
    ResourceInfo resInfo{
        .name = resource.getName(),
        .bufferInfo = info,
        .persistent = info.persistent,
    };
    resInfo.bufferInfo.usage |= resource.getBufferUsage();
    return resInfo;
  }
} // namespace kt::vkh