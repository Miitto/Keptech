#include "types.hpp"

namespace kt::rdr {
  bool BufferInfo::isHostAccessible() const { return mappingMode != MappingMode::None; }
} // namespace kt::rdr