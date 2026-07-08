#pragma once

#include <Volk/volk.h>
#include <keptech/shaders/shader.h>

namespace kt::vkh {
  VkShaderStageFlagBits from(shaders::ShaderStages stage);

  VkFormat from(shaders::DataType type);

  uint32_t getSize(shaders::DataType type);
} // namespace kt::vkh
