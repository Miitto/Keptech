#include "types.hpp"

namespace kt::rdr {
  bool BufferInfo::isHostAccessible() const {
    return (allocationFlags & (VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                               VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)) != 0;
  }
} // namespace kt::rdr