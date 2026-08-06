#pragma once

#include "keptech/render/wrappers/buffer.hpp"
#include <span>

namespace kt::rdr {
  template <typename T> class SubdivBuffer {
  public:
    SubdivBuffer(Buffer&& buffer);

    /// Get the number of elements in the buffer.
    size_t count() const;
    /// Get the occupied size of the buffer in bytes.
    size_t occupied() const;
    /// Get the capacity of the buffer in number of elements.
    size_t capacity() const;

    void write(const T& value);
    void write(std::span<const T> values);

    /// Register that a certain number of elements have been written to the buffer without using in built-in methods.
    void registerWrite(size_t count);

    SubdivBuffer() = default;
    SubdivBuffer(const SubdivBuffer&) = delete;
    SubdivBuffer& operator=(const SubdivBuffer&) = delete;
    SubdivBuffer(SubdivBuffer&&) = default;
    SubdivBuffer& operator=(SubdivBuffer&&) = default;
    ~SubdivBuffer() = default;

    Buffer& getBuffer();
    Buffer& operator*();

  private:
    Buffer buffer;
    size_t _count = 0;
  };

} // namespace kt::rdr