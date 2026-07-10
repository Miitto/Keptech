#pragma once

#include "keptech/vulkan/wrappers/pipeline.hpp"

namespace kt::vkh {

  struct Layouts {
    VkPipelineLayout empty;
    VkPipelineLayout onlyGlobals;
    VkPipelineLayout meshShaderLayout;
    VkPipelineLayout pointLightShadowsLayout;
    VkPipelineLayout pointLightLayout;
    VkPipelineLayout ssaoBlurLayout;
    VkPipelineLayout bloomDownsampleLayout;
    VkPipelineLayout bloomUpsampleLayout;

    void destroy(const VkDevice& device);
  };

  struct Pipelines {
    Pipeline basic;
    Pipeline mesh_shader;
    Pipeline pointLightShadows;
    Pipeline deferredPointLight;
    Pipeline ssao;
    Pipeline ssaoBlur;
    Pipeline deferredCombine;
    Pipeline bloomDownsample;
    Pipeline bloomUpsample;
    Pipeline bloomCombine;

    void destroy(const VkDevice& device);
  };
} // namespace kt::vkh