#pragma once

#include "keptech/core/result.hpp"
#include "keptech/rhi/bufferTypes.hpp"
#include "keptech/rhi/bufferUsage.hpp"
#include "keptech/rhi/result.hpp"

namespace kt::rhi {
  class BufferRef;
  class BufferCreateInfo;
  class Buffer;

  class BufferBase {
  public:
    friend class Buffer;

    BufferBase() = default;
    BufferBase(const BufferBase&) = delete;
    BufferBase& operator=(const BufferBase&) = delete;
    BufferBase(BufferBase&&) = default;
    BufferBase& operator=(BufferBase&&) = default;
    ~BufferBase() = default;

    static kt::Result<Buffer, RawRhiResult, RawRhiResultOk> create(const BufferCreateInfo& info);

    const std::string& getName() const;
    BufferType getType() const;
    size_t size() const;
    Bitflag<BufferUsage> getUsage() const;

    bool isValid(this const Buffer& self);

    void setName(this Buffer& self, const std::string& newName);

    bool isMapped(this const Buffer& self);
    void* mapping(this const Buffer& self, size_t offset = 0);
    template <typename T, typename Self> T* mapping(this const Self& self, size_t offset = 0) {
      return static_cast<T*>(self.mapping(offset));
    }

    void destroy(this Buffer& self);

    /// Creates a new buffer with the specified size.
    /// @param newSize The new size of the buffer in bytes.
    /// @returns A Result containing the new buffer, or an error code if the reallocation failed.
    /// @note Data is not copied from the old buffer to the new buffer. The caller is responsible for copying any necessary data after the
    /// reallocation.
    /// @note The old buffer may need to be submitted to the RHI for destruction next frame, as it may still be in use by the GPU.
    kt::Result<Buffer, RawRhiResult, RawRhiResultOk> reallocate(this const Buffer& self, size_t newSize);

    operator BufferRef(this const Buffer& self);

  protected:
    BufferBase(std::string name, size_t size, Bitflag<BufferUsage> usage, BufferType type)
        : name(std::move(name)), _size(size), _type(type), _usage(usage) {}

  private:
    std::string name{};
    size_t _size = 0;
    BufferType _type = BufferType::Default;
    Bitflag<BufferUsage> _usage = BufferUsage::None;
  };
} // namespace kt::rhi