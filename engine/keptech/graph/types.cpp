#include "types.hpp"

namespace kt {
  bool BufferInfo::isHostAccessible() const { return type != rhi::BufferType::Default; }
} // namespace kt