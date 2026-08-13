#include "types.hpp"

namespace kt {
  bool BufferInfo::isHostAccessible() const { return mappingMode != MappingMode::None; }
} // namespace kt