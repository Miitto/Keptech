#include "cmdBuf.hpp"
#include "bufferRef.hpp"
#include "d3dx12.h"
#include "descriptorSet.hpp"
#include "dx/dx-logger.hpp"
#include "image.hpp"
#include "imageRef.hpp"
#include "pipeline.hpp"
#include "rhi.hpp"

#ifndef KT_DISABLE_STATS
#define STATS_INC(x) ++(RHI::get().getStats().x)
#else
#define STATS_INC(x)
#endif

namespace kt::rhi {
  CommandBuffer& CommandBuffer::bindGraphicsPipeline(const Pipeline& pipeline) {
    cmdList->SetPipelineState(pipeline.pipelineState.Get());
    std::array<ID3D12DescriptorHeap*, 2> heaps = {
        rhi::RHI::get().dxGetMembers().samplerHeap.heap.Get(),
        rhi::RHI::get().dxGetMembers().cbvSrvUavHeap.heap.Get(),
    };
    cmdList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
    cmdList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
    cmdList->IASetPrimitiveTopology(pipeline.primitiveTopology);
    gPipeline = &pipeline;

    STATS_INC(pipelineSwitches);

    return *this;
  }

  CommandBuffer& CommandBuffer::bindComputePipeline(const Pipeline& pipeline) {
    cmdList->SetPipelineState(pipeline.pipelineState.Get());
    std::array<ID3D12DescriptorHeap*, 2> heaps = {rhi::RHI::get().dxGetMembers().cbvSrvUavHeap.heap.Get(),
                                                  rhi::RHI::get().dxGetMembers().samplerHeap.heap.Get()};
    cmdList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
    cmdList->SetComputeRootSignature(pipeline.rootSignature.Get());
    cPipeline = &pipeline;

    STATS_INC(pipelineSwitches);

    return *this;
  }

  CommandBuffer& CommandBuffer::clearColorImage(const rhi::ImageRef& image, const std::array<float, 4>& clearColor) {
    cmdList->ClearRenderTargetView(image.dxGetRtvDsvHandle(), clearColor.data(), 0, nullptr);

    return *this;
  }

  CommandBuffer& CommandBuffer::beginRendering(const std::span<ColorAttachmentDesc> colorAttachments,
                                               std::optional<DepthStencilAttachmentDesc> depthStencilAttachment) {
    std::vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC> renderTargetDescs(colorAttachments.size());
    for (size_t i = 0; i < colorAttachments.size(); ++i) {
      renderTargetDescs[i] = {
          .cpuDescriptor = colorAttachments[i].imageRef.dxGetRtvDsvHandle(),
          .BeginningAccess = {},
          .EndingAccess = {},
      };

      switch (colorAttachments[i].loadOp) {
      case LoadOp::Load:
        renderTargetDescs[i].BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
        break;
      case LoadOp::Clear:
        renderTargetDescs[i].BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
        renderTargetDescs[i].BeginningAccess.Clear.ClearValue.Format = raw(colorAttachments[i].imageRef.format());
        std::copy(colorAttachments[i].clearColor.begin(), colorAttachments[i].clearColor.end(),
                  renderTargetDescs[i].BeginningAccess.Clear.ClearValue.Color);
        break;
      case LoadOp::DontCare:
        renderTargetDescs[i].BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
        break;
      }

      switch (colorAttachments[i].storeOp) {
      case StoreOp::Store:
        renderTargetDescs[i].EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
        break;
      case StoreOp::DontCare:
        renderTargetDescs[i].EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
        break;
      }
    }

    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depthStencilDesc{};
    if (depthStencilAttachment.has_value()) {
      depthStencilDesc.cpuDescriptor = depthStencilAttachment->imageRef.dxGetRtvDsvHandle();

      switch (depthStencilAttachment->loadOp) {
      case LoadOp::Load:
        depthStencilDesc.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
        depthStencilDesc.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
        break;
      case LoadOp::Clear:
        depthStencilDesc.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
        depthStencilDesc.DepthBeginningAccess.Clear.ClearValue.Format = raw(depthStencilAttachment->imageRef.format());
        depthStencilDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = depthStencilAttachment->clearDepth;
        depthStencilDesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = depthStencilAttachment->clearStencil;
        depthStencilDesc.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
        depthStencilDesc.StencilBeginningAccess.Clear.ClearValue.Format = raw(depthStencilAttachment->imageRef.format());
        depthStencilDesc.StencilBeginningAccess.Clear.ClearValue.DepthStencil.Depth = depthStencilAttachment->clearDepth;
        depthStencilDesc.StencilBeginningAccess.Clear.ClearValue.DepthStencil.Stencil = depthStencilAttachment->clearStencil;
        break;
      case LoadOp::DontCare:
        depthStencilDesc.DepthBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
        depthStencilDesc.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
        break;
      }

      switch (depthStencilAttachment->storeOp) {
      case StoreOp::Store:
        depthStencilDesc.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
        depthStencilDesc.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
        break;
      case StoreOp::DontCare:
        depthStencilDesc.DepthEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
        depthStencilDesc.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
        break;
      }

      bool hasStencil = false; // TODO: Add stencil formats

      if (!hasStencil) {
        depthStencilDesc.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
        depthStencilDesc.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
      }
    }

    cmdList->BeginRenderPass(static_cast<UINT>(renderTargetDescs.size()), renderTargetDescs.data(),
                             depthStencilAttachment.has_value() ? &depthStencilDesc : nullptr, D3D12_RENDER_PASS_FLAG_NONE);

    STATS_INC(renderPasses);

    return *this;
  }

  CommandBuffer& CommandBuffer::bindVertexBuffer(size_t slot, const BufferRef& buffer, size_t stride, size_t offset) {
    D3D12_VERTEX_BUFFER_VIEW view{
        .BufferLocation = buffer.dxGetResource()->GetGPUVirtualAddress() + offset,
        .SizeInBytes = static_cast<UINT>(buffer.size() - offset),
        .StrideInBytes = static_cast<UINT>(stride),
    };
    cmdList->IASetVertexBuffers(static_cast<UINT>(slot), 1, &view);
    return *this;
  }
  CommandBuffer& CommandBuffer::bindVertexBuffers(size_t firstSlot, const std::span<const VertexBufferBinding> bindings) {
    std::vector<D3D12_VERTEX_BUFFER_VIEW> views;
    views.reserve(bindings.size());
    for (const auto& binding : bindings) {
      D3D12_VERTEX_BUFFER_VIEW view{
          .BufferLocation = binding.buffer.dxGetResource()->GetGPUVirtualAddress() + binding.offset,
          .SizeInBytes = static_cast<UINT>(binding.buffer.size() - binding.offset),
          .StrideInBytes = static_cast<UINT>(binding.stride),
      };
      views.push_back(view);
    }
    cmdList->IASetVertexBuffers(static_cast<UINT>(firstSlot), static_cast<UINT>(views.size()), views.data());
    return *this;
  }

  CommandBuffer& CommandBuffer::bindIndexBuffer(const BufferRef& buffer, IndexType indexType, size_t offset) {
    D3D12_INDEX_BUFFER_VIEW view{
        .BufferLocation = buffer.dxGetResource()->GetGPUVirtualAddress() + offset,
        .SizeInBytes = static_cast<UINT>(buffer.size() - offset),
        .Format = indexType == IndexType::UInt16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
    };
    cmdList->IASetIndexBuffer(&view);
    return *this;
  }

  CommandBuffer& CommandBuffer::writeGraphicsPushConstants(const void* data, size_t size, size_t offset) {
    DX_ASSERT(size > 0, "Size must be greater than 0");
    DX_ASSERT(offset + size <= 128, "Push constant size must be less than or equal to 128 bytes");
    DX_ASSERT(size % sizeof(uint32_t) == 0, "Push constant size must be a multiple of 4 bytes (32 bits) for DX12");
    DX_ASSERT(offset % sizeof(uint32_t) == 0, "Push constant offset must be a multiple of 4 bytes (32 bits) for DX12");
    cmdList->SetGraphicsRoot32BitConstants(gPipeline->constantSlot, static_cast<UINT>(size / sizeof(uint32_t)), data,
                                           static_cast<UINT>(offset / sizeof(uint32_t)));
    return *this;
  }

  CommandBuffer& CommandBuffer::writeComputePushConstants(const void* data, size_t size, size_t offset) {
    DX_ASSERT(size > 0, "Size must be greater than 0");
    DX_ASSERT(offset + size <= 128, "Push constant size must be less than or equal to 128 bytes");
    DX_ASSERT(size % sizeof(uint32_t) == 0, "Push constant size must be a multiple of 4 bytes (32 bits) for DX12");
    DX_ASSERT(offset % sizeof(uint32_t) == 0, "Push constant offset must be a multiple of 4 bytes (32 bits) for DX12");
    cmdList->SetComputeRoot32BitConstants(cPipeline->constantSlot, static_cast<UINT>(size / sizeof(uint32_t)), data,
                                          static_cast<UINT>(offset / sizeof(uint32_t)));
    return *this;
  }

  CommandBuffer& CommandBuffer::pushGraphicsUniformBuffer(const BufferRef& buffer, uint32_t binding, size_t offset) {
    DX_ASSERT(buffer.dxGetResource() != nullptr, "Buffer resource is null");
    DX_ASSERT(offset < buffer.size(), "Offset is out of bounds of the buffer");
    cmdList->SetGraphicsRootConstantBufferView(binding + gPipeline->cbvOffset, buffer.dxGetResource()->GetGPUVirtualAddress() + offset);
    return *this;
  }

  CommandBuffer& CommandBuffer::pushGraphicsStorageBuffer(const BufferRef& buffer, uint32_t binding, size_t offset) {
    DX_ASSERT(buffer.dxGetResource() != nullptr, "Buffer resource is null");
    DX_ASSERT(offset < buffer.size(), "Offset is out of bounds of the buffer");
    cmdList->SetGraphicsRootShaderResourceView(binding + gPipeline->srvOffset, buffer.dxGetResource()->GetGPUVirtualAddress() + offset);
    return *this;
  }

  CommandBuffer& CommandBuffer::bindGraphicsDescriptorSet(const DescriptorSet& set, uint32_t setIndex) {
    DX_ASSERT(set.dxGetGpuHandle().ptr != 0, "Descriptor set GPU handle is null");
    cmdList->SetGraphicsRootDescriptorTable(setIndex, set.dxGetGpuHandle());
    return *this;
  }

  CommandBuffer& CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    cmdList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
    STATS_INC(drawCalls);
    return *this;
  }

  CommandBuffer& CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset,
                                            uint32_t firstInstance) {
    cmdList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    STATS_INC(drawCalls);
    return *this;
  }

  CommandBuffer& CommandBuffer::drawIndirect(const BufferRef& buffer, uint32_t drawCount, uint32_t offset) {
    DX_ASSERT(buffer.dxGetResource() != nullptr, "Buffer resource is null");
    DX_ASSERT(drawCount > 0, "Draw count must be greater than 0");
    DX_ASSERT(offset + drawCount * sizeof(D3D12_DRAW_ARGUMENTS) <= buffer.size(), "Buffer overflow");

    cmdList->ExecuteIndirect(rhi::RHI::get().dxGetMembers().drawIndirectSignature.Get(), drawCount, buffer.dxGetResource(), offset, nullptr,
                             0);
    STATS_INC(drawCalls);
    return *this;
  }

  CommandBuffer& CommandBuffer::drawIndexedIndirect(const BufferRef& buffer, uint32_t drawCount, uint32_t offset) {
    DX_ASSERT(buffer.dxGetResource() != nullptr, "Buffer resource is null");
    DX_ASSERT(drawCount > 0, "Draw count must be greater than 0");
    DX_ASSERT(offset + drawCount * sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) <= buffer.size(), "Buffer overflow");

    cmdList->ExecuteIndirect(rhi::RHI::get().dxGetMembers().drawIndexedIndirectSignature.Get(), drawCount, buffer.dxGetResource(), offset,
                             nullptr, 0);
    STATS_INC(drawCalls);
    return *this;
  }

  CommandBuffer& CommandBuffer::drawIndirectCount(const BufferRef& buffer, const BufferRef& countBuffer, uint32_t maxDrawCount,
                                                  uint32_t drawOffset, uint32_t countBufferOffset) {
    DX_ASSERT(buffer.dxGetResource() != nullptr, "Buffer resource is null");
    DX_ASSERT(countBuffer.dxGetResource() != nullptr, "Count buffer resource is null");
    DX_ASSERT(maxDrawCount > 0, "Max draw count must be greater than 0");
    DX_ASSERT(drawOffset + maxDrawCount * sizeof(D3D12_DRAW_ARGUMENTS) <= buffer.size(), "Buffer overflow");
    DX_ASSERT(countBufferOffset + sizeof(uint32_t) <= countBuffer.size(), "Count buffer overflow");

    cmdList->ExecuteIndirect(rhi::RHI::get().dxGetMembers().drawIndirectSignature.Get(), maxDrawCount, buffer.dxGetResource(), drawOffset,
                             countBuffer.dxGetResource(), countBufferOffset);
    STATS_INC(drawCalls);
    return *this;
  }

  CommandBuffer& CommandBuffer::drawIndexedIndirectCount(const BufferRef& buffer, const BufferRef& countBuffer, uint32_t maxDrawCount,
                                                         uint32_t drawOffset, uint32_t countBufferOffset) {
    DX_ASSERT(buffer.dxGetResource() != nullptr, "Buffer resource is null");
    DX_ASSERT(countBuffer.dxGetResource() != nullptr, "Count buffer resource is null");
    DX_ASSERT(maxDrawCount > 0, "Max draw count must be greater than 0");
    DX_ASSERT(drawOffset + maxDrawCount * sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) <= buffer.size(), "Buffer overflow");
    DX_ASSERT(countBufferOffset + sizeof(uint32_t) <= countBuffer.size(), "Count buffer overflow");

    cmdList->ExecuteIndirect(rhi::RHI::get().dxGetMembers().drawIndexedIndirectSignature.Get(), maxDrawCount, buffer.dxGetResource(),
                             drawOffset, countBuffer.dxGetResource(), countBufferOffset);
    STATS_INC(drawCalls);
    return *this;
  }

  CommandBuffer& CommandBuffer::endRendering() {
    cmdList->EndRenderPass();
    return *this;
  }

  CommandBuffer& CommandBuffer::setViewport(const maths::Viewport& viewport) {
    D3D12_VIEWPORT dxViewport{
        .TopLeftX = viewport.x,
        .TopLeftY = viewport.y,
        .Width = viewport.width,
        .Height = viewport.height,
        .MinDepth = viewport.minDepth,
        .MaxDepth = viewport.maxDepth,
    };
    cmdList->RSSetViewports(1, &dxViewport);
    return *this;
  }

  CommandBuffer& CommandBuffer::setScissor(const maths::Rect2D<uint32_t, uint32_t>& rect) {
    D3D12_RECT dxRect{
        .left = static_cast<LONG>(rect.x),
        .top = static_cast<LONG>(rect.y),
        .right = static_cast<LONG>(rect.x + rect.width),
        .bottom = static_cast<LONG>(rect.y + rect.height),
    };
    cmdList->RSSetScissorRects(1, &dxRect);
    return *this;
  }

  CommandBuffer& CommandBuffer::transitionImage(const rhi::ImageRef& image, ImageLayout oldLayout, ImageLayout newLayout) {
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(image, raw(oldLayout), raw(newLayout));
    cmdList->ResourceBarrier(1, &barrier);
    return *this;
  }

  CommandBuffer& CommandBuffer::transitionImage(const ImageLayoutTransition& transition) {
    return transitionImage(transition.imageRef, transition.oldLayout, transition.newLayout);
  }

  CommandBuffer& CommandBuffer::transitionImages(std::span<const ImageLayoutTransition> transitions) {
    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    barriers.reserve(transitions.size());

    for (const auto& transition : transitions) {
      barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(transition.imageRef, raw(transition.oldLayout), raw(transition.newLayout)));
    }
    cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    return *this;
  }

  CommandBuffer& CommandBuffer::copyBufferRegion(const rhi::BufferRef& dst, const rhi::BufferRef& src, size_t dstOffset, size_t srcOffset,
                                                 size_t size) {
    DX_ASSERT(size > 0, "Size must be greater than 0");
    DX_ASSERT(dstOffset + size <= dst.size(), "Destination buffer overflow");
    DX_ASSERT(srcOffset + size <= src.size(), "Source buffer overflow");
    DX_ASSERT(dst.dxGetResource() != nullptr, "Destination buffer resource is null");
    DX_ASSERT(src.dxGetResource() != nullptr, "Source buffer resource is null");
    cmdList->CopyBufferRegion(dst, dstOffset, src, srcOffset, size);
    return *this;
  }

  CommandBuffer& CommandBuffer::copyImageRegion(const rhi::ImageRef& dst, const rhi::ImageRef& src, size_t dstOffsetX, size_t dstOffsetY,
                                                size_t srcOffsetX, size_t srcOffsetY, size_t width, size_t height, size_t dstMipLevel,
                                                size_t srcMipLevel) {
    DX_ASSERT(width > 0 && height > 0, "Width and height must be greater than 0");
    DX_ASSERT(dst.dxGetResource() != nullptr, "Destination image resource is null");
    DX_ASSERT(src.dxGetResource() != nullptr, "Source image resource is null");

    D3D12_TEXTURE_COPY_LOCATION dstLocation{
        .pResource = dst.dxGetResource(),
        .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .SubresourceIndex = static_cast<UINT>(dstMipLevel),
    };

    D3D12_TEXTURE_COPY_LOCATION srcLocation{
        .pResource = src.dxGetResource(),
        .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .SubresourceIndex = static_cast<UINT>(srcMipLevel),
    };

    D3D12_BOX srcBox{
        .left = static_cast<UINT>(srcOffsetX),
        .top = static_cast<UINT>(srcOffsetY),
        .front = 0,
        .right = static_cast<UINT>(srcOffsetX + width),
        .bottom = static_cast<UINT>(srcOffsetY + height),
        .back = 1,
    };

    cmdList->CopyTextureRegion(&dstLocation, static_cast<UINT>(dstOffsetX), static_cast<UINT>(dstOffsetY), 0, &srcLocation, &srcBox);
    return *this;
  }

  CommandBuffer& CommandBuffer::copyBufferToImage(const rhi::BufferRef& buffer, const rhi::ImageRef& image, size_t width, size_t height,
                                                  size_t mipLevel, size_t bufferOffset, size_t offsetX, size_t offsetY) {
    DX_ASSERT(width > 0 && height > 0, "Width and height must be greater than 0");
    DX_ASSERT(buffer.dxGetResource() != nullptr, "Buffer resource is null");
    DX_ASSERT(image.dxGetResource() != nullptr, "Image resource is null");

    D3D12_TEXTURE_COPY_LOCATION dstLocation{
        .pResource = image.dxGetResource(),
        .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .SubresourceIndex = static_cast<UINT>(mipLevel),
    };

    D3D12_TEXTURE_COPY_LOCATION srcLocation{
        .pResource = buffer.dxGetResource(),
        .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
        .PlacedFootprint =
            {
                .Offset = bufferOffset,
                .Footprint =
                    {
                        .Format = raw(image.format()),
                        .Width = static_cast<UINT>(width),
                        .Height = static_cast<UINT>(height),
                        .Depth = 1,
                        .RowPitch = static_cast<UINT>(buffer.size()), // Assuming the buffer is tightly packed
                    },
            },
    };

    cmdList->CopyTextureRegion(&dstLocation, static_cast<UINT>(offsetX), static_cast<UINT>(offsetY), 0, &srcLocation, nullptr);
    return *this;
  }

  CommandBuffer& CommandBuffer::blitImage(const rhi::ImageRef& src, const rhi::ImageRef& dst) {
    DX_ASSERT(src.dxGetResource() != nullptr, "Source image resource is null");
    DX_ASSERT(dst.dxGetResource() != nullptr, "Destination image resource is null");

    auto& rhi = RHI::get();
    auto& pipeline = rhi.dxGetBlitPipeline(dst.format());

    bindGraphicsPipeline(pipeline);

    cmdList->SetGraphicsRootShaderResourceView(0, src.dxGetResource()->GetGPUVirtualAddress());

    cmdList->DrawInstanced(3, 1, 0, 0);

    return *this;
  }

  void CommandBuffer::end() {
    auto res = cmdList->Close();
    DX_ASSERT(SUCCEEDED(res), "Failed to close command list: {}", res);
  }
  void CommandBuffer::label(std::string_view name) {
    cmdList->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
  }
  CommandBuffer::CommandBuffer(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator,
                               Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> cmdList)
      : allocator(std::move(allocator)), cmdList(std::move(cmdList)) {}
  ID3D12GraphicsCommandList* CommandBuffer::operator->() const { return cmdList.Get(); }
  CommandBuffer::operator ID3D12GraphicsCommandList*() const { return cmdList.Get(); }
  CommandBuffer::operator ID3D12CommandList* const*() { return reinterpret_cast<ID3D12CommandList* const*>(cmdList.GetAddressOf()); }
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>& CommandBuffer::ComPtr() { return cmdList; }

} // namespace kt::rhi