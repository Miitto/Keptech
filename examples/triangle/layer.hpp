#pragma once

#include "keptech/components/transform.hpp"
#include "keptech/core/kt-logger.hpp"
#include "keptech/core/layers/layer.hpp"
#include "keptech/core/scene.hpp"
#include "keptech/core/window.hpp"
#include "keptech/graph/builder.hpp"
#include "keptech/rhi/rhi.hpp"
#include "pass.hpp"

class ExampleLayer : public kt::Layer {
public:
  ExampleLayer(kt::Window& window, kt::rhi::RenderGraphBuilder& builder, kt::rhi::Renderer& renderer) : kt::Layer("Example Mesh Shader") {
    auto& scene = kt::Scene::active();

    auto& pass = builder.addPass("Triangle");
    pass.setInterface(&trianglePass);

    builder.setBackbufferSource("color");
  }

  void onUpdate(kt::Timestep ts) final {}

  void onEvent(kt::Event& event, kt::Timestep ts) final {}

private:
  TrianglePass trianglePass{};
};