#include "graph.hpp"

#include "keptech/core/kt-logger.hpp"
#include <ranges>

namespace kt::render_graph {
  void RenderGraph::addAttachment(const AttachmentInfo& attachment) { attachments[attachment.name] = attachment; }

  PassId RenderGraph::addPass(const Pass& pass) {
    passes.push_back(pass);
    return PassId{passes.size() - 1};
  }

  [[nodiscard]] const std::unordered_map<std::string, AttachmentInfo>& RenderGraph::getAttachments() const { return attachments; }

  [[nodiscard]] const std::vector<Pass>& RenderGraph::getPasses() const { return passes; }

  [[nodiscard]] const std::vector<PassId>& RenderGraph::getExecutionOrder() const { return executionOrder; }

  void RenderGraph::compile() {
    std::vector<std::vector<size_t>> dependencies(passes.size());
    std::vector<std::vector<size_t>> dependents(passes.size());

    std::unordered_map<std::string, size_t> attachmentWrites;

    for (auto [idx, pass] : passes | std::views::enumerate) {
      switch (pass.type) {
      case PassType::Graphics:
        passTypeCounts.graphicsCount++;
        break;
      case PassType::Compute:
        passTypeCounts.computeCount++;
        break;
      case PassType::AsyncCompute:
        passTypeCounts.asyncComputeCount++;
        break;
      }

      for (const auto& input : pass.inputs) {
        auto it = attachmentWrites.find(input);
        if (it != attachmentWrites.end()) {
          dependencies[idx].push_back(it->second);
          dependents[it->second].push_back(idx);
        }
      }

      for (const auto& output : pass.outputs) {
        attachmentWrites[output] = idx;
      }
    }

    std::vector<bool> visited(passes.size(), false);
    std::vector<bool> inStack(passes.size(), false);

    std::function<void(size_t)> visit = [&](size_t node) {
      if (inStack[node]) {
        KT_CRITICAL("Cycle detected in render graph at pass: {}", passes[node].name);
        abort();
      }

      if (visited[node]) {
        return;
      }

      inStack[node] = true;

      for (size_t dep : dependencies[node]) {
        visit(dep);
      }

      inStack[node] = false;
      visited[node] = true;
      executionOrder.emplace_back(node);
    };

    for (size_t i = 0; i < passes.size(); ++i) {
      if (!visited[i]) {
        visit(i);
      }
    }

    syncInfos.clear();
    syncInfos.resize(passes.size());

    {

      std::unordered_map<std::string, std::pair<rendering::ImageLayout, PassType>> attachmentLayouts;
      for (auto& i : attachments) {
        attachmentLayouts[i.first] = {rendering::ImageLayout::Undefined, PassType::Graphics};
      }

      for (auto i : executionOrder) {
        SyncInfo& syncInfo = syncInfos[i];

        for (const auto& inputName : passes[i].inputs) {
          auto attachment = getAttachment(inputName);
          auto it = attachmentLayouts.find(inputName);
          if (it == attachmentLayouts.end()) {
            KT_CRITICAL("Attachment not found: {}", inputName); // Should have pre-loaded them all
            abort();
          }

          auto [prevLayout, prevType] = it->second;
          rendering::Image::TransitionInfoType transition(attachment.image.type(), prevLayout, attachment.desiredLayout,
                                                          attachment.image.mips(), attachment.image.layers());
        }

        for (auto dep : dependencies[i]) {
          PassType depType = passes[dep].type;
          PassType currType = passes[i].type;

          if (depType != currType) {
            auto& depInfo = syncInfos[dep];
            depInfo.signal = true;
            syncInfo.waitOn = dep;
          }
        }
      }
    }
  }

  [[nodiscard]] const AttachmentInfo& RenderGraph::getAttachment(const std::string& name) const {
    auto it = attachments.find(name);
    if (it == attachments.end()) {
      KT_CRITICAL("Attachment not found: {}", name);
      abort();
    }
    return it->second;
  }
} // namespace kt::render_graph