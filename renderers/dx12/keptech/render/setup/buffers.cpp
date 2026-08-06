#include "macros.hpp"
#include "renderer.hpp"
#include "wrappers/bufferCreateInfo.hpp"

#define MAKE(expr)                                                                                                                         \
  {                                                                                                                                        \
    auto res = (expr);                                                                                                                     \
    if (!res) {                                                                                                                            \
      return std::unexpected(res.error());                                                                                                 \
    }                                                                                                                                      \
  }

namespace kt::rdr {
  namespace {
    constexpr size_t INITIAL_VERTICES = 1000;
    constexpr size_t INITIAL_INDICES = 3000;

    template <typename T> std::expected<void, std::string> createBuffer(size_t numElements, const char* name, Buffer& buffer) {
      auto r = Buffer::create(BufferCreateInfo(numElements * sizeof(T), MappingMode::None, MemoryUsage::PreferDevice, name));
      if (!r) {
        return std::unexpected(std::string("Failed to create buffer: ") + name);
      }
      buffer = std::move(r.value());
      return {};
    }
  } // namespace

  std::expected<void, std::string> Renderer::initBuffers() {
    MAKE(createBuffer<glm::vec3>(INITIAL_VERTICES, "Positions", *m.buffers.positions));
    MAKE(createBuffer<VertexAttribs>(INITIAL_VERTICES, "VertexAttribs", *m.buffers.vertexAttribs));
    MAKE(createBuffer<uint32_t>(INITIAL_INDICES, "Indices", *m.buffers.indices));

    return {};
  }
} // namespace kt::rdr