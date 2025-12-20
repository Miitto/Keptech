#pragma once

#include "structs.hpp"
#include <expected>
#include <glm/glm.hpp>
#include <keptech/core/moveGuard.hpp>
#include <keptech/core/vertex.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {
  using Vertex = keptech::core::Vertex;

  struct Mesh {
    struct Submesh {
      uint32_t indexCount;
      uint32_t indexOffset;
    };

    AddressedAllocatedBuffer vertexBuffer;
    std::optional<AllocatedBuffer> indexBuffer;
    std::vector<Submesh> submeshes;

    static std::expected<std::pair<Mesh, OnGoingCmdTransfer>, std::string>
    fromData(const vk::raii::Device& device, vma::Allocator& allocator,
             const CommandPool& transferPool, std::span<const Vertex> vertices,
             std::span<const uint32_t> indices,
             std::vector<Mesh::Submesh> submeshes = {});

    void destroy(vma::Allocator& allocator) {
      vertexBuffer.destroy(allocator);
      if (indexBuffer.has_value()) {
        indexBuffer->destroy(allocator);
      }
    }
  };
} // namespace keptech::vkh
