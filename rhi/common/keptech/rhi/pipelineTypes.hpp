#pragma once

namespace kt::rhi {
  enum class DepthCompareOp : uint8_t { Never, Less, LessOrEqual, Equal, GreaterOrEqual, Greater, NotEqual, Always };

  enum class FrontFace : uint8_t { Clockwise, CounterClockwise };

  enum class PolygonMode : uint8_t { Fill, Line, Point };
} // namespace kt::rhi