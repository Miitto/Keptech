#pragma once

#include <concepts>
#include <type_traits>

namespace keptech::core {
  template <typename T>
    requires std::is_enum_v<T>
  struct Bitflag {
    using Underlying = std::underlying_type_t<T>;
    constexpr inline Bitflag() : flags(0) {}
    constexpr inline Bitflag(T flag) : flags(static_cast<Underlying>(flag)) {}
    constexpr explicit inline Bitflag(Underlying flag) : flags(flag) {}

    constexpr static inline Bitflag<T> none() { return Bitflag<T>(0); }

    constexpr static inline Bitflag<T> all()
      requires requires() { T::MAX_VALUE; }
    {
      return Bitflag<T>(T::MAX_VALUE - 1);
    }

    constexpr static inline Bitflag<T> all()
      requires(!requires() { T::MAX_VALUE; })
    {
      return Bitflag<T>(~Underlying(0));
    }

    constexpr inline Bitflag<T>& operator|=(T flag) {
      flags |= static_cast<Underlying>(flag);
      return *this;
    }

    constexpr inline Bitflag<T>& operator&=(T flag) {
      flags &= static_cast<Underlying>(flag);
      return *this;
    }

    constexpr inline Bitflag<T>& operator^=(T flag) {
      flags ^= static_cast<Underlying>(flag);
      return *this;
    }

    constexpr inline Bitflag<T>& operator|=(Bitflag<T> other) {
      flags |= other.flags;
      return *this;
    }

    constexpr inline Bitflag<T>& operator&=(Bitflag<T> other) {
      flags &= other.flags;
      return *this;
    }

    constexpr inline Bitflag<T>& operator^=(Bitflag<T> other) {
      flags ^= other.flags;
      return *this;
    }

    constexpr inline operator bool() const { return flags != 0; }
    constexpr inline bool operator!() const { return flags == 0; }

    constexpr explicit inline operator Underlying() const { return flags; }
    constexpr explicit inline operator T() const {
      return static_cast<T>(flags);
    }

    constexpr inline T as_enum() const { return static_cast<T>(flags); }

    constexpr inline Underlying as_underlying() const { return flags; }

    constexpr bool inline operator==(Bitflag<T> other) const {
      return flags == other.flags;
    }
    constexpr bool inline operator!=(Bitflag<T> other) const {
      return flags != other.flags;
    }

    constexpr inline Bitflag<T> operator~() const { return Bitflag<T>(~flags); }

    constexpr inline Bitflag& set(T flag) {
      flags |= static_cast<Underlying>(flag);
      return *this;
    }

    constexpr inline bool has(T flag) const {
      return (flags & static_cast<Underlying>(flag)) != 0;
    }

    constexpr inline Bitflag& clear(T flag) {
      flags &= ~static_cast<Underlying>(flag);
      return *this;
    }

    Underlying flags;
  };
} // namespace keptech::core

template <typename T>
  requires std::is_enum_v<T>
constexpr inline keptech::core::Bitflag<T>
operator|(keptech::core::Bitflag<T> lhs, T rhs) {
  lhs |= rhs;
  return lhs;
}
template <typename T>
  requires std::is_enum_v<T>
constexpr inline keptech::core::Bitflag<T>
operator&(keptech::core::Bitflag<T> lhs, T rhs) {
  lhs &= rhs;
  return lhs;
}

template <typename T>
  requires std::is_enum_v<T>
constexpr inline keptech::core::Bitflag<T>
operator^(keptech::core::Bitflag<T> lhs, T rhs) {
  lhs ^= rhs;
  return lhs;
}

template <typename T>
  requires std::is_enum_v<T> &&
           std::constructible_from<keptech::core::Bitflag<T>, T>
constexpr inline keptech::core::Bitflag<T> operator|(T lhs, T rhs) {
  return Bitflag<T>(lhs) | rhs;
}

template <typename T>
  requires std::is_enum_v<T> &&
           std::constructible_from<keptech::core::Bitflag<T>, T>
constexpr inline keptech::core::Bitflag<T> operator&(T lhs, T rhs) {
  return Bitflag<T>(lhs) & rhs;
}

template <typename T>
  requires std::is_enum_v<T> &&
           std::constructible_from<keptech::core::Bitflag<T>, T>
constexpr inline keptech::core::Bitflag<T> operator^(T lhs, T rhs) {
  return Bitflag<T>(lhs) ^ rhs;
}
