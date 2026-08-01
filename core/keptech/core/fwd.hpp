#pragma once

#include <type_traits>

namespace kt {
  template <typename T, typename E, const E E_OK>
    requires std::is_enum_v<E> && std::is_same_v<decltype(E_OK), E>
  class Result;
}