#pragma once

#include "keptech/core/macros.hpp"
#include "keptech/render/wrappers/fwd.hpp"
#include "keptech/rendering/mesh.hpp"

namespace kt::rdr {
  struct Buffers;
  struct Members;
} // namespace kt::rdr

namespace kt::rdr::passes::geometry {

  struct Target {
    using T = Image;
    T albedo;
    T normal;
    T emissive;
    T metRough;
    T depth;
  };

  CLANG_IGNORE_WARNING_PUSH
  // NOLINTBEGIN
  struct Payload {
    const std::vector<Submesh>& submeshes;
    const std::vector<glm::mat4>& modelMatrices;
  };
  // NOLINTEND
  CLANG_IGNORE_WARNING_POP

  void draw(const Members& members, VkCommandBuffer cmdBuf, const Target& target, const Payload& payload);
} // namespace kt::rdr::passes::geometry