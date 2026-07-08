#pragma once

#include "keptech/vulkan/vk-logger.hpp"
#include <concepts>

using VmaAllocator = struct VmaAllocator_T*;
using VkDevice = struct VkDevice_T*;

namespace kt::vkh {
  template <typename T>
  concept Destroyable = requires(T t, const VmaAllocator& allocator) {
    { t.destroy(allocator) } -> std::same_as<void>;
  };

  template <typename T>
  concept DerefDestroyable = requires(T t, const VmaAllocator& allocator) { t->destroy(allocator); };

  template <typename T>
  concept DestroyableDevice = requires(T t, const VmaAllocator& allocator, const VkDevice& device) {
    { t.destroy(allocator, device) } -> std::same_as<void>;
  };

  template <typename T>
  concept DerefDestroyableDevice = requires(T t, const VmaAllocator& allocator, const VkDevice& device) { t->destroy(allocator, device); };

  template <typename T>
    requires Destroyable<T> || DerefDestroyable<T> || DestroyableDevice<T> || DerefDestroyableDevice<T>
  class Owned {
    struct Members {
      T object{};
      VmaAllocator allocator = nullptr;
      VkDevice device = nullptr;
    };

  public:
    Owned() = default;
    Owned(const Owned&) = delete;
    Owned(Owned&& o) noexcept : m(std::move(o.m)) {
      o.m.allocator = nullptr;
      o.m.device = nullptr;
    }
    Owned& operator=(const Owned&) = delete;
    Owned& operator=(Owned&& o) noexcept {
      if (this == &o)
        return *this;

      m = std::move(o.m);

      o.m.allocator = nullptr;
      o.m.device = nullptr;

      return *this;
    }
    Owned& operator=(T&& o) {
      VK_ASSERT(m.allocator != nullptr && m.device != nullptr, "Cannot assign to Owned object without allocator and device");
      m.object = std::move(o);
      return *this;
    }

    template <typename U = T>
      requires std::constructible_from<T, U&&>
    Owned(U object, const VmaAllocator& allocator, const VkDevice& device) : m(Members{std::move(object), allocator, device}) {}
    ~Owned() {
      if (m.device != nullptr) {
        if constexpr (Destroyable<T>)
          m.object.destroy(m.allocator);
        else if constexpr (DerefDestroyable<T>)
          m.object->destroy(m.allocator);
        else if constexpr (DestroyableDevice<T>)
          m.object.destroy(m.allocator, m.device);
        else if constexpr (DerefDestroyableDevice<T>)
          m.object->destroy(m.allocator, m.device);
      }
    }

    const T& get() const { return m.object; }
    T& get() { return m.object; }
    const T& operator*() const { return m.object; }
    T& operator*() { return m.object; }
    const T* operator->() const { return &m.object; }
    T* operator->() { return &m.object; }
    operator const T&() const { return m.object; }
    operator T&() { return m.object; }

  private:
    Members m;
  };
} // namespace kt::vkh