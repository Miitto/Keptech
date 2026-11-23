#include "buffer_base.hpp"

namespace kt::rhi {
  const std::string& BufferBase::getName() const { return name; }
  BufferType BufferBase::getType() const { return _type; }
  size_t BufferBase::size() const { return _size; }
  Bitflag<BufferUsage> BufferBase::getUsage() const { return _usage; }
} // namespace kt::rhi