#include "cmdBuf.hpp"
#include "bufferRef.hpp"
#include "d3dx12.h"
#include "descriptorSet.hpp"
#include "dx/dx-logger.hpp"
#include "image.hpp"
#include "imageRef.hpp"
#include "keptech/maths/sizes.hpp"
#include "pipeline.hpp"
#include "rhi.hpp"
#include <WinPixEventRuntime/pix3.h>

#ifndef KT_DISABLE_STATS
#define STATS_INC(x) ++(RHI::get().getStats().x)
#else
#define STATS_INC(x)
#endif

namespace kt::rhi {
  CommandBuffer& CommandBufferBase::bindGraphicsPipeline(this CommandBuffer& self, const Pipeline& pipeline) {
    self.cmdList->SetPipelineState(pipeline.pipelineState.Get());
    std::array<ID3D12DescriptorHeap*, 2> heaps = {
        rhi::RHI::get().dxGetMembers().samplerHeap.heap.Get(),
        rhi::RHI::get().dxGetMembers().cbvSrvUavHeap.heap.Get(),
    };
    self.cmdList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
    self.cmdList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
    self.cmdList->IASetPrimitiveTopology(pipeline.primitiveTopology);
    self.gPipeline = &pipeline;

    STATS_INC(pipelineSwitches);

    return self;
  }

  CommandBuffer& CommandBufferBase::bindComputePipeline(this CommandBuffer& self, const Pipeline& pipeline) {
    self.cmdList->SetPipelineState(pipeline.pipelineState.Get());
    std::array<ID3D12DescriptorHeap*, 2> heaps = {rhi::RHI::get().dxGetMembers().cbvSrvUavHeap.heap.Get(),
                                                  rhi::RHI::get().dxGetMembers().samplerHeap.heap.Get()};
    self.cmdList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
    self.cmdList->SetComputeRootSignature(pipeline.rootSignature.Get());
    self.cPipeline = &pipeline;

    STATS_INC(pipelineSwitches);

    return self;
  }

  CommandBuffer& CommandBufferBase::clearColorImage(this CommandBuffer& self, const rhi::ImageRef& image,
                                                    const std::array<float, 4>& clearColor) {
    self.cmdList->ClearRenderTargetView(image.dxGetRtvDsvHandle(), clearColor.data(), 0, nullptr);

    return self;
  }

  CommandBuffer& CommandBufferBase::clearDepthStencilImage(this CommandBuffer& self, const rhi::ImageRef& image, float clearDepth,
                                                           uint8_t clearStencil) {
    D3D12_CLEAR_FLAGS clearFlags = static_cast<D3D12_CLEAR_FLAGS>(0);
    switch (image.format()) {
    // TODO: Other depth formats
    case ImageFormat::D16_UNORM:
    case ImageFormat::D32_FLOAT:
      clearFlags |= D3D12_CLEAR_FLAG_DEPTH;
      break;
    default:
      break;
    }
    self.cmdList->ClearDepthStencilView(image.dxGetRtvDsvHandle(), clearFlags, clearDepth, clearStencil, 0, nullptr);

    return self;
  }

  CommandBuffer& CommandBufferBase::beginRendering(this CommandBuffer& self, const std::span<ColorAttachmentDesc> colorAttachments,
                                                   std::optional<DepthStencilAttachmentDesc> depthStencilAttachment) {
    Vec<D3D12_RENDER_PASS_RENDER_TARGET_DESC> renderTargetDescs(colorAttachments.size());
    for (size_t i = 0; i < colorAttachments.size(); ++i) {
      renderTargetDescs[i] = {
          .cpuDescriptor = colorAttachments[i].imageRef.dxGetRtvDsvHandle(),
          .BeginningAccess = {},
          .EndingAccess = {},
      };
      renderTargetDescs[i].cpuDescriptor.ptr += (colorAttachments[i].mipLevel * RTV_DESCRIPTOR_SIZE);

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

    self.cmdList->BeginRenderPass(static_cast<UINT>(renderTargetDescs.size()), renderTargetDescs.data(),
                                  depthStencilAttachment.has_value() ? &depthStencilDesc : nullptr, D3D12_RENDER_PASS_FLAG_NONE);

    STATS_INC(renderPasses);

    return self;
  }

  CommandBuffer& CommandBufferBase::bindVertexBuffer(this CommandBuffer& self, size_t slot, const BufferRef& buffer, size_t stride,
                                                     size_t offset) {
    D3D12_VERTEX_BUFFER_VIEW view{
        .BufferLocation = buffer.dxGetResource()->GetGPUVirtualAddress() + offset,
        .SizeInBytes = static_cast<UINT>(buffer.size() - offset),
        .StrideInBytes = static_cast<UINT>(stride),
    };
    self.cmdList->IASetVertexBuffers(static_cast<UINT>(slot), 1, &view);
    return self;
  }
  CommandBuffer& CommandBufferBase::bindVertexBuffers(this CommandBuffer& self, size_t firstSlot,
                                                      const std::span<const VertexBufferBinding> bindings) {
    Vec<D3D12_VERTEX_BUFFER_VIEW> views;
    views.reserve(bindings.size());
    for (const auto& binding : bindings) {
      D3D12_VERTEX_BUFFER_VIEW view{
          .BufferLocation = binding.buffer.dxGetResource()->GetGPUVirtualAddress() + binding.offset,
          .SizeInBytes = static_cast<UINT>(binding.buffer.size() - binding.offset),
          .StrideInBytes = static_cast<UINT>(binding.stride),
      };
      views.push_back(view);
    }
    self.cmdList->IASetVertexBuffers(static_cast<UINT>(firstSlot), static_cast<UINT>(views.size()), views.data());
    return self;
  }

  CommandBuffer& CommandBufferBase::bindIndexBuffer(this CommandBuffer& self, const BufferRef& buffer, IndexType indexType, size_t offset) {
    D3D12_INDEX_BUFFER_VIEW view{
        .BufferLocation = buffer.dxGetResource()->GetGPUVirtualAddress() + offset,
        .SizeInBytes = static_cast<UINT>(buffer.size() - offset),
        .Format = indexType == IndexType::UInt16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
    };
    self.cmdList->IASetIndexBuffer(&view);
    return self;
  }

  CommandBuffer& CommandBufferBase::writeGraphicsPushConstants(this CommandBuffer& self, const void* data, size_t size, size_t offset) {
    DX_ASSERT(size > 0, "Size must be greater than 0");
    DX_ASSERT(offset + size <= 128, "Push constant size must be less than or equal to 128 bytes");
    DX_ASSERT(size % sizeof(uint32_t) == 0, "Push constant size must be a multiple of 4 bytes (32 bits) for DX12");
    DX_ASSERT(offset % sizeof(uint32_t) == 0, "Push constant offset must be a multiple of 4 bytes (32 bits) for DX12");
    self.cmdList->SetGraphicsRoot32BitConstants(self.gPipeline->constantSlot, static_cast<UINT>(size / sizeof(uint32_t)), data,
                                                static_cast<UINT>(offset / sizeof(uint32_t)));
    return self;
  }

  CommandBuffer& CommandBufferBase::writeComputePushConstants(this CommandBuffer& self, const void* data, size_t size, size_t offset) {
    DX_ASSERT(size > 0, "Size must be greater than 0");
    DX_ASSERT(offset + size <= 128, "Push constant size must be less than or equal to 128 bytes");
    DX_ASSERT(size % sizeof(uint32_t) == 0, "Push constant size must be a multiple of 4 bytes (32 bits) for DX12");
    DX_ASSERT(offset % sizeof(uint32_t) == 0, "Push constant offset must be a multiple of 4 bytes (32 bits) for DX12");
    self.cmdList->SetComputeRoot32BitConstants(self.cPipeline->constantSlot, static_cast<UINT>(size / sizeof(uint32_t)), data,
                                               static_cast<UINT>(offset / sizeof(uint32_t)));
    return self;
  }

  CommandBuffer& CommandBufferBase::pushGraphicsUniformBuffer(this CommandBuffer& self, const BufferRef& buffer, uint32_t binding,
                                                              size_t offset) {
    DX_ASSERT(buffer.dxGetResource() != nullptr, "Buffer resource is null");
    DX_ASSERT(offset < buffer.size(), "Offset is out of bounds of the buffer");
    self.cmdList->SetGraphicsRootConstantBufferView(binding + self.gPipeline->cbvOffset,
                                                    buffer.dxGetResource()->GetGPUVirtualAddress() + offset);
    return self;
  }

  CommandBuffer& CommandBufferBase::pushGraphicsStorageBuffer(this CommandBuffer& self, const BufferRef& buffer, uint32_t binding,
                                                              size_t offset) {
    DX_ASSERT(buffer.dxGetResource() != nullptr, "Buffer resource is null");
    DX_ASSERT(offset < buffer.size(), "Offset is out of bounds of the buffer");
    self.cmdList->SetGraphicsRootShaderResourceView(binding + self.gPipeline->srvOffset,
                                                    buffer.dxGetResource()->GetGPUVirtualAddress() + offset);
    return self;
  }

  CommandBuffer& CommandBufferBase::bindGraphicsDescriptorSet(this CommandBuffer& self, const DescriptorSet& set, uint32_t setIndex) {
    DX_ASSERT(set.dxGetGpuHandle().ptr != 0, "Descriptor set GPU handle is null");
    self.cmdList->SetGraphicsRootDescriptorTable(setIndex, set.dxGetGpuHandle());
    return self;
  }

  CommandBuffer& CommandBufferBase::draw(this CommandBuffer& self, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex,
                                         uint32_t firstInstance) {
    self.cmdList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
    STATS_INC(drawCalls);
    return self;
  }

  CommandBuffer& CommandBufferBase::drawIndexed(this CommandBuffer& self, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex,
                                                int32_t vertexOffset, uint32_t firstInstance) {
    self.cmdList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    STATS_INC(drawCalls);
    return self;
  }

  CommandBuffer& CommandBufferBase::drawIndirect(this CommandBuffer& self, const BufferRef& buffer, uint32_t drawCount, uint32_t offset) {
    DX_ASSERT(buffer.dxGetResource() != nullptr, "Buffer resource is null");
    DX_ASSERT(drawCount > 0, "Draw count must be greater than 0");
    DX_ASSERT(offset + drawCount * sizeof(D3D12_DRAW_ARGUMENTS) <= buffer.size(), "Buffer overflow");

    self.cmdList->ExecuteIndirect(rhi::RHI::get().dxGetMembers().drawIndirectSignature.Get(), drawCount, buffer.dxGetResource(), offset,
                                  nullptr, 0);
    STATS_INC(drawCalls);
    return self;
  }

  CommandBuffer& CommandBufferBase::drawIndexedIndirect(this CommandBuffer& self, const BufferRef& buffer, uint32_t drawCount,
                                                        uint32_t offset) {
    DX_ASSERT(buffer.dxGetResource() != nullptr, "Buffer resource is null");
    DX_ASSERT(drawCount > 0, "Draw count must be greater than 0");
    DX_ASSERT(offset + drawCount * sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) <= buffer.size(), "Buffer overflow");

    self.cmdList->ExecuteIndirect(rhi::RHI::get().dxGetMembers().drawIndexedIndirectSignature.Get(), drawCount, buffer.dxGetResource(),
                                  offset, nullptr, 0);
    STATS_INC(drawCalls);
    return self;
  }

  CommandBuffer& CommandBufferBase::drawIndirectCount(this CommandBuffer& self, const BufferRef& buffer, const BufferRef& countBuffer,
                                                      uint32_t maxDrawCount, uint32_t drawOffset, uint32_t countBufferOffset) {
    DX_ASSERT(buffer.dxGetResource() != nullptr, "Buffer resource is null");
    DX_ASSERT(countBuffer.dxGetResource() != nullptr, "Count buffer resource is null");
    DX_ASSERT(maxDrawCount > 0, "Max draw count must be greater than 0");
    DX_ASSERT(drawOffset + maxDrawCount * sizeof(D3D12_DRAW_ARGUMENTS) <= buffer.size(), "Buffer overflow");
    DX_ASSERT(countBufferOffset + sizeof(uint32_t) <= countBuffer.size(), "Count buffer overflow");

    self.cmdList->ExecuteIndirect(rhi::RHI::get().dxGetMembers().drawIndirectSignature.Get(), maxDrawCount, buffer.dxGetResource(),
                                  drawOffset, countBuffer.dxGetResource(), countBufferOffset);
    STATS_INC(drawCalls);
    return self;
  }

  CommandBuffer& CommandBufferBase::drawIndexedIndirectCount(this CommandBuffer& self, const BufferRef& buffer,
                                                             const BufferRef& countBuffer, uint32_t maxDrawCount, uint32_t drawOffset,
                                                             uint32_t countBufferOffset) {
    DX_ASSERT(buffer.dxGetResource() != nullptr, "Buffer resource is null");
    DX_ASSERT(countBuffer.dxGetResource() != nullptr, "Count buffer resource is null");
    DX_ASSERT(maxDrawCount > 0, "Max draw count must be greater than 0");
    DX_ASSERT(drawOffset + maxDrawCount * sizeof(D3D12_DRAW_INDEXED_ARGUMENTS) <= buffer.size(), "Buffer overflow");
    DX_ASSERT(countBufferOffset + sizeof(uint32_t) <= countBuffer.size(), "Count buffer overflow");

    self.cmdList->ExecuteIndirect(rhi::RHI::get().dxGetMembers().drawIndexedIndirectSignature.Get(), maxDrawCount, buffer.dxGetResource(),
                                  drawOffset, countBuffer.dxGetResource(), countBufferOffset);
    STATS_INC(drawCalls);
    return self;
  }

  CommandBuffer& CommandBufferBase::drawMeshTasks(this CommandBuffer& self, uint32_t taskGroupCountX, uint32_t taskGroupCountY,
                                                  uint32_t taskGroupCountZ) {
    DX_ASSERT(taskGroupCountX > 0 && taskGroupCountY > 0 && taskGroupCountZ > 0, "Task group counts must be greater than 0: ({}, {}, {})",
              taskGroupCountX, taskGroupCountY, taskGroupCountZ);
    self.cmdList->DispatchMesh(taskGroupCountX, taskGroupCountY, taskGroupCountZ);
    STATS_INC(drawCalls);
    return self;
  }

  CommandBuffer& CommandBufferBase::endRendering(this CommandBuffer& self) {
    self.cmdList->EndRenderPass();
    return self;
  }

  CommandBuffer& CommandBufferBase::setViewport(this CommandBuffer& self, const maths::Viewport& viewport) {
    D3D12_VIEWPORT dxViewport{
        .TopLeftX = viewport.x,
        .TopLeftY = viewport.y,
        .Width = viewport.width,
        .Height = viewport.height,
        .MinDepth = viewport.minDepth,
        .MaxDepth = viewport.maxDepth,
    };
    self.cmdList->RSSetViewports(1, &dxViewport);
    return self;
  }

  CommandBuffer& CommandBufferBase::setScissor(this CommandBuffer& self, const maths::Rect2D<uint32_t, uint32_t>& rect) {
    D3D12_RECT dxRect{
        .left = static_cast<LONG>(rect.x),
        .top = static_cast<LONG>(rect.y),
        .right = static_cast<LONG>(rect.x + rect.width),
        .bottom = static_cast<LONG>(rect.y + rect.height),
    };
    self.cmdList->RSSetScissorRects(1, &dxRect);
    return self;
  }

  CommandBuffer& CommandBufferBase::transitionImage(this CommandBuffer& self, const rhi::ImageRef& image, ImageLayout oldLayout,
                                                    ImageLayout newLayout) {
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(image, raw(oldLayout), raw(newLayout));
    self.cmdList->ResourceBarrier(1, &barrier);
    return self;
  }

  CommandBuffer& CommandBufferBase::transitionImage(this CommandBuffer& self, const ImageLayoutTransition& transition) {
    return self.transitionImage(transition.imageRef, transition.oldLayout, transition.newLayout);
  }

  CommandBuffer& CommandBufferBase::transitionImages(this CommandBuffer& self, std::span<const ImageLayoutTransition> transitions) {
    Vec<D3D12_RESOURCE_BARRIER> barriers;
    barriers.reserve(transitions.size());

    for (const auto& transition : transitions) {
      barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(transition.imageRef, raw(transition.oldLayout), raw(transition.newLayout)));
    }
    self.cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
    return self;
  }

  CommandBuffer& CommandBufferBase::copyBufferRegion(this CommandBuffer& self, const rhi::BufferRef& dst, const rhi::BufferRef& src,
                                                     size_t dstOffset, size_t srcOffset, size_t size) {
    DX_ASSERT(
        size > 0,
        "Size must be greater than 0. Attempted to copy 0 bytes from src (\"{}\") to dst (\"{}\") with dstOffset {} and srcOffset {}.",
        src.getName(), dst.getName(), dstOffset, srcOffset);
    DX_ASSERT(dstOffset + size <= dst.size(),
              "Destination buffer overflow. Dst (\"{}\") has size {} bytes, but the copy operation would write up to {} bytes ({} "
              "overflow). Cannot copy "
              "from src (\"{}\") to dst (\"{}\") with dstOffset {} and size {}.",
              dst.getName(), dst.size(), dstOffset + size, dstOffset + size - dst.size(), src.getName(), dst.getName(), dst.size(),
              dstOffset + size);
    DX_ASSERT(srcOffset + size <= src.size(),
              "Source buffer overflow. Src (\"{}\") has size {} bytes, but the copy operation would read up to {} bytes ({} overflow). "
              "Cannot copy "
              "from src (\"{}\") to dst (\"{}\") with srcOffset {} and size {}.",
              src.getName(), src.size(), srcOffset + size, srcOffset + size - src.size(), src.getName(), dst.getName(), src.size(),
              srcOffset + size);
    DX_ASSERT(
        dst.dxGetResource() != nullptr,
        "Destination buffer resource is null. Attempted to copy from src (\"{}\") to dst (\"{}\") with dstOffset {} and srcOffset {}.",
        src.getName(), dst.getName(), dstOffset, srcOffset);
    DX_ASSERT(src.dxGetResource() != nullptr,
              "Source buffer resource is null. Attempted to copy from src (\"{}\") to dst (\"{}\") with dstOffset {} and srcOffset {}.",
              src.getName(), dst.getName(), dstOffset, srcOffset);
    self.cmdList->CopyBufferRegion(dst, dstOffset, src, srcOffset, size);
    return self;
  }

  CommandBuffer& CommandBufferBase::copyImageRegion(this CommandBuffer& self, const rhi::ImageRef& dst, const rhi::ImageRef& src,
                                                    size_t dstOffsetX, size_t dstOffsetY, size_t srcOffsetX, size_t srcOffsetY,
                                                    size_t width, size_t height, size_t dstMipLevel, size_t srcMipLevel) {
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

    self.cmdList->CopyTextureRegion(&dstLocation, static_cast<UINT>(dstOffsetX), static_cast<UINT>(dstOffsetY), 0, &srcLocation, &srcBox);
    return self;
  }

  CommandBuffer& CommandBufferBase::copyBufferToImage(this CommandBuffer& self, const rhi::BufferRef& buffer, const rhi::ImageRef& image,
                                                      size_t width, size_t height, size_t layer, size_t mipLevel, size_t bufferOffset,
                                                      size_t offsetX, size_t offsetY) {
    DX_ASSERT(width > 0 && height > 0, "Width and height must be greater than 0");
    DX_ASSERT(buffer.dxGetResource() != nullptr, "Buffer resource is null");
    DX_ASSERT(image.dxGetResource() != nullptr, "Image resource is null");

    auto& rhi = RHI::get();
    auto device = rhi.dxGetDevice();

    D3D12_TEXTURE_COPY_LOCATION dstLocation{
        .pResource = image.dxGetResource(),
        .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        .SubresourceIndex = static_cast<UINT>(mipLevel + (layer * image.mips())),
    };

    size_t bytesPerPixel = size(image.format());

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
                        .RowPitch = static_cast<UINT>(bytesPerPixel * width), // Assuming the buffer is tightly packed
                    },
            },
    };

    self.cmdList->CopyTextureRegion(&dstLocation, static_cast<UINT>(offsetX), static_cast<UINT>(offsetY), 0, &srcLocation, nullptr);
    return self;
  }

  CommandBuffer& CommandBufferBase::blitImage(this CommandBuffer& self, const rhi::ImageRef& src, rhi::ImageLayout srcImageStart,
                                              rhi::ImageLayout srcImageEnd, const rhi::ImageRef& dst, rhi::ImageLayout dstImageStart,
                                              rhi::ImageLayout dstImageEnd) {
    DX_ASSERT(src.dxGetResource() != nullptr, "Source image resource is null");
    DX_ASSERT(dst.dxGetResource() != nullptr, "Destination image resource is null");
    auto evt = self.scopedEvent("Blit Image", {255, 255, 0});

    auto& rhi = RHI::get();
    auto& pipeline = rhi.dxGetBlitPipeline(dst.format());

    if (srcImageStart != ImageLayout::ShaderReadOnly) {
      self.transitionImage(src, srcImageStart, ImageLayout::ShaderReadOnly);
    }
    if (dstImageStart != ImageLayout::RenderTarget) {
      self.transitionImage(dst, dstImageStart, ImageLayout::RenderTarget);
    }

    self.bindGraphicsPipeline(pipeline);

    struct BlitPushConstants {
      glm::vec2 offset;
      glm::vec2 extent;
      float z;
      float level;
      uint64_t src;
    } pc{
        .offset = {},
        .extent = {static_cast<float>(dst.extent().x), static_cast<float>(dst.extent().y)},
        .z = 0.f,
        .level = 0.f,
        .src = src.getTextureIndex(),
    };

    DX_ASSERT(pc.src != UINT64_MAX,
              "Source image must have a valid texture index. Ensure that the source image was created with usage including "
              "`kt::rhi::CommandBufferBase::getBlitSrcUsage()`. The Render Graph also supports `requestRenderTargetsBlitable()` to "
              "automatically set this usage for render targets created by it.");

    self.cmdList->SetGraphicsRoot32BitConstants(0, sizeof(BlitPushConstants) / sizeof(uint32_t), &pc, 0);

    D3D12_VIEWPORT viewport{
        .TopLeftX = 0.f,
        .TopLeftY = 0.f,
        .Width = static_cast<float>(dst.extent().x),
        .Height = static_cast<float>(dst.extent().y),
        .MinDepth = 0.f,
        .MaxDepth = 1.f,
    };
    D3D12_RECT scissorRect{
        .left = 0,
        .top = 0,
        .right = static_cast<LONG>(dst.extent().x),
        .bottom = static_cast<LONG>(dst.extent().y),
    };
    self.cmdList->RSSetViewports(1, &viewport);
    self.cmdList->RSSetScissorRects(1, &scissorRect);

    auto rtvHandle = dst.dxGetRtvDsvHandle();
    DX_ASSERT(rtvHandle.ptr != 0, "Destination image must have a valid RTV handle");

    // Discard the previous contents of the render target before rendering to it
    D3D12_RENDER_PASS_RENDER_TARGET_DESC renderTargetDesc{
        .cpuDescriptor = rtvHandle,
        .BeginningAccess =
            {
                .Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD,
            },
        .EndingAccess =
            {
                .Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE,
            },
    };
    self.cmdList->BeginRenderPass(1, &renderTargetDesc, nullptr, D3D12_RENDER_PASS_FLAG_NONE);

    self.cmdList->DrawInstanced(3, 1, 0, 0);

    self.cmdList->EndRenderPass();

    if (srcImageEnd != ImageLayout::ShaderReadOnly) {
      self.transitionImage(src, ImageLayout::ShaderReadOnly, srcImageEnd);
    }

    if (dstImageEnd != ImageLayout::RenderTarget) {
      self.transitionImage(dst, ImageLayout::RenderTarget, dstImageEnd);
    }

    return self;
  }

  rhi::ImageLayout CommandBufferBase::getOptimalBlitSrcLayout() { return rhi::ImageLayout::ShaderReadOnly; }

  rhi::ImageLayout CommandBufferBase::getOptimalBlitDstLayout() { return rhi::ImageLayout::RenderTarget; }
  rhi::ImageUsage CommandBufferBase::getBlitSrcUsage() { return rhi::ImageUsage::Sampled; }

  void CommandBufferBase::marker(this CommandBuffer& self, const std::string& name, glm::u8vec3 color) {
    PIXSetMarker(self.cmdList.Get(), PIX_COLOR(color.r, color.g, color.b), name.c_str());
  }

  void CommandBufferBase::beginEvent(this CommandBuffer& self, const std::string& name, glm::u8vec3 color) {
    PIXBeginEvent(self.cmdList.Get(), PIX_COLOR(color.r, color.g, color.b), name.c_str());
  }

  void CommandBufferBase::endEvent(this CommandBuffer& self) { PIXEndEvent(self.cmdList.Get()); }

  void CommandBufferBase::end(this CommandBuffer& self) {
    auto res = self.cmdList->Close();
    DX_ASSERT(SUCCEEDED(res), "Failed to close command list: {}", res);
  }
  void CommandBufferBase::label(this CommandBuffer& self, std::string_view name) {
    self.cmdList->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(name.size()), name.data());
  }
  CommandBuffer::CommandBuffer(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator,
                               Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList10> cmdList)
      : allocator(std::move(allocator)), cmdList(std::move(cmdList)) {}
  ID3D12GraphicsCommandList* CommandBuffer::operator->() const { return cmdList.Get(); }
  CommandBuffer::operator ID3D12GraphicsCommandList*() const { return cmdList.Get(); }
  CommandBuffer::operator ID3D12CommandList* const*() { return reinterpret_cast<ID3D12CommandList* const*>(cmdList.GetAddressOf()); }
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList10>& CommandBuffer::ComPtr() { return cmdList; }

} // namespace kt::rhi