#pragma once

#include "keptech/render/mesh.hpp"
#include "keptech/render/wrappers/subdivBuffer.hpp"
#include <glm/fwd.hpp>

namespace kt::rdr {
  struct Buffers {
    SubdivBuffer<glm::vec3> positions;
    SubdivBuffer<VertexAttribs> vertexAttribs;
    SubdivBuffer<uint32_t> indices;
  };
} // namespace kt::rdr