#include "renderer.hpp"
#include "backends/imgui_impl_dx12.h"
#include "d3dx12.h"
#include "dx-logger.hpp"
#include "keptech/components/transform.hpp"
#include "keptech/maths/frustum.hpp"
#include "keptech/render/graph/builder.hpp"
#include "keptech/render/imgui.hpp"
#include "wrappers/cmdBuf.hpp"
#include <d3d12.h>

namespace kt::rdr {
  Renderer Renderer::singleton{};
  bool Renderer::isInitialized = false;

  Renderer& Renderer::get() { return singleton; }
  bool Renderer::isInit() { return isInitialized; }

  void Renderer::newFrame() {
    auto allocator = m.graphicsCmdAlloc[m.frameIndex];

    allocator->Reset();

    m.graphicsCmdList->Reset(allocator.Get(), nullptr);

    ImGui_ImplDX12_NewFrame();
    rendering::newImGuiFrame();
  }

  maths::Frustum Renderer::startFrame() { return maths::Frustum{}; }

  void Renderer::debugUi() const {
    ImGui::Begin("Debug View");

    auto camera = Scene::active().getActiveCamera();
    auto& camT = camera.getComponents<components::Transform>();
    auto camPos = camT.getGlobal()[3];

    ImGui::Text("Camera Position: %.2f, %.2f, %.2f", static_cast<double>(camPos.x), static_cast<double>(camPos.y),
                static_cast<double>(camPos.z));

    ImGui::End();
  }

  void Renderer::endFrame(const CommandBuffer& cmd) {

    {
      CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
          m.backbuffers[m.imageIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
      cmd->ResourceBarrier(1, &barrier);
    }

#ifndef NDEBUG
    debugUi();
#endif

    ImGui::Render();

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m.rtvHeap->GetCPUDescriptorHandleForHeapStart(), m.imageIndex,
                                            static_cast<UINT>(RTV_DESCRIPTOR_SIZE));

    std::array<FLOAT, 4> clearColor = {.0f, .2f, .4f, 1.0f};
    cmd->ClearRenderTargetView(rtvHandle, clearColor.data(), 0, nullptr);
    cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    cmd->SetDescriptorHeaps(1, m.imGuiSrvHeap.GetAddressOf());

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m.backbuffers[m.imageIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    cmd->ResourceBarrier(1, &barrier);

    cmd->Close();

    std::array<ID3D12CommandList* const, 1> cmdLists = {cmd};
    m.graphicsQueue->ExecuteCommandLists(cmdLists.size(), cmdLists.data());

    present();
  }

  void Renderer::present() {
    UINT syncInterval = m.vSync ? 1 : 0;
    UINT presentFlags = m.vSync ? 0 : DXGI_PRESENT_ALLOW_TEARING;

    DX_DEBUG("Presenting swapchain image {} for frame {}.", m.imageIndex, m.frameIndex);
    auto presentRes = m.swapchain->Present(syncInterval, presentFlags);

    switch (presentRes) {
    case S_OK:
      break;
    case DXGI_ERROR_DEVICE_RESET:
      DX_ABORT("Device reset during present of swapchain image {} on frame index {}", m.imageIndex, m.frameIndex);
      break;
    case DXGI_ERROR_DEVICE_REMOVED:
      DX_ABORT("Device lost during present of swapchain image {} on frame index {}", m.imageIndex, m.frameIndex);
      break;
    case DXGI_STATUS_OCCLUDED:
      DX_DEBUG("Window occluded during present of swapchain image {} on frame index {}", m.imageIndex, m.frameIndex);
      break;
    default:
      DX_WARN("Unknown error during present of swapchain image {} on frame index {}: {}", m.imageIndex, m.frameIndex, presentRes);
      break;
    }

    m.fenceValues[m.frameIndex] = m.fence.signal(m.graphicsQueue);

    m.frameIndex = (m.frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    m.fence.wait(m.fenceValues[m.frameIndex], m.fenceEvent);

    m.imageIndex = static_cast<uint8_t>(m.swapchain->GetCurrentBackBufferIndex());
  }

  void Renderer::onResize() {
    glm::uvec2 renderSize = m.window->getRenderSize();
    DX_DEBUG("Window resized to {}x{}", renderSize.x, renderSize.y);
    m.fence.flush(m.graphicsQueue, m.fenceEvent);

    for (uint32_t i = 0; i < SWAPCHAIN_IMAGE_COUNT; ++i) {
      m.backbuffers[i].Reset();
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      m.fenceValues[i] = m.fenceValues[m.frameIndex];
    }

    DXGI_SWAP_CHAIN_DESC swapchainDesc{};
    DX_REQUIRE(SUCCEEDED(m.swapchain->GetDesc(&swapchainDesc)), "Failed to get swapchain description");
    DX_REQUIRE(SUCCEEDED(m.swapchain->ResizeBuffers(SWAPCHAIN_IMAGE_COUNT, renderSize.x, renderSize.y, swapchainDesc.BufferDesc.Format,
                                                    swapchainDesc.Flags)),
               "Failed to resize swapchain buffers");

    m.imageIndex = static_cast<uint8_t>(m.swapchain->GetCurrentBackBufferIndex());
    auto res = updateBackbufferDescriptors();
    if (!res) {
      DX_ABORT("Failed to update backbuffer descriptors: {}", res.error());
    }
  }

  Renderer::~Renderer() {
    m.fence.flush(m.graphicsQueue, m.fenceEvent);
    CloseHandle(m.fenceEvent);

    if (m.allocator) {
      m.allocator->Release();
      m.allocator = nullptr;
    }

    ImGui_ImplDX12_Shutdown();
    rendering::shutdownImGui();
  }

  void Renderer::setRenderGraphProps(RenderGraphBuilder& builder) const {
    DXGI_SWAP_CHAIN_DESC swapchainDesc{};
    DX_REQUIRE(SUCCEEDED(m.swapchain->GetDesc(&swapchainDesc)), "Failed to get swapchain description");
    builder.setSwapchainFormat(m.formats.swapchain);
    builder.setSwapchainSize({swapchainDesc.BufferDesc.Width, swapchainDesc.BufferDesc.Height});

    auto d = SDL_GetDisplayForWindow(m.window->getHandle());
    auto* dm = SDL_GetCurrentDisplayMode(d);
    builder.setRenderResolution({dm->w, dm->h});
  }

  void Renderer::addGeometryPass(RenderGraphBuilder& builder, bool clearColor) {
    auto& p = builder.addPass("kt::geometry");
    p.addColorOutput("kt::albedo", {
                                       .sizeType = AttachmentSize::SwapchainRelative,
                                   });
  }
} // namespace kt::rdr