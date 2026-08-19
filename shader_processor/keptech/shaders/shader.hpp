#pragma once

#include "keptech/shaders/info.hpp"
#include "keptech/shaders/stages.hpp"
#include <spdlog/fmt/bundled/format.h>
#include <vector>

namespace kt::shaders {

  struct Shader {
    const char* file = nullptr;
#ifdef KT_VULKAN
    std::vector<uint8_t> code;
#elif defined(KT_DX12)
    std::vector<std::vector<uint8_t>> code;
#endif
    std::vector<ShaderStage> stages;
    ShaderInfo info;
  };
} // namespace kt::shaders
