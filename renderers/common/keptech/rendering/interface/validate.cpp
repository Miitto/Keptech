#include "keptech/rendering/interface.hpp"
#include "keptech/rendering/interface/image.hpp"
#include <glm/vec3.hpp>

namespace kt::rendering {
  static_assert(interface::Image<Image>, "Image must satisfy the Image concept");
}