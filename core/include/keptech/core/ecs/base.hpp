#pragma once

#include <bitset>

namespace keptech::ecs {
  using EntityHandle = uint16_t;
  constexpr EntityHandle MAX_ENTITIES = 5000;
  constexpr EntityHandle INVALID_ENTITY_HANDLE = UINT16_MAX;

  using ComponentType = uint8_t;
  constexpr ComponentType MAX_COMPONENTS = 64;

  using Signature = std::bitset<MAX_COMPONENTS>;
} // namespace keptech::ecs
