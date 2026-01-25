#pragma once

#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"
#include <cstdint>
#include <expected>
#include <memory>
#include <string>

namespace keptech {
  enum class BufferUsage : uint8_t {
    None = 0,
    VertexBuffer = BIT(0),
    IndexBuffer = BIT(1),
    UniformBuffer = BIT(2),
    StorageBuffer = BIT(3),
    TransferSrc = BIT(4),
    TransferDst = BIT(5),
  };

  enum class BufferMemoryType : uint8_t {
    GPUOnly = 0,
    CPUToGPU,
    GPUToCPU,
  };

  class IBuffer {
  public:
    [[nodiscard]] virtual size_t getSize() const = 0;
    [[nodiscard]] virtual void* getMapping() const;
    [[nodiscard]] virtual uint64_t getDeviceAddress() const;

    IBuffer(const IBuffer&) = default;
    IBuffer(IBuffer&&) = default;
    IBuffer& operator=(const IBuffer&) = default;
    IBuffer& operator=(IBuffer&&) = default;
    virtual ~IBuffer() = default;

#ifdef KT_ADD_RESOURCE_INFO
    void setDebugName(const std::string& name) { debugName = name; }
    const std::string& getDebugName() const { return debugName; }

    Bitflag<BufferUsage> getUsageFlags() const { return usageFlags; }
    BufferMemoryType getMemoryType() const { return memoryType; }
#endif

  private:
#ifdef KT_ADD_RESOURCE_INFO
    std::string debugName = "";
    Bitflag<BufferUsage> usageFlags = BufferUsage::None;
    BufferMemoryType memoryType = BufferMemoryType::GPUOnly;
#endif
    size_t size = 0;
  };

  using UBufPtr = std::unique_ptr<IBuffer>;
  using SBufPtr = std::shared_ptr<IBuffer>;

  template <typename T>
  concept IsBuffer = requires(T t, std::size_t size, BufferUsage usage,
                              BufferMemoryType memType) {
    {
      T::create(size, usage, memType, true)
    } -> std::same_as<std::expected<T, std::string>>;
    { t.destroy() } -> std::same_as<void>;
  };
} // namespace keptech
