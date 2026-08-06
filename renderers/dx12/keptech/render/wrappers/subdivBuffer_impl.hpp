#include "keptech/render/wrappers/subdivBuffer.hpp"

namespace kt::rdr {
  template <typename T> SubdivBuffer<T>::SubdivBuffer(Buffer&& buffer) : buffer(std::move(buffer)) {}
  template <typename T> size_t SubdivBuffer<T>::count() const { return _count; }
  template <typename T> size_t SubdivBuffer<T>::capacity() const { return buffer.size() / sizeof(T); }
  template <typename T> size_t SubdivBuffer<T>::occupied() const { return _count * sizeof(T); }
  template <typename T> Buffer& SubdivBuffer<T>::getBuffer() { return buffer; }
  template <typename T> Buffer& SubdivBuffer<T>::operator*() { return buffer; }
  template <typename T> void SubdivBuffer<T>::registerWrite(size_t count) {
    DX_ASSERT(_count + count <= capacity(), "SubdivBuffer overflow: cannot register more elements than capacity");
    _count += count;
  }

  template <typename T> void SubdivBuffer<T>::write(const T& value) {
    DX_ASSERT(_count < capacity(), "SubdivBuffer overflow: cannot write more elements than capacity");

    auto* ptr = static_cast<T*>(buffer.mapping());
    DX_ASSERT(ptr != nullptr, "SubdivBuffer mapping is null");
    ptr[_count++] = value;
  }

  template <typename T> void SubdivBuffer<T>::write(std::span<const T> values) {
    DX_ASSERT(_count + values.size() <= capacity(), "SubdivBuffer overflow: cannot write more elements than capacity");

    auto* ptr = static_cast<T*>(buffer.mapping());
    DX_ASSERT(ptr != nullptr, "SubdivBuffer mapping is null");
    memcpy(ptr + _count, values.data(), values.size() * sizeof(T));
    _count += values.size();
  }
} // namespace kt::rdr