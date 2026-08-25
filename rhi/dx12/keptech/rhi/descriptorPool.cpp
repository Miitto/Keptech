#include "descriptorPool.hpp"
#include "d3dx12.h"
#include "descriptorLayout.hpp"
#include "descriptorSet.hpp"
#include "dx/constants.hpp"
#include "dx/dx-logger.hpp"

namespace kt::rhi {
  DescriptorSet DescriptorPool::allocate(const DescriptorLayout& layout) {
    auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(pool->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(count),
                                                   static_cast<uint32_t>(CBV_SRV_UAV_DESCRIPTOR_SIZE));
    auto gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(pool->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(count),
                                                   static_cast<uint32_t>(CBV_SRV_UAV_DESCRIPTOR_SIZE));

    uint32_t c = 0;
    for (const auto& range : layout.dxGetRanges()) {
      c += range.NumDescriptors;
    }
    count += c;

    DX_ASSERT(count <= capacity, "Descriptor pool overflow: allocated {} descriptors, but capacity is {}", count, capacity);

    DX_DEBUG("Allocated {} descriptors from descriptor pool ({} used, {} remaining)", c, count, capacity - count);

#ifndef NDEBUG
    if (c == 0) {
      DX_WARN("Allocated 0 descriptors from descriptor pool. This may indicate a misconfiguration in the descriptor layout.");
    }
#endif

    return DescriptorSet{cpuHandle, gpuHandle
#ifndef NDEBUG
                         ,
                         c
#endif
    };
  }
} // namespace kt::rhi