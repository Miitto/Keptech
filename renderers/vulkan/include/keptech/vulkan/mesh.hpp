#pragma once

#include "structs.hpp"
#include <expected>
#include <glm/glm.hpp>
#include <keptech/core/moveGuard.hpp>
#include <keptech/core/rendering/mesh.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace keptech::vkh {
  struct Mesh : public core::rendering::Mesh {
    Mesh(AddressedAllocatedBuffer vBuffer,
         std::optional<AllocatedBuffer> iBuffer,
         std::vector<core::rendering::Mesh::Submesh> submeshes,
         vma::Allocator& allocator)
        : core::rendering::Mesh{std::move(submeshes)}, vertexBuffer(vBuffer),
          indexBuffer(iBuffer), allocator(&allocator) {}

    Mesh() = delete;
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& o) noexcept
        : core::rendering::Mesh(std::move(o)), vertexBuffer(o.vertexBuffer),
          indexBuffer(o.indexBuffer), allocator(o.allocator) {
      o.allocator = nullptr;
    }
    Mesh& operator=(Mesh&& o) noexcept {
      if (this != &o) {
        destroy();

        core::rendering::Mesh::operator=(std::move(o));
        vertexBuffer = o.vertexBuffer;
        indexBuffer = o.indexBuffer;
        allocator = o.allocator;
        o.allocator = nullptr;
      }
      return *this;
    }

    AddressedAllocatedBuffer vertexBuffer;
    std::optional<AllocatedBuffer> indexBuffer;

    vma::Allocator* allocator;

    static std::expected<std::pair<Mesh, OnGoingCmdTransfer>, std::string>
    fromData(const vk::raii::Device& device, vma::Allocator& allocator,
             const CommandPool& transferPool, std::span<const Vertex> vertices,
             std::span<const uint32_t> indices,
             std::vector<Mesh::Submesh> submeshes = {});

    void destroy() {
      if (!allocator)
        return;
      vertexBuffer.destroy(*allocator);
      if (indexBuffer.has_value()) {
        indexBuffer->destroy(*allocator);
      }

      allocator = nullptr;
    }

    ~Mesh() { destroy(); }
  };
} // namespace keptech::vkh
