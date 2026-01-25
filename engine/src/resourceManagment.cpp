#include "keptech/renderer.hpp"

namespace keptech {
  std::future<std::expected<MeshPtr, std::string>>
  Renderer::loadMesh(const MeshData& data, bool backgroundLoad) {
    return std::async(
        std::launch::deferred, [this]() -> std::expected<MeshPtr, std::string> {
          return std::unexpected("Mesh loading from path not implemented yet.");
        });
  }
  std::future<std::expected<std::vector<MeshPtr>, std::string>>
  Renderer::loadMesh(std::string_view path, bool backgroundLoad) {
    return std::async(
        std::launch::deferred,
        [this]() -> std::expected<std::vector<MeshPtr>, std::string> {
          return std::unexpected("Mesh loading from path not implemented yet.");
        });
  }
} // namespace keptech
