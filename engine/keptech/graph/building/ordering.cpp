#include "../builder.hpp"

#include <ranges>

namespace kt {
  void RenderGraphBuilder::cullPasses() {
    passStack.reserve(passes.size());

    passDependencies.clear();
    passDependencies.resize(passes.size());

    auto& backbuffer = *resources[resourceNameToId.find(backbufferSource)->second];

    for (auto& pass : backbuffer.getWritePasses()) {
      passStack.push_back(passes[pass].get());
    }

    auto tmp = passStack;
    for (auto pass : tmp) {
      traverseDependencies(*pass, 0);
    }

    std::ranges::reverse(passStack);

    std::unordered_set<RenderPassBuilder*> seen;
    seen.reserve(passStack.size());

    passStack.retain([&](RenderPassBuilder* pass) {
      if (seen.contains(pass)) {
        return false;
      }
      seen.insert(pass);
      return true;
    });
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
    KT_REQUIRE(noCheck || !writtenPasses.empty(), "Pass '{}': No passes found that write to an input resource", self.getName());

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

      passStack.push_back(passes[pass].get());
      auto& depPass = *passes[pass];
      traverseDependencies(depPass, stackCount);
    }
  }

  void RenderGraphBuilder::reorderPasses() {
    if (passStack.size() <= 2)
      return;

    Vec<RenderPassBuilder*> unscheduled;
    unscheduled.reserve(passStack.size());
    std::swap(passStack, unscheduled);

    const auto& schedule = [&](size_t i) {
      passStack.push_back(unscheduled[i]);
      std::move(unscheduled.begin() + static_cast<Vec<RenderPassBuilder*>::iterator::difference_type>(i) + 1, unscheduled.end(),
                unscheduled.begin() + static_cast<Vec<RenderPassBuilder*>::iterator::difference_type>(i));
      unscheduled.pop_back();
    };

    schedule(PassId(0));

    while (!unscheduled.empty()) {
      size_t best = 0;
      size_t bestOverlap = 0;

      for (const auto& [idxL, pass] : unscheduled | std::views::enumerate) {
        /// Number of scheduled passes between the current pass and its first scheduled dependency.
        size_t overlap = 0;
        size_t idx = static_cast<size_t>(idxL);

        // Prefer maximum number of passes between the current pass and its first scheduled dependency, as this will reduce the number of
        // barriers needed.
        for (auto& candidate : passStack | std::views::reverse) {
          if (dependsOnPass(pass->getId(), candidate->getId()))
            break;

          ++overlap;
        }

        if (overlap < bestOverlap)
          continue;

        bool possible = true;

        for (size_t j = 0; j < idx; ++j) {
          if (dependsOnPass(pass->getId(), unscheduled[j]->getId())) {
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

    for (const auto& [idx, pass] : passStack | std::views::enumerate) {
      pass->setIndex(static_cast<size_t>(idx));
    }
  }
} // namespace kt