#include "renderer.hpp"

#include <expected>
#include <string_view>

namespace kt::rdr {
  std::expected<gltf::Scene, std::string> Renderer::loadMesh(std::string_view path) { return {}; }
} // namespace kt::rdr