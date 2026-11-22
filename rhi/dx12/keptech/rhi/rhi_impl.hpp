#pragma once

#include "keptech/rhi/buffer.hpp"
#include "keptech/rhi/cmdBuf.hpp"
#include "keptech/rhi/dx/dx-logger.hpp"
#include "keptech/rhi/rhi.hpp"
#include "keptech/std/vec.hpp"


namespace kt::rhi {
  template <typename F>
    requires(std::is_invocable_v<F, CommandBuffer&> && std::is_same_v<std::invoke_result_t<F, CommandBuffer&>, Vec<Buffer>>)
  uint64_t RhiBase::oneshotCopy(this RHI& self, const F& copyFunc) {
    ComPtr<ID3D12CommandAllocator> cmdAlloc;
    DX_REQUIRE(SUCCEEDED(self.m.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&cmdAlloc))),
               "Failed to create copy command allocator");

    self.m.copyCmdList->Reset(cmdAlloc.Get(), nullptr);

    CommandBuffer cmdBuf(std::move(cmdAlloc), self.m.copyCmdList);

    Vec<Buffer> buffers = copyFunc(cmdBuf);

    cmdBuf.end();

    self.m.queues.copy->ExecuteCommandLists(1, cmdBuf);

    uint64_t fenceValue = self.m.copyFence.signal(self.m.queues.copy);

    Vec<D3D12MA::Allocation*> allocsToDrop;
    allocsToDrop.reserve(buffers.size());
    for (auto& buffer : buffers) {
      allocsToDrop.push_back(buffer.dxTakeAllocation());
    }

    self.m.ongoingCopies.push_back({.cmdAlloc = cmdBuf.dxGetAlloc(), .fenceValue = fenceValue, .allocsToDrop = std::move(allocsToDrop)});

    return fenceValue;
  }
} // namespace kt::rhi