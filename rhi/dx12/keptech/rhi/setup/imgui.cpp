#include "macros.hpp"
#include "rhi.hpp"

#include "keptech/core/imgui.hpp"
#include "keptech/core/window.hpp"
#include <imgui/backends/imgui_impl_dx12.h>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/imgui.h>

namespace kt::rhi {
  std::expected<void, std::string> RHI::initImGui() {
    rendering::initImGui();

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {
        .Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        .NumDescriptors = 1000,
        .Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        .NodeMask = 0,
    };
    DX_MAKE(m.device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m.imGui.srvHeap)), "Failed to create ImGui SRV descriptor heap");

    m.imGui.descriptorAlloc.Create(m.device.Get(), m.imGui.srvHeap.Get());

    ImGui_ImplSDL3_InitForD3D(m.window->getHandle());

    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device = m.device.Get();
    initInfo.NumFramesInFlight = SWAPCHAIN_IMAGE_COUNT;
    initInfo.CommandQueue = m.queues.graphics.Get();
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = m.imGui.srvHeap.Get();

    initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle,
                                       D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) {
      return RHI::get().dxGetMembers().imGui.descriptorAlloc.Alloc(out_cpu_handle, out_gpu_handle);
    };
    initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle,
                                      D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) {
      return RHI::get().dxGetMembers().imGui.descriptorAlloc.Free(cpu_handle, gpu_handle);
    };

    ImGui_ImplDX12_Init(&initInfo);

    return {};
  }
} // namespace kt::rhi