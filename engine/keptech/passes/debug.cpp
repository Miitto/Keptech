#include "debug.hpp"

#include "keptech/graph/builder.hpp"
#include "keptech/graph/graph.hpp"
#include "keptech/rhi/cmdBuf.hpp"
#include <imgui/imgui.h>

namespace kt {
  void DebugPass::setupDependencies(RenderPassBuilder& self, RenderGraphBuilder&) {
    self.addColorOutput("kt::debug", {.format = rhi::ImageFormat::R8G8B8A8_UNORM});

    self.addTextureInput(backbufferSource);
  }

  void DebugPass::setup(RenderGraph& graph, const rhi::DescriptorLayout&) {
    debugImageIndex = graph.getImageIndex("kt::debug");
    auto& images = graph.getImages();

    for (size_t i = 0; i < images.size(); ++i) {
      if (images[i].getUsage().has(rhi::ImageUsage::RenderTarget)) {
        renderTargets.push_back(i);
      }
    }

    debugView = graph.getImageIndex(backbufferSource);
  }

  void DebugPass::execute(RenderGraph& graph, rhi::CommandBuffer& cmdBuf, const rhi::DescriptorSet&, glm::uvec2) {
    auto& debugImage = graph.getImage(debugImageIndex);

    {
      auto& srcImage = graph.getImage(debugView);
      if (ImGui::Begin("Debug View")) {

        ImGui::Text("Select Render Target");
        if (ImGui::BeginCombo("Render Target", srcImage.getName().c_str())) {
          for (size_t i = 0; i < renderTargets.size(); ++i) {
            auto& image = graph.getImage(renderTargets[i]);

            if (image == debugImage) {
              continue;
            }

            bool isSelected = (debugView == renderTargets[i]);
            if (ImGui::Selectable(image.getName().c_str(), isSelected)) {
              debugView = renderTargets[i];
            }
            if (isSelected) {
              ImGui::SetItemDefaultFocus();
            }
          }
          ImGui::EndCombo();
        }

        ImGui::End();
      }
    }

    auto& srcImage = graph.getImage(debugView);

    if (srcImage == debugImage) {
      KT_WARN("Debug pass source image is the same as the debug output image.");
      return;
    }

    cmdBuf.transitionImage(debugImage, rhi::ImageLayout::RenderTarget, rhi::ImageLayout::TransferDst);
    cmdBuf.transitionImage(srcImage,
                           srcImage.getName() == backbufferSource ? rhi::ImageLayout::ShaderReadOnly : rhi::ImageLayout::RenderTarget,
                           rhi::ImageLayout::TransferSrc);

    cmdBuf.blitImage(srcImage, debugImage);

    cmdBuf.transitionImage(debugImage, rhi::ImageLayout::TransferDst, rhi::ImageLayout::RenderTarget);
    cmdBuf.transitionImage(srcImage, rhi::ImageLayout::TransferSrc,
                           srcImage.getName() == backbufferSource ? rhi::ImageLayout::ShaderReadOnly : rhi::ImageLayout::RenderTarget);
  }

  void DebugPass::addToGraph(RenderGraphBuilder& graph) {
    auto& pass = graph.addPass("kt::debug");
    pass.setInterface(this);

    backbufferSource = graph.getBackbufferSource();
    graph.setBackbufferSource("kt::debug");

    if (backbufferSource.empty()) {
      KT_ERROR("Debug pass added to render graph, but no backbuffer source is set. This will error during graph bake.");
    }
  }
} // namespace kt