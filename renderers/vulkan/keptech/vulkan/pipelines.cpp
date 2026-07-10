#include "pipelines.hpp"

namespace kt::vkh {
  void Layouts::destroy(const VkDevice& device) {
    auto d = [&](VkPipelineLayout layout) { vkDestroyPipelineLayout(device, layout, nullptr); };
    d(empty);
    d(onlyGlobals);
    d(meshShaderLayout);
    d(pointLightShadowsLayout);
    d(pointLightLayout);
    d(ssaoBlurLayout);
    d(bloomDownsampleLayout);
    d(bloomUpsampleLayout);
  }

  void Pipelines::destroy(const VkDevice& device) {
    auto d = [&](Pipeline& pipeline) { vkDestroyPipeline(device, pipeline.pipeline, nullptr); };
    d(basic);
    d(mesh_shader);
    d(pointLightShadows);
    d(deferredPointLight);
    d(ssao);
    d(ssaoBlur);
    d(deferredCombine);
    d(bloomDownsample);
    d(bloomUpsample);
    d(bloomCombine);
  }
} // namespace kt::vkh