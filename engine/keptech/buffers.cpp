#include "buffers.hpp"
#include "keptech/rhi/descriptorInfo.hpp"
#include "keptech/rhi/rhi.hpp"

namespace kt {
  Buffers Buffers::instance;
  rhi::DescriptorLayout Buffers::layout;

  template <typename T> using R = ::kt::Result<Buffers::SB<T>, rhi::RawRhiResult, rhi::RawRhiResultOk>;

  namespace {
    template <typename T> R<T> reallocBuffer(SubdivBuffer<T>& buffer, size_t newSize) {
      auto result = buffer->reallocate(newSize * sizeof(T));
      if (!result) {
        return result.error();
      }
      R<T> oldBuffer = std::move(buffer);
      buffer = SubdivBuffer<T>(std::move(result.value()), 0);
      return oldBuffer;
    }

    void ensureDescriptor(rhi::DescriptorSet& set) {
      Buffers::ensureDescriptorLayout();
      if (!set) {
        auto& rhi = rhi::RHI::get();
        set = rhi.allocateDescriptorSet(Buffers::getLayout());
      }
    }

    template <typename T>
    R<T> reallocAndWriteDescriptor(SubdivBuffer<T>& buffer, size_t newSize, rhi::DescriptorSet& set, uint32_t binding, const char* name) {
      if (buffer->getName().empty()) {
        buffer->setName(name);
      }
      auto result = reallocBuffer(buffer, newSize);
      if (!result) {
        return result.error();
      }
      ensureDescriptor(set);
      set.write(Buffers::getLayout(), binding, 0, rhi::DescriptorWriteBufferType::Storage, buffer, 0, newSize * sizeof(T), sizeof(T));
      return result;
    }
  } // namespace

  void Buffers::ensureDescriptorLayout() {
    if (!layout) {
      auto& rhi = rhi::RHI::get();
      std::array<kt::rhi::DescriptorInfo, 8> descriptorInfos = {{
          {rhi::DescriptorType::StorageBuffer, 0, 1},
          {rhi::DescriptorType::StorageBuffer, 1, 1},
          {rhi::DescriptorType::StorageBuffer, 2, 1},
          {rhi::DescriptorType::StorageBuffer, 3, 1},
          {rhi::DescriptorType::StorageBuffer, 4, 1},
          {rhi::DescriptorType::StorageBuffer, 5, 1},
          {rhi::DescriptorType::StorageBuffer, 6, 1},
          {rhi::DescriptorType::StorageBuffer, 7, 1},
      }};
      layout = rhi.createDescriptorLayout(descriptorInfos);
    }
  }

  R<glm::vec3> Buffers::reallocatePositions(size_t newSize) {
    return reallocAndWriteDescriptor(positions, newSize, descriptor, 0, "KT Vertex Positions");
  }

  R<VertexAttribs> Buffers::reallocateVertexAttribs(size_t newSize) {
    return reallocAndWriteDescriptor(vertexAttribs, newSize, descriptor, 1, "KT Vertex Attributes");
  }

  R<uint32_t> Buffers::reallocateIndices(size_t newSize) {
    return reallocAndWriteDescriptor(indices, newSize, descriptor, 2, "KT Indices");
  }

  R<Meshlet> Buffers::reallocateMeshlets(size_t newSize) {
    return reallocAndWriteDescriptor(meshlets, newSize, descriptor, 3, "KT Meshlets");
  }

  R<uint32_t> Buffers::reallocateMeshletVertices(size_t newSize) {
    return reallocAndWriteDescriptor(meshletVertices, newSize, descriptor, 4, "KT Meshlet Vertices");
  }

  R<uint32_t> Buffers::reallocateMeshletTriangles(size_t newSize) {
    return reallocAndWriteDescriptor(meshletTriangles, newSize, descriptor, 5, "KT Meshlet Triangles");
  }

  R<GpuSubmesh> Buffers::reallocateSubmeshes(size_t newSize) {
    return reallocAndWriteDescriptor(submeshes, newSize, descriptor, 6, "KT Submeshes");
  }

  R<GpuMaterial> Buffers::reallocateMaterials(size_t newSize) {
    return reallocAndWriteDescriptor(materials, newSize, descriptor, 7, "KT Materials");
  }

} // namespace kt