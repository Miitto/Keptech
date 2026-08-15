#pragma once

#include <d3d12.h>
#include <wrl.h>

namespace kt::rhi {
  struct Pipeline {
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;
    D3D12_PRIMITIVE_TOPOLOGY primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    uint32_t constantSlot;
  };
} // namespace kt::rhi