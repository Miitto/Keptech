#include "../builder.hpp"

#include <ranges>

template <> struct fmt::formatter<kt::QueueHandoff> : fmt::formatter<std::string_view> {
  template <typename FormatContext> auto format(const kt::QueueHandoff& handoff, FormatContext& ctx) const {
    switch (handoff) {
    case kt::QueueHandoff::No:
      return fmt::formatter<std::string_view>::format("No", ctx);
    case kt::QueueHandoff::ToCompute:
      return fmt::formatter<std::string_view>::format("ToCompute", ctx);
    case kt::QueueHandoff::FromCompute:
      return fmt::formatter<std::string_view>::format("FromCompute", ctx);
    }
  }
};

namespace kt {
  void RenderGraphBuilder::log() const {
    KT_DEBUG("Render Graph Log:");
    KT_DEBUG("  Passes (Used/Total): {}/{}", passStack.size(), passes.size());
    KT_DEBUG("  Resources (Phys/Virtual): {}/{}", physicalResourceInfos.size(), resources.size());
    KT_DEBUG("  Backbuffer Source: {}", backbufferSource);
    KT_DEBUG("");
    KT_DEBUG("  Pass Execution Order:");
    KT_DEBUG("    Idx: Name (Id)");
    for (const auto& [idx, passPtr] : passStack | std::views::enumerate) {
      const auto& pass = *passPtr;
      KT_DEBUG("    {}: {}", idx, pass.getName());
    }
    KT_DEBUG("");

#if KT_LOG_LEVEL <= SPDLOG_LEVEL_DEBUG
#define VERBOSE(...) KT_DEBUG(__VA_ARGS__)
    VERBOSE("  Requirements:");
    for (const auto& [idx, passPtr] : passStack | std::views::enumerate) {
      const auto& pass = *passPtr;
      const auto& reqs = passRequirements[static_cast<size_t>(idx)];

      VERBOSE("    {}: {}", idx, pass.getName());
      VERBOSE("      Read Requirements: {}", reqs.invalidate.size());
      for (const auto& req : reqs.invalidate) {
        VERBOSE("        Resource: {}{}", physicalResourceInfos[req.resourceId].name, req.history ? " (History)" : "");
        VERBOSE("          Layout: {}", req.layout);
#ifdef KT_VULKAN
        VERBOSE("          Access: {}", VkAccessFlags2Formatter(req.access));
        VERBOSE("          Stages: {}", VkPipelineStageFlags2Formatter(req.stages));
#endif
      }
      VERBOSE("");

      VERBOSE("      Write Requirements: {}", reqs.flush.size());
      for (const auto& req : reqs.flush) {
        VERBOSE("        Resource: {}{}", physicalResourceInfos[req.resourceId].name, req.history ? " (History)" : "");
        VERBOSE("          Layout: {}", req.layout);
#ifdef KT_VULKAN
        VERBOSE("          Access: {}", VkAccessFlags2Formatter(req.access));
        VERBOSE("          Stages: {}", VkPipelineStageFlags2Formatter(req.stages));
#endif
      }
      VERBOSE("");
    }
    VERBOSE("  Barriers:");

    for (const auto& [idx, passPtr] : passStack | std::views::enumerate) {
      const auto& pass = *passPtr;
      const auto& barriers = passBarriers[static_cast<size_t>(idx)];

      VERBOSE("    {}: {}", idx, pass.getName());
      if (!barriers.pre.image.empty()) {
        VERBOSE("      Pre-pass Image Barriers: {}", barriers.pre.image.size());
        for (const auto& barrier : barriers.pre.image) {
          const auto& res = physicalResourceInfos[barrier.resourceId];
          VERBOSE("        Resource: {}", res.name);
          VERBOSE("          Old Layout: {}", barrier.oldLayout);
          VERBOSE("          New Layout: {}", barrier.newLayout);
#ifdef KT_VULKAN
          VERBOSE("          Src Stages: {}", VkPipelineStageFlags2Formatter(barrier.srcStages));
          VERBOSE("          Src Access: {}", VkAccessFlags2Formatter(barrier.srcAccess));
          VERBOSE("          Dst Stages: {}", VkPipelineStageFlags2Formatter(barrier.dstStages));
          VERBOSE("          Dst Access: {}", VkAccessFlags2Formatter(barrier.dstAccess));
#endif
          if (barrier.handoff != QueueHandoff::No) {
            VERBOSE("          Handoff: {}", barrier.handoff);
          }
        }
        VERBOSE("");
      }

      if (!barriers.pre.buffer.empty()) {
        VERBOSE("      Pre-pass Buffer Barriers: {}", barriers.pre.buffer.size());
        for (const auto& barrier : barriers.pre.buffer) {
          const auto& res = physicalResourceInfos[barrier.resourceId];
          VERBOSE("        Resource: {}", res.name);
#ifdef KT_VULKAN
          VERBOSE("          Src Stages: {}", VkPipelineStageFlags2Formatter(barrier.srcStages));
          VERBOSE("          Src Access: {}", VkAccessFlags2Formatter(barrier.srcAccess));
          VERBOSE("          Dst Stages: {}", VkPipelineStageFlags2Formatter(barrier.dstStages));
          VERBOSE("          Dst Access: {}", VkAccessFlags2Formatter(barrier.dstAccess));
#endif
          if (barrier.handoff != QueueHandoff::No) {
            VERBOSE("          Handoff: {}", barrier.handoff);
          }
        }
        VERBOSE("");
      }

      if (!barriers.post.image.empty()) {
        VERBOSE("      Post-pass Image Barriers: {}", barriers.post.image.size());
        for (const auto& barrier : barriers.post.image) {
          const auto& res = physicalResourceInfos[barrier.resourceId];
          VERBOSE("        Resource: {}", res.name);
          VERBOSE("          Old Layout: {}", barrier.oldLayout);
          VERBOSE("          New Layout: {}", barrier.newLayout);
#ifdef KT_VULKAN
          VERBOSE("          Src Stages: {}", VkPipelineStageFlags2Formatter(barrier.srcStages));
          VERBOSE("          Src Access: {}", VkAccessFlags2Formatter(barrier.srcAccess));
          VERBOSE("          Dst Stages: {}", VkPipelineStageFlags2Formatter(barrier.dstStages));
          VERBOSE("          Dst Access: {}", VkAccessFlags2Formatter(barrier.dstAccess));
#endif
          if (barrier.handoff != QueueHandoff::No) {
            VERBOSE("          Handoff: {}", barrier.handoff);
          }
        }
        VERBOSE("");
      }

      if (!barriers.post.buffer.empty()) {
        VERBOSE("      Post-pass Buffer Barriers: {}", barriers.post.buffer.size());
        for (const auto& barrier : barriers.post.buffer) {
          const auto& res = physicalResourceInfos[barrier.resourceId];
          VERBOSE("        Resource: {}", res.name);
#ifdef KT_VULKAN
          VERBOSE("          Src Stages: {}", VkPipelineStageFlags2Formatter(barrier.srcStages));
          VERBOSE("          Src Access: {}", VkAccessFlags2Formatter(barrier.srcAccess));
          VERBOSE("          Dst Stages: {}", VkPipelineStageFlags2Formatter(barrier.dstStages));
          VERBOSE("          Dst Access: {}", VkAccessFlags2Formatter(barrier.dstAccess));
#endif
          if (barrier.handoff != QueueHandoff::No) {
            VERBOSE("          Handoff: {}", barrier.handoff);
          }
        }
        VERBOSE("");
      }
    }
#undef VERBOSE
#endif
  }

  void RenderGraphBuilder::validatePasses() const {
    KT_REQUIRE(!backbufferSource.empty(), "Backbuffer source must be set before baking the render graph.");
    auto it = resourceNameToId.find(backbufferSource);
    KT_REQUIRE(it != resourceNameToId.end(), "Backbuffer source '{}' not found in render graph", backbufferSource);

    auto& backbuffer = *resources[it->second];

    KT_REQUIRE(!backbuffer.getWritePasses().empty(), "Backbuffer source '{}' must be written by at least one pass", backbufferSource);

    for (const auto& passPtr : passes) {
      const auto& pass = *passPtr;

      KT_REQUIRE(pass.validate(), "Pass '{}' failed validation. Check that all required resources are present.", pass.getName());

      KT_REQUIRE(pass.getColorOutputs().size() == pass.getColorInputs().size(), "Pass '{}': Size of color inputs and outputs must match",
                 pass.getName());

      KT_REQUIRE(pass.getStorageImageOutputs().size() == pass.getStorageImageInputs().size(),
                 "Pass '{}': Size of storage image inputs and outputs must match", pass.getName());

      KT_REQUIRE(pass.getStorageInputs().size() == pass.getStorageOutputs().size(),
                 "Pass '{}': Size of storage inputs and outputs must match", pass.getName());

      if (pass.getQueue() == QueueType::Graphics) {
        KT_REQUIRE(!pass.getColorOutputs().empty() || pass.getDepthStencilOutput() != nullptr,
                   "Graphics Pass {} does not output any color or depth/stencil attachments.", pass.getName());
      }

      for (const auto& [idx, output] : pass.getColorOutputs() | std::views::enumerate) {
        auto* input = pass.getColorInputs()[static_cast<size_t>(idx)];
        if (!input)
          continue;

        KT_REQUIRE(
            input->getAttachmentInfo().format != rhi::ImageFormat::Undefined,
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
        KT_REQUIRE(output->getBufferInfo().usage == input->getBufferInfo().usage,
                   "Pass '{}': Storage output '{}' and input '{}' must have the same usage flags", pass.getName(), output->getName(),
                   input->getName());
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
} // namespace kt