#pragma once

#include <cstdint>

enum class MemoryUsage : uint8_t {
  Auto = 0,
  PreferDevice = 1,
  PreferHost = 2,
};

enum class MappingMode : uint8_t {
  None = 0,
  SeqWrite = 1,
  RandomWrite = 2,
  Read = 3,
};