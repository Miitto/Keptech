#include "shadows.hpp"

#include "glm/ext/matrix_transform.hpp"
#include <array>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace kt::rdr::passes::shadows {
  namespace {
    constexpr std::array<glm::mat4, 6> getCubemapViews(glm::vec3 center);
  } // namespace

  namespace {
    constexpr std::array<glm::mat4, 6> getCubemapViews(glm::vec3 center) {
      constexpr std::array<glm::vec3, 6> directions{
          glm::vec3{-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1},
      };

      constexpr std::array<glm::vec3, 6> upDirections{
          glm::vec3{0, 1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}, {0, 1, 0},
      };

      return std::array<glm::mat4, 6>{
          glm::lookAtLH(center, center + directions[0], upDirections[0]), glm::lookAtLH(center, center + directions[1], upDirections[1]),
          glm::lookAtLH(center, center + directions[2], upDirections[2]), glm::lookAtLH(center, center + directions[3], upDirections[3]),
          glm::lookAtLH(center, center + directions[4], upDirections[4]), glm::lookAtLH(center, center + directions[5], upDirections[5]),
      };
    }
  } // namespace
} // namespace kt::rdr::passes::shadows