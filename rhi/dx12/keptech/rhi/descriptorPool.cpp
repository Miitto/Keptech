#pragma once

#include "descriptorPool.hpp"
#include "d3dx12.h"
#include "descriptorSet.hpp"
#include "dx/constants.hpp"
#include "dx/descriptorLayout.hpp"
#include "dx/dx-logger.hpp"

namespace kt::rhi {
  DescriptorSet DescriptorPool::allocate(const DescriptorLayout& layout) {
    auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pool->GetCPUDescriptorHandleForHeapStart(), count, CBV_SRV_UAV_DESCRIPTOR_SIZE);
    auto gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(pool->GetGPUDescriptorHandleForHeapStart(), count, CBV_SRV_UAV_DESCRIPTOR_SIZE);

    for (const auto& range : layout.dxGetRanges()) {
      count += range.NumDescriptors;
    }

    return DescriptorSet{cpuHandle, gpuHandle};
  }
} // namespace kt::rhi