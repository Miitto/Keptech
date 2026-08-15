#pragma once

#include <d3d12.h>

namespace kt::rhi {
  struct DrawCommand : public D3D12_DRAW_ARGUMENTS {
    DrawCommand() = default;
    DrawCommand(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertexLocation, uint32_t startInstanceLocation)
        : D3D12_DRAW_ARGUMENTS{.VertexCountPerInstance = vertexCount,
                               .InstanceCount = instanceCount,
                               .StartVertexLocation = startVertexLocation,
                               .StartInstanceLocation = startInstanceLocation} {}
  };

  struct DrawIndexedCommand : public D3D12_DRAW_INDEXED_ARGUMENTS {
    DrawIndexedCommand() = default;
    DrawIndexedCommand(uint32_t indexCount, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation,
                       uint32_t startInstanceLocation)
        : D3D12_DRAW_INDEXED_ARGUMENTS{.IndexCountPerInstance = indexCount,
                                       .InstanceCount = instanceCount,
                                       .StartIndexLocation = startIndexLocation,
                                       .BaseVertexLocation = baseVertexLocation,
                                       .StartInstanceLocation = startInstanceLocation} {}
  };
} // namespace kt::rhi