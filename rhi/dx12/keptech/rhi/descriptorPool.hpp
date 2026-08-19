#pragma once

#include <d3d12.h>
#include <wrl/client.h>

namespace kt::rhi {
  class DescriptorLayout;
  class DescriptorSet;
  using RawDescriptorPool = Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>;

  class DescriptorPool {
  public:
    DescriptorPool() = default;
    DescriptorPool(RawDescriptorPool pool) : pool(std::move(pool)) {}

    DescriptorSet allocate(const DescriptorLayout& layout);

    [[nodiscard]] auto& dxGetPool(this auto& self) { return self.pool; }

  private:
    RawDescriptorPool pool;
    uint32_t count = 0;
  };
} // namespace kt::rhi