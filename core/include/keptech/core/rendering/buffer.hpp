#pragma once

#include "keptech/core/bitflag.hpp"
#include "keptech/core/macros.hpp"
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace keptech {
  enum class BufferUsage : uint8_t {
    None = 0,
    Vertex = BIT(0),
    Index = BIT(1),
    Uniform = BIT(2),
    Storage = BIT(3),
    TransferSrc = BIT(4),
    TransferDst = BIT(5),
  };

  enum class BufferMemoryType : uint8_t {
    Auto,
    PreferDevice,
    PreferHost,
  };

  enum class BufferMapType : uint8_t {
    None = 0,
    SeqWrite = BIT(0),
    Random = BIT(1),
    AllowTransferInstead = BIT(2),
  };

  struct BufferCreateInfo {
    std::optional<std::string> name = std::nullopt;
    size_t size = 0;
    Bitflag<BufferUsage> usage = BufferUsage::None;
    BufferMemoryType memoryType = BufferMemoryType::Auto;
    Bitflag<BufferMapType> map = BufferMapType::None;
  };

  class IBuffer {
  public:
    [[nodiscard]] size_t getSize() const { return size; }
    [[nodiscard]] virtual void* getMapping() const = 0;
    [[nodiscard]] virtual uint64_t getDeviceAddress() const = 0;

    IBuffer() = default;
    IBuffer(size_t size) : size(size) {}
    IBuffer(const IBuffer&) = default;
    IBuffer(IBuffer&&) = default;
    IBuffer& operator=(const IBuffer&) = default;
    IBuffer& operator=(IBuffer&&) = default;
    virtual ~IBuffer() = default;

#ifdef KT_ADD_RESOURCE_INFO
    IBuffer(size_t size, std::string name, Bitflag<BufferUsage> usage,
            BufferMemoryType memoryType)
        : debugName(std::move(name)), usageFlags(usage), memoryType(memoryType),
          size(size) {}

    void setDebugName(const std::string& name) { debugName = name; }
    [[nodiscard]] const std::string& getDebugName() const { return debugName; }

    [[nodiscard]] Bitflag<BufferUsage> getUsageFlags() const {
      return usageFlags;
    }
    [[nodiscard]] BufferMemoryType getMemoryType() const { return memoryType; }
#endif

  protected:
#ifdef KT_ADD_RESOURCE_INFO
    std::string debugName = "";
    Bitflag<BufferUsage> usageFlags = BufferUsage::None;
    BufferMemoryType memoryType = BufferMemoryType::Auto;
#endif
    size_t size = 0;
  };

  using BufPtr = std::shared_ptr<IBuffer>;

  template <typename T>
  concept IsBuffer = requires(T t, BufferCreateInfo i) {
    { T::create(i) } -> std::same_as<std::expected<T, std::string>>;
    { t.destroy() } -> std::same_as<void>;
  };
} // namespace keptech

DEFINE_BITFLAG_ENUM_OPERATORS(keptech::BufferUsage)
DEFINE_BITFLAG_ENUM_OPERATORS(keptech::BufferMapType)
