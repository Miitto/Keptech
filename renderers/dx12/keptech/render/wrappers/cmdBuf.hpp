#pragma once

#include <d3d12.h>
#include <wrl.h>

namespace kt::rdr {
  class CommandBuffer {
  public:
    CommandBuffer() = default;
    CommandBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList) : cmdList(std::move(cmdList)) {}

    ID3D12GraphicsCommandList* operator->() const { return cmdList.Get(); }
    operator ID3D12GraphicsCommandList*() const { return cmdList.Get(); }

  private:
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cmdList;
  };
} // namespace kt::rdr