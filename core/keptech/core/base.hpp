#pragma once

#include <glm/ext/vector_float3.hpp>
namespace kt {
  using u8 = uint8_t;
  using u16 = uint16_t;
  using u32 = uint32_t;
  using u64 = uint64_t;
  using i8 = int8_t;
  using i16 = int16_t;
  using i32 = int32_t;
  using i64 = int64_t;
  using f32 = float;
  using f64 = double;
  using usize = size_t;
  using isize = std::make_signed_t<size_t>;

  /// Timestep in milliseconds
  using Timestep = float;

  constexpr glm::vec3 FORWARD{0, 0, 1};
  constexpr glm::vec3 RIGHT{1, 0, 0};
  constexpr glm::vec3 UP{0, 1, 0};
  constexpr glm::vec3 ORIGIN{0, 0, 0};

  template <typename... T> struct overloaded : T... {
    using T::operator()...;
  };
} // namespace kt
