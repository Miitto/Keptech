#pragma once

#include "keptech/maths/sizes.hpp"
#include "keptech/rhi/imageLayout.hpp"
#include "keptech/rhi/imageRef.hpp"
#include "keptech/rhi/loadStoreOps.hpp"
#include <array>
#include <d3d12.h>
#include <span>
#include <wrl.h>

namespace kt::rhi {
  class ImageRef;
  struct Pipeline;
  class BufferRef;
  class DescriptorSet;

  class CommandBuffer {
#include "keptech/rhi/interface/cmdBuf.hpp"

  public:
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>& dxGetAlloc() { return allocator; }

    CommandBuffer(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> cmdList);

    ID3D12GraphicsCommandList* operator->() const;
    operator ID3D12GraphicsCommandList*() const;
    operator ID3D12CommandList* const*();

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>& ComPtr();

  private:
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> cmdList;
    const rhi::Pipeline* gPipeline = nullptr;
    const rhi::Pipeline* cPipeline = nullptr;
  };
} // namespace kt::rhi