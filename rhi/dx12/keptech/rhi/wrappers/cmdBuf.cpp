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
                               Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList)
      : allocator(std::move(allocator)), cmdList(std::move(cmdList)) {}
  ID3D12GraphicsCommandList* CommandBuffer::operator->() const { return cmdList.Get(); }
  CommandBuffer::operator ID3D12GraphicsCommandList*() const { return cmdList.Get(); }
  CommandBuffer::operator ID3D12CommandList* const*() { return reinterpret_cast<ID3D12CommandList* const*>(cmdList.GetAddressOf()); }
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& CommandBuffer::ComPtr() { return cmdList; }

} // namespace kt::rhi