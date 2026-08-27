#pragma once

#include "keptech/core/layers/layer.hpp"
#include "keptech/core/window.hpp"
#include "keptech/graph/builder.hpp"
#include "keptech/graph/graph.hpp"
#include "keptech/rhi/rhi.hpp"

class ExampleLayer : public kt::Layer {
public:
  ExampleLayer(kt::Window& window, kt::RenderGraphBuilder& builder, kt::rhi::RHI& rhi) : kt::Layer("Example Clear Screen") {
    auto& pass = builder.addPass("clear_pass", kt::QueueType::Graphics);

    pass.addColorOutput("color", {
                                     .sizeType = kt::AttachmentSize::SwapchainRelative,
                                     .format = kt::rhi::ImageFormat::R8G8B8A8_UNORM,
                                 });

    pass.setBuildCallback(
        [](kt::RenderGraph& graph, kt::rhi::CommandBuffer& cmd, const kt::rhi::DescriptorSet&, glm::uvec2 framebufferSize) {
          auto index = graph.getImageIndex("color");
          auto& image = graph.getImage(index);

          cmd.clearColorImage(image, {0.0f, 0.2f, 0.5f, 1.0f});
        });

    builder.setBackbufferSource("color");
  }

private:
};