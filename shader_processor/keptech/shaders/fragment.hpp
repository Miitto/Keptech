#pragma once

#include <cstdint>

namespace kt::shaders {
  enum class BlendFactor : uint8_t {
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    DstColor,
    OneMinusDstColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstAlpha,
    OneMinusDstAlpha,
  };

  struct Fragment {
    bool enableBlending = false;
    BlendFactor srcColorBlendFactor = BlendFactor::One;
    BlendFactor dstColorBlendFactor = BlendFactor::Zero;
    bool depthWrite = true;
  };
} // namespace kt::shaders