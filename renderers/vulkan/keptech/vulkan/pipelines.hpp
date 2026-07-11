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

    void destroy(const VkDevice& device);
  };
} // namespace kt::vkh