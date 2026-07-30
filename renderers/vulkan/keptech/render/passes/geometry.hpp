#pragma once

#include "keptech/render/renderGraph/passInterface.hpp"
#include "keptech/render/wrappers/pipeline.hpp"

namespace kt::rdr {
  struct Buffers;
  struct Members;
} // namespace kt::rdr

namespace kt::rdr {

  /// @brief A render pass that renders the scene geometry into G-buffers.
  /// @details This pass renders into the following G-buffers:
  /// - `kt::albedo`: Albedo (RGB) + Alpha (A)
  /// - `kt::normal`: Normal (RGB)
  /// - `kt::material`: Material (Metallic (R) + Roughness (G))
  /// - `kt::emissive`: Emissive (RGB)
  /// - `kt::depth`: Depth (D)
  /// @note Only the depth buffer is cleared at the start of the pass by default, and the color buffers are loaded as `DontCare`. If you
  /// want to clear the color buffers, you can call `setClearColorBuffers(true)` before the render graph is baked.
  class GeometryPass : public RenderPassInterface {
  public:
    void setupDependencies(RenderPassBuilder& self, RenderGraphBuilder& graph, const Renderer& renderer) override;

    /// Called once after the render graph has been built.
    void setup(Renderer& renderer, VkDescriptorSetLayout descriptorSetLayout) override;

    [[nodiscard]] bool getClearColor(size_t, VkClearColorValue* value) const override;
    [[nodiscard]] bool getClearDepthStencil(VkClearDepthStencilValue* value) const override;

    void execute(const CommandBuffer& cmd, VkDescriptorSet descriptorSet, glm::uvec2 framebufferSize = {}) override;

    void setClearColorBuffers(bool clear);
    void setDepthClearValue(float value);

  private:
    Pipeline pipeline;
    bool clearColorBuffers = false;
    float depthClearValue = 1.0f;
  };
} // namespace kt::rdr