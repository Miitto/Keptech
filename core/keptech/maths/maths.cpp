#include "maths.hpp"

namespace kt {
  uint32_t calcMipLevels(glm::uvec2 size) { return static_cast<uint32_t>(std::floor(std::log2(max(size.x, size.y)))) + 1; }
  uint32_t calcMipLevels(glm::uvec3 size) { return static_cast<uint32_t>(std::floor(std::log2(max(max(size.x, size.y), size.z)))) + 1; }
} // namespace kt