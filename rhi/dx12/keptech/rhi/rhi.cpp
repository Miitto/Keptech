#include "rhi.hpp"
#include "backends/imgui_impl_dx12.h"
#include "buffer.hpp"
#include "cmdBuf.hpp"
#include "d3dx12.h"
#include "descriptorLayout.hpp"
#include "dx/dx-logger.hpp"
#include "image.hpp"
#include "imageRef.hpp"
#include "imgui.h"
#include "keptech/components/transform.hpp"
#include "keptech/core/scene.hpp"
#include "keptech/core/window.hpp"
#include "keptech/rhi/descriptorInfo.hpp"
#include "keptech/rhi/descriptorSet.hpp"
#include "keptech/rhi/imgui.hpp"
#include "pipelineBuilder.hpp"
#include "shaders/keptech/rhi/blit.h"
#include <d3d12.h>
#include <utility>

namespace kt::rhi {

  RHI RHI::singleton{};
  bool RHI::isInitialized = false;

  RHI& RHI::get() { return singleton; }
  bool RHI::isInit() { return isInitialized; }
  uint64_t RHI::getTimelineValue() const { return m.fence.currentValue(); }

  bool RHI::canRenderToFormat(ImageFormat format) const {
    D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport{.Format = raw(format)};
    m.device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport));
    return (formatSupport.Support1 & (D3D12_FORMAT_SUPPORT1_RENDER_TARGET | D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)) != 0;
  }

  bool RHI::canSampleFromFormat(ImageFormat format) const {
    D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport{.Format = raw(format)};
    m.device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport));
    return (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) != 0;
  }
  ImageFormat RHI::getSwapchainFormat() const { return static_cast<ImageFormat>(ImageFormat::R8G8B8A8_UNORM); }

  void RHI::waitGraphicsIdle() { m.fence.flush(m.queues.graphics); }
  void RHI::waitComputeIdle() { m.fence.flush(m.queues.compute); }
  void RHI::waitCopyIdle() { m.copyFence.flush(m.queues.copy); }
  void RHI::waitIdle() {
    waitGraphicsIdle();
    waitComputeIdle();
    waitCopyIdle();
  }

  kt::Result<ImageRef, HRESULT, S_OK> RHI::createTexture(const ImageCreateInfo& createInfo) {
    auto imageRes = Image::create(createInfo);
    if (imageRes.isError()) {
      return imageRes.error();
    }

    size_t index = m.loadedTextures.size();

    m.loadedTextures.push_back(std::move(imageRes.value()));

    auto& img = m.loadedTextures.back();

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle =
        CD3DX12_CPU_DESCRIPTOR_HANDLE(m.cbvSrvUavHeap.heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(index),
                                      static_cast<UINT>(CBV_SRV_UAV_DESCRIPTOR_SIZE));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
        .Format = raw(img.format()),
        .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
    };

    switch (img.dim()) {
    case ImageDim::e1D:
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
      srvDesc.Texture1D.MipLevels = img.mips();
      break;
    case ImageDim::e2D:
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvDesc.Texture2D.MipLevels = img.mips();
      break;
    case ImageDim::e3D:
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
      srvDesc.Texture3D.MipLevels = img.mips();
      break;
    case ImageDim::eCube:
      srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
      srvDesc.TextureCube.MipLevels = img.mips();
      break;
    }

    m.device->CreateShaderResourceView(img.dxresource().Get(), &srvDesc, srvHandle);

    img.setTextureIndex(index);

    return static_cast<ImageRef>(img);
  }

  DescriptorLayout RHI::createDescriptorLayout(std::span<const DescriptorInfo> descriptorInfos) {
    DescriptorLayout layout{};
    auto& ranges = layout.dxGetRanges();
    ranges.reserve(descriptorInfos.size());

    uint32_t descriptorIndex = 0;
    for (const auto& info : descriptorInfos) {
      ranges.emplace_back(raw(info.type), info.count, info.binding, 0, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, descriptorIndex);
      descriptorIndex += info.count;
    }

    return layout;
  }

  DescriptorSet RHI::allocateDescriptorSet(const DescriptorLayout& layout) {
    auto cpuHandle =
        CD3DX12_CPU_DESCRIPTOR_HANDLE(m.cbvSrvUavHeap.heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(m.cbvSrvUavHeap.count),
                                      static_cast<uint32_t>(CBV_SRV_UAV_DESCRIPTOR_SIZE));
    auto gpuHandle =
        CD3DX12_GPU_DESCRIPTOR_HANDLE(m.cbvSrvUavHeap.heap->GetGPUDescriptorHandleForHeapStart(), static_cast<INT>(m.cbvSrvUavHeap.count),
                                      static_cast<uint32_t>(CBV_SRV_UAV_DESCRIPTOR_SIZE));

    uint32_t c = 0;
    for (const auto& range : layout.dxGetRanges()) {
      c += range.NumDescriptors;
    }
    m.cbvSrvUavHeap.count += c;

    // TODO: Reallocate heap.
    DX_ASSERT(m.cbvSrvUavHeap.count <= m.cbvSrvUavHeap.capacity, "Descriptor pool overflow: allocated {} descriptors, but capacity is {}",
              m.cbvSrvUavHeap.count, m.cbvSrvUavHeap.capacity);

    DX_DEBUG("Allocated {} descriptors from descriptor pool ({} used, {} remaining)", c, m.cbvSrvUavHeap.count,
             m.cbvSrvUavHeap.capacity - m.cbvSrvUavHeap.count);

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

  void RHI::newFrame() {

    ImGui_ImplDX12_NewFrame();
    imgui::newFrame();

    resetStats();
  }

  void RHI::startFrame() {
    auto& frame = m.frames[m.frameIndex];

    // Work gets queued up at some pretty random points occasionally (looking at you window resize), so just quickly wait for pre frame
    // submitted work before dropping things.
    m.fence.wait(m.fence.getValue());

    for (auto* alloc : frame.allocsToDrop) {
      alloc->Release();
    }
    frame.allocsToDrop.clear();

    uint64_t fenceValue = m.fence.currentValue();

    for (auto& copy : m.ongoingCopies) {
      if (copy.fenceValue <= fenceValue) {
        for (auto* alloc : copy.allocsToDrop) {
          alloc->Release();
        }
      }
    }
    m.ongoingCopies.erase(std::remove_if(m.ongoingCopies.begin(), m.ongoingCopies.end(),
                                         [fenceValue](const Members::OngoingCopy& copy) { return copy.fenceValue <= fenceValue; }),
                          m.ongoingCopies.end());

    m.runningAllocs[m.frameIndex].clear();
  }

  void RHI::debugUi() const {}

  void RHI::endFrame(CommandBuffer& cmd) {
#ifndef NDEBUG
    debugUi();
#endif

    ImGui::Render();

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m.swapchain.rtvHeap->GetCPUDescriptorHandleForHeapStart(), m.imageIndex,
                                            static_cast<UINT>(RTV_DESCRIPTOR_SIZE));

    cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    cmd->SetDescriptorHeaps(1, m.imGui.srvHeap.GetAddressOf());

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        m.swapchain.backbuffers[m.imageIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    cmd->ResourceBarrier(1, &barrier);

    cmd->Close();

    m.queues.graphics->Wait(m.fence, m.fence.getValue());

    std::array<ID3D12CommandList* const, 1> cmdLists = {cmd};
    m.queues.graphics->ExecuteCommandLists(cmdLists.size(), cmdLists.data());

    m.runningAllocs[m.frameIndex].push_back(std::move(cmd.dxGetAlloc()));

    present();
  }

  void RHI::present() {
    UINT syncInterval = m.swapchain.vSync ? 1 : 0;
    UINT presentFlags = m.swapchain.vSync ? 0 : DXGI_PRESENT_ALLOW_TEARING;

    auto presentRes = m.swapchain.swapchain->Present(syncInterval, presentFlags);

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

    m.frames[m.frameIndex].fenceValue = m.fence.signal(m.queues.graphics);

    m.frameIndex = (m.frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    m.fence.wait(m.frames[m.frameIndex].fenceValue);

    m.imageIndex = static_cast<uint8_t>(m.swapchain.swapchain->GetCurrentBackBufferIndex());
  }

  void RHI::onResize() {
    glm::uvec2 renderSize = m.window->getRenderSize();
    DX_DEBUG("Window resized to {}x{}", renderSize.x, renderSize.y);
    m.fence.flush(m.queues.graphics);

    for (uint32_t i = 0; i < SWAPCHAIN_IMAGE_COUNT; ++i) {
      m.swapchain.backbuffers[i].Reset();
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      m.frames[i].fenceValue = m.frames[m.frameIndex].fenceValue;
    }

    DXGI_SWAP_CHAIN_DESC swapchainDesc{};
    DX_REQUIRE(SUCCEEDED(m.swapchain.swapchain->GetDesc(&swapchainDesc)), "Failed to get swapchain description");
    DX_REQUIRE(SUCCEEDED(m.swapchain.swapchain->ResizeBuffers(SWAPCHAIN_IMAGE_COUNT, renderSize.x, renderSize.y,
                                                              swapchainDesc.BufferDesc.Format, swapchainDesc.Flags)),
               "Failed to resize swapchain buffers");

    m.swapchain.size = renderSize;

    m.imageIndex = static_cast<uint8_t>(m.swapchain.swapchain->GetCurrentBackBufferIndex());
    auto res = updateBackbufferDescriptors();
    if (!res) {
      DX_ABORT("Failed to update backbuffer descriptors: {}", res.error());
    }
  }

  RHI::~RHI() {
    m.fence.flush(m.queues.graphics);

    for (auto& frame : m.frames) {
      for (auto* alloc : frame.allocsToDrop) {
        alloc->Release();
      }
    }

    if (m.allocator) {
      m.allocator->Release();
      m.allocator = nullptr;
    }

    ImGui_ImplDX12_Shutdown();
    imgui::shutdown();
  }

  ImageRef RHI::getSwapchainImage() const {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m.swapchain.rtvHeap->GetCPUDescriptorHandleForHeapStart(), m.imageIndex,
                                            static_cast<UINT>(RTV_DESCRIPTOR_SIZE));
    return {"Swapchain Image", m.swapchain.backbuffers[m.imageIndex].Get(), ImageDim::e2D, ImageFormat::R8G8B8A8_UNORM, 1, 1, rtvHandle};
  }
  glm::uvec2 RHI::getSwapchainSize() const { return m.swapchain.size; }

  CD3DX12_CPU_DESCRIPTOR_HANDLE RHI::dxGetRtvHandle(uint16_t index) const {
    if (index >= m.rtvHeap.count) {
      DX_ABORT("Invalid RTV index {}. Max count is {}", index, m.rtvHeap.count);
    }
    return {m.rtvHeap.heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(index), static_cast<UINT>(RTV_DESCRIPTOR_SIZE)};
  }

  CD3DX12_CPU_DESCRIPTOR_HANDLE RHI::dxGetDsvHandle(uint16_t index) const {
    if (index >= m.dsvHeap.count) {
      DX_ABORT("Invalid DSV index {}. Max count is {}", index, m.dsvHeap.count);
    }
    return {m.dsvHeap.heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(index), static_cast<UINT>(DSV_DESCRIPTOR_SIZE)};
  }

  void RHI::dxRegisterRenderTargetImage(rhi::Image& image) {
    if (image.dxGetRtvDsvIndex() != 65535) {
      DX_WARN("Image {} is already registered as a render target", image.getName());
      return;
    }

    image.dxSetRtvDsvIndex(m.rtvHeap.count++);

    dxUpdateRenderTargetImage(image);
  }

  Pipeline& RHI::getBlitPipeline(ImageFormat format) {
    auto it = m.blitPipelines.find(format);
    if (it != m.blitPipelines.end()) {
      return it->second;
    }

    PipelineBuilder pipelineBuilder{};
    pipelineBuilder.setShader(::shaders::kt::blit).addColorAttachment(format);
    auto pipelineRes = pipelineBuilder.build();
    if (!pipelineRes) {
      DX_ABORT("Failed to create blit pipeline for format {}: {}", format, pipelineRes.error());
    }

    m.blitPipelines[format] = std::move(pipelineRes.value());
    return m.blitPipelines[format];
  }

  void RHI::dxRegisterDepthStencilImage(rhi::Image& image) {
    if (image.dxGetRtvDsvIndex() != 65535) {
      DX_WARN("Image {} is already registered as a depth stencil", image.getName());
      return;
    }

    image.dxSetRtvDsvIndex(m.dsvHeap.count++);
    dxUpdateDepthStencilImage(image);
  }

  void RHI::dxUpdateRenderTargetImage(rhi::Image& image) {
    DX_ASSERT(image.dxGetRtvDsvIndex() != 65535, "Image {} is not registered as a render target", image.getName());
    DX_ASSERT(image.getUsage().has(kt::rhi::ImageUsage::RenderTarget), "Image {} is not a render target", image.getName());

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m.rtvHeap.heap->GetCPUDescriptorHandleForHeapStart(),
                                            static_cast<INT>(image.dxGetRtvDsvIndex()), static_cast<UINT>(RTV_DESCRIPTOR_SIZE));

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = raw(image.format());
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;

    m.device->CreateRenderTargetView(image.dxresource().Get(), &rtvDesc, rtvHandle);
  }

  void RHI::dxUpdateDepthStencilImage(rhi::Image& image) {
    DX_ASSERT(image.getUsage().has(kt::rhi::ImageUsage::DepthStencil), "Image {} is not a depth stencil", image.getName());
    DX_ASSERT(image.dxGetRtvDsvIndex() != 65535, "Image {} is not registered as a depth stencil", image.getName());

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m.dsvHeap.heap->GetCPUDescriptorHandleForHeapStart(),
                                            static_cast<INT>(image.dxGetRtvDsvIndex()), static_cast<UINT>(DSV_DESCRIPTOR_SIZE));

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = raw(image.format());
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.Texture2D.MipSlice = 0;

    m.device->CreateDepthStencilView(image.dxresource().Get(), &dsvDesc, dsvHandle);
  }

  void RHI::submitGraphicsCmd(CommandBuffer& cmd, uint64_t waitFor, uint64_t signalTo, uint64_t waitForCopy) {
    if (waitForCopy > 0) {
      m.queues.graphics->Wait(m.copyFence, waitForCopy);
    }
    if (waitFor > 0) {
      m.queues.graphics->Wait(m.fence, waitFor);
    }

    std::array<ID3D12CommandList* const, 1> cmdLists = {cmd};
    m.queues.graphics->ExecuteCommandLists(cmdLists.size(), cmdLists.data());

    if (signalTo > 0) {
      m.fence.signal(m.queues.graphics, signalTo);
    }

    m.runningAllocs[m.frameIndex].push_back(std::move(cmd.dxGetAlloc()));
  }

  void RHI::submitComputeCmd(CommandBuffer& cmd, uint64_t waitFor, uint64_t signalTo, uint64_t waitForCopy) {
    if (waitForCopy > 0) {
      m.queues.compute->Wait(m.copyFence, waitForCopy);
    }
    if (waitFor > 0) {
      m.queues.compute->Wait(m.fence, waitFor);
    }

    std::array<ID3D12CommandList* const, 1> cmdLists = {cmd};
    m.queues.compute->ExecuteCommandLists(cmdLists.size(), cmdLists.data());

    if (signalTo > 0) {
      m.fence.signal(m.queues.compute, signalTo);
    }

    m.runningAllocs[m.frameIndex].push_back(std::move(cmd.dxGetAlloc()));
  }

  std::vector<CommandBuffer> RHI::allocateGraphicsCommandBuffers(uint32_t count) {
    std::vector<CommandBuffer> cmdBuffers;
    cmdBuffers.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
      ComPtr<ID3D12CommandAllocator> cmdAlloc;
      DX_REQUIRE(SUCCEEDED(m.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc))),
                 "Failed to create graphics command allocator");

      ComPtr<ID3D12GraphicsCommandList> cmdList;
      DX_REQUIRE(SUCCEEDED(m.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&cmdList))),
                 "Failed to create graphics command list");
      ComPtr<ID3D12GraphicsCommandList4> cmdList4;
      DX_REQUIRE(SUCCEEDED(cmdList.As(&cmdList4)), "Failed to query ID3D12GraphicsCommandList4 interface");
      cmdBuffers.emplace_back(std::move(cmdAlloc), std::move(cmdList4));
    }
    return cmdBuffers;
  }

  std::vector<CommandBuffer> RHI::allocateComputeCommandBuffers(uint32_t count) {
    std::vector<CommandBuffer> cmdBuffers;
    cmdBuffers.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
      ComPtr<ID3D12CommandAllocator> cmdAlloc;
      DX_REQUIRE(SUCCEEDED(m.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&cmdAlloc))),
                 "Failed to create compute command allocator");

      ComPtr<ID3D12GraphicsCommandList> cmdList;
      DX_REQUIRE(
          SUCCEEDED(m.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&cmdList))),
          "Failed to create compute command list");
      ComPtr<ID3D12GraphicsCommandList4> cmdList4;
      DX_REQUIRE(SUCCEEDED(cmdList.As(&cmdList4)), "Failed to query ID3D12GraphicsCommandList4 interface");
      cmdBuffers.emplace_back(std::move(cmdAlloc), std::move(cmdList4));
    }
    return cmdBuffers;
  }

  void RHI::submitBufferToDrop(Buffer& buffer) {
    auto alloc = buffer.dxTakeAllocation();
    if (alloc) {
      m.frames[m.frameIndex].allocsToDrop.emplace_back(alloc);
    }
  }

  void RHI::submitImageToDrop(Image& image) {
    auto alloc = image.dxTakeAllocation();
    if (alloc) {
      m.frames[m.frameIndex].allocsToDrop.emplace_back(alloc);
    }
  }
} // namespace kt::rhi