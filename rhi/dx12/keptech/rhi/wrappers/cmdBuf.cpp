#include "cmdBuf.hpp"
#include "d3dx12.h"
#include "dx-logger.hpp"
#include "imageRef.hpp"
#include "wrappers/image.hpp"
#include "wrappers/pipeline.hpp"

namespace kt::rhi {
  CommandBuffer& CommandBuffer::bindGraphicsPipeline(const Pipeline& pipeline) {
    cmdList->SetPipelineState(pipeline.pipelineState.Get());
    cmdList->SetGraphicsRootSignature(pipeline.rootSignature.Get());
    cmdList->IASetPrimitiveTopology(pipeline.primitiveTopology);
    return *this;
  }

  CommandBuffer& CommandBuffer::bindComputePipeline(const Pipeline& pipeline) {
    cmdList->SetPipelineState(pipeline.pipelineState.Get());
    cmdList->SetComputeRootSignature(pipeline.rootSignature.Get());
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
    }

    cmdList->BeginRenderPass(static_cast<UINT>(renderTargetDescs.size()), renderTargetDescs.data(),
                             depthStencilAttachment.has_value() ? &depthStencilDesc : nullptr, D3D12_RENDER_PASS_FLAG_NONE);

    return *this;
  }

  CommandBuffer& CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) {
    cmdList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
    return *this;
  }

  CommandBuffer& CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset,
                                            uint32_t firstInstance) {
    cmdList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
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

  CommandBuffer& CommandBuffer::blitImage(const rhi::ImageRef& src, const rhi::ImageRef& dst) {
    cmdList->CopyResource(dst, src);
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