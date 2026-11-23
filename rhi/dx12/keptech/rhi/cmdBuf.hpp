#pragma once

#include "keptech/rhi/interface/cmdBuf_base.hpp"
#include <d3d12.h>
#include <wrl.h>


namespace kt::rhi {
  class BufferRef;
  class DescriptorSet;

  class CommandBuffer : public CommandBufferBase {

  public:
    friend class CommandBufferBase;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>& dxGetAlloc() { return allocator; }
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList10>& dxGetCmd() { return cmdList; }

    CommandBuffer(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList10> cmdList);

    ID3D12GraphicsCommandList* operator->() const;
    operator ID3D12GraphicsCommandList*() const;
    operator ID3D12CommandList* const*();

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList10>& ComPtr();

  private:
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList10> cmdList;
    const rhi::Pipeline* gPipeline = nullptr;
    const rhi::Pipeline* cPipeline = nullptr;
  };
} // namespace kt::rhi