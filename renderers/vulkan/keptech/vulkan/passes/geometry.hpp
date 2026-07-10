#pragma once

#include "helpers/owned.hpp"
#include "keptech/core/macros.hpp"
#include "keptech/rendering/mesh.hpp"
#include "wrappers/fwd.hpp"

namespace kt::vkh::passes::geometry {

  struct Target {
    using T = Owned<Image>;
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
  };
  // NOLINTEND
  CLANG_IGNORE_WARNING_POP

  void draw(VkCommandBuffer cmdBuf, const Target& target, const Payload& payload);
} // namespace kt::vkh::passes::geometry