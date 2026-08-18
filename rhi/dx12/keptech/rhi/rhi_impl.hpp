#pragma once

#include "keptech/rhi/rhi.hpp"

namespace kt::rhi {
  template <typename F>
    requires(std::is_invocable_v<F, CommandBuffer&> && std::is_same_v<std::invoke_result_t<F, CommandBuffer&>, std::vector<Buffer>>)
  uint64_t RHI::oneshotCopy(const F& copyFunc) {
    ComPtr<ID3D12CommandAllocator> cmdAlloc;
    DX_REQUIRE(SUCCEEDED(m.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&cmdAlloc))),
               "Failed to create copy command allocator");

    m.copyCmdList->Reset(cmdAlloc.Get(), nullptr);

    CommandBuffer cmdBuf(std::move(cmdAlloc), m.copyCmdList);

    auto buffers = copyFunc(cmdBuf);

    cmdBuf.end();

    m.queues.copy->ExecuteCommandLists(1, cmdBuf);

    uint64_t fenceValue = m.copyFence.signal(m.queues.copy);

    std::vector<D3D12MA::Allocation*> allocsToDrop;
    allocsToDrop.reserve(buffers.size());
    for (auto& buffer : buffers) {
      allocsToDrop.push_back(buffer.dxTakeAllocation());
    }

    m.ongoingCopies.push_back({.cmdAlloc = cmdBuf.dxGetAlloc(), .fenceValue = fenceValue, .allocsToDrop = std::move(allocsToDrop)});

    return fenceValue;
  }
} // namespace kt::rhi