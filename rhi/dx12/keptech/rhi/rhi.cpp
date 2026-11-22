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
#include "keptech/core/window.hpp"
#include "keptech/rhi/descriptorInfo.hpp"
#include "keptech/rhi/descriptorSet.hpp"
#include "keptech/rhi/imgui.hpp"
#include "pipelineBuilder.hpp"
#include "shaders/keptech/rhi/blit.h"
#include <codecvt>
#include <d3d12.h>
#include <iostream>
#ifndef NDEBUG
#include <dxgidebug.h>
#include <spdlog/fmt/bundled/xchar.h>
#endif
#include <utility>

namespace kt::rhi {

  RHI RhiBase::singleton{};
  bool RhiBase::isInitialized = false;

  RHI& RhiBase::get() { return singleton; }
  bool RhiBase::isInit() { return isInitialized; }
  uint64_t RhiBase::getTimelineValue(this const RHI& self) { return self.m.fence.currentValue(); }

  bool RhiBase::canRenderToFormat(this const RHI& self, ImageFormat format) {
    D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport{.Format = raw(format)};
    self.m.device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport));
    return (formatSupport.Support1 & (D3D12_FORMAT_SUPPORT1_RENDER_TARGET | D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL)) != 0;
  }

  bool RhiBase::canSampleFromFormat(this const RHI& self, ImageFormat format) {
    D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport{.Format = raw(format)};
    self.m.device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport));
    return (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) != 0;
  }
  ImageFormat RhiBase::getSwapchainFormat(this const RHI&) { return static_cast<ImageFormat>(ImageFormat::R8G8B8A8_UNORM); }

  void RhiBase::waitGraphicsIdle(this RHI& self) { self.m.fence.flush(self.m.queues.graphics); }
  void RhiBase::waitComputeIdle(this RHI& self) { self.m.fence.flush(self.m.queues.compute); }
  void RhiBase::waitCopyIdle(this RHI& self) { self.m.copyFence.flush(self.m.queues.copy); }
  void RhiBase::waitIdle(this RHI& self) {
    self.waitGraphicsIdle();
    self.waitComputeIdle();
    self.waitCopyIdle();
  }

  kt::Result<ImageRef, HRESULT, S_OK> RhiBase::createTexture(this RHI& self, const ImageCreateInfo& createInfo) {
    auto imageRes = Image::create(createInfo);
    if (imageRes.isError()) {
      return imageRes.error();
    }

    size_t index = self.m.cbvSrvUavHeap.count++;

    self.m.loadedTextures.push_back(std::move(imageRes.value()));

    auto& img = self.m.loadedTextures.back();

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle =
        CD3DX12_CPU_DESCRIPTOR_HANDLE(self.m.cbvSrvUavHeap.heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(index),
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

    self.m.device->CreateShaderResourceView(img.dxresource().Get(), &srvDesc, srvHandle);

    img.setTextureIndex(index);

    return static_cast<ImageRef>(img);
  }

  DescriptorLayout RhiBase::createDescriptorLayout(this RHI& self, std::span<const DescriptorInfo> descriptorInfos) {
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

  DescriptorSet RhiBase::allocateDescriptorSet(this RHI& self, const DescriptorLayout& layout) {
    auto cpuHandle =
        CD3DX12_CPU_DESCRIPTOR_HANDLE(self.m.cbvSrvUavHeap.heap->GetCPUDescriptorHandleForHeapStart(),
                                      static_cast<INT>(self.m.cbvSrvUavHeap.count), static_cast<uint32_t>(CBV_SRV_UAV_DESCRIPTOR_SIZE));
    auto gpuHandle =
        CD3DX12_GPU_DESCRIPTOR_HANDLE(self.m.cbvSrvUavHeap.heap->GetGPUDescriptorHandleForHeapStart(),
                                      static_cast<INT>(self.m.cbvSrvUavHeap.count), static_cast<uint32_t>(CBV_SRV_UAV_DESCRIPTOR_SIZE));

    uint32_t c = 0;
    for (const auto& range : layout.dxGetRanges()) {
      c += range.NumDescriptors;
    }
    self.m.cbvSrvUavHeap.count += c;

    // TODO: Reallocate heap.
    DX_ASSERT(self.m.cbvSrvUavHeap.count <= self.m.cbvSrvUavHeap.capacity,
              "Descriptor pool overflow: allocated {} descriptors, but capacity is {}", self.m.cbvSrvUavHeap.count,
              self.m.cbvSrvUavHeap.capacity);

    DX_DEBUG("Allocated {} descriptors from descriptor pool ({} used, {} remaining)", c, self.m.cbvSrvUavHeap.count,
             self.m.cbvSrvUavHeap.capacity - self.m.cbvSrvUavHeap.count);

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

  void RhiBase::newFrame(this RHI& self) {

    ImGui_ImplDX12_NewFrame();
    imgui::newFrame();

    self.resetStats();
  }

  void RhiBase::startFrame(this RHI& self) {
    auto& frame = self.m.frames[self.m.frameIndex];

    // Work gets queued up at some pretty random points occasionally (looking at you window resize), so just quickly wait for pre frame
    // submitted work before dropping things.
    self.m.fence.wait(self.m.fence.getValue());

    for (auto* alloc : frame.allocsToDrop) {
      alloc->Release();
    }
    frame.allocsToDrop.clear();

    uint64_t fenceValue = self.m.fence.currentValue();

    for (auto& copy : self.m.ongoingCopies) {
      if (copy.fenceValue <= fenceValue) {
        for (auto* alloc : copy.allocsToDrop) {
          alloc->Release();
        }
      }
    }
    self.m.ongoingCopies.erase(std::remove_if(self.m.ongoingCopies.begin(), self.m.ongoingCopies.end(),
                                              [fenceValue](const Members::OngoingCopy& copy) { return copy.fenceValue <= fenceValue; }),
                               self.m.ongoingCopies.end());

    self.m.runningAllocs[self.m.frameIndex].clear();
  }

  void RhiBase::debugUi(this const RHI& self) {}

  void RhiBase::endFrame(this RHI& self, CommandBuffer& cmd) {
#ifndef NDEBUG
    self.debugUi();
#endif

    ImGui::Render();

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(self.m.swapchain.rtvHeap->GetCPUDescriptorHandleForHeapStart(), self.m.imageIndex,
                                            static_cast<UINT>(RTV_DESCRIPTOR_SIZE));

    cmd->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    cmd->SetDescriptorHeaps(1, self.m.imGui.srvHeap.GetAddressOf());

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        self.m.swapchain.backbuffers[self.m.imageIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    cmd->ResourceBarrier(1, &barrier);

    cmd->Close();

    self.m.queues.graphics->Wait(self.m.fence, self.m.fence.getValue());

    std::array<ID3D12CommandList* const, 1> cmdLists = {cmd};
    self.m.queues.graphics->ExecuteCommandLists(cmdLists.size(), cmdLists.data());

    self.m.runningAllocs[self.m.frameIndex].push_back(std::move(cmd.dxGetAlloc()));

    self.present();
  }

  void RhiBase::present(this RHI& self) {
    UINT syncInterval = self.m.swapchain.vSync ? 1 : 0;
    UINT presentFlags = self.m.swapchain.vSync ? 0 : DXGI_PRESENT_ALLOW_TEARING;

    auto presentRes = self.m.swapchain.swapchain->Present(syncInterval, presentFlags);

    switch (presentRes) {
    case S_OK:
      break;
    case DXGI_ERROR_DEVICE_RESET:
      DX_ABORT("Device reset during present of swapchain image {} on frame index {}", self.m.imageIndex, self.m.frameIndex);
      break;
    case DXGI_ERROR_DEVICE_REMOVED:
      DX_ABORT("Device lost during present of swapchain image {} on frame index {}", self.m.imageIndex, self.m.frameIndex);
      break;
    case DXGI_STATUS_OCCLUDED:
      DX_DEBUG("Window occluded during present of swapchain image {} on frame index {}", self.m.imageIndex, self.m.frameIndex);
      break;
    default:
      DX_WARN("Unknown error during present of swapchain image {} on frame index {}: {}", self.m.imageIndex, self.m.frameIndex, presentRes);
      break;
    }

    self.m.frames[self.m.frameIndex].fenceValue = self.m.fence.signal(self.m.queues.graphics);

    self.m.frameIndex = (self.m.frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    self.m.fence.wait(self.m.frames[self.m.frameIndex].fenceValue);

    self.m.imageIndex = static_cast<uint8_t>(self.m.swapchain.swapchain->GetCurrentBackBufferIndex());
  }

  void RhiBase::onResize(this RHI& self) {
    glm::uvec2 renderSize = self.m.window->getRenderSize();
    DX_DEBUG("Window resized to {}x{}", renderSize.x, renderSize.y);
    self.m.fence.flush(self.m.queues.graphics);

    for (uint32_t i = 0; i < SWAPCHAIN_IMAGE_COUNT; ++i) {
      self.m.swapchain.backbuffers[i].Reset();
    }

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      self.m.frames[i].fenceValue = self.m.frames[self.m.frameIndex].fenceValue;
    }

    DXGI_SWAP_CHAIN_DESC swapchainDesc{};
    DX_REQUIRE(SUCCEEDED(self.m.swapchain.swapchain->GetDesc(&swapchainDesc)), "Failed to get swapchain description");
    DX_REQUIRE(SUCCEEDED(self.m.swapchain.swapchain->ResizeBuffers(SWAPCHAIN_IMAGE_COUNT, renderSize.x, renderSize.y,
                                                                   swapchainDesc.BufferDesc.Format, swapchainDesc.Flags)),
               "Failed to resize swapchain buffers");

    self.m.swapchain.size = renderSize;

    self.m.imageIndex = static_cast<uint8_t>(self.m.swapchain.swapchain->GetCurrentBackBufferIndex());
    auto res = self.updateBackbufferDescriptors();
    if (!res) {
      DX_ABORT("Failed to update backbuffer descriptors: {}", res.error());
    }
  }

  void Swapchain::destroy() {
    for (auto& backbuffer : backbuffers) {
      backbuffer.Reset();
    }
    rtvHeap.Reset();
    swapchain.Reset();
  }

  void RhiBase::shutdown(this RHI& self) {
    auto& m = self.m;
    if (m.allocator) {
      self.waitIdle();

      m.ongoingCopies.clear();

      for (auto& frame : m.frames) {
        for (auto* alloc : frame.allocsToDrop) {
          alloc->Release();
        }
      }

      for (auto& allocs : m.runningAllocs) {
        for (auto& alloc : allocs) {
          alloc.Reset();
        }
      }

      for (auto& tex : m.loadedTextures) {
        tex.destroy();
      }

      ImGui_ImplDX12_Shutdown();
      imgui::shutdown();

      m.integrateBRDFPipeline.destroy();
      m.convoluteIrradiancePipeline.destroy();
      m.cubeFromEquirectangularPipeline.destroy();
      m.preFilterPipeline.destroy();
      m.blitPipelines.clear();

      m.imGui.srvHeap.Reset();
      m.dsvHeap.heap.Reset();
      m.rtvHeap.heap.Reset();
      m.cbvSrvUavHeap.heap.Reset();
      m.samplerHeap.heap.Reset();

      m.drawIndexedIndirectSignature.Reset();
      m.drawIndirectSignature.Reset();

      m.copyFence.destroy();
      m.fence.destroy();

      m.copyCmdList.Reset();

      m.queues.graphics.Reset();
      m.queues.compute.Reset();
      m.queues.copy.Reset();

      m.swapchain.destroy();

      D3D12MA::TotalStatistics stats{};
      m.allocator->CalculateStatistics(&stats);
      if (stats.Total.Stats.AllocationCount > 0) {
        WCHAR* statsString = nullptr;
        m.allocator->BuildStatsString(&statsString, TRUE);

        std::wstring ws(statsString);
        std::string statsStr(ws.begin(), ws.end());

        std::cerr << "D3D12 MEMORY LEAK:\n" << statsStr << std::endl;

        m.allocator->FreeStatsString(statsString);
      }

      m.allocator->Release();
      m.allocator = nullptr;

      m.device.Reset();

#ifndef NDEBUG
      Microsoft::WRL::ComPtr<IDXGIDebug1> dxgiDebug;
      if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug)))) {
        dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
      }
#endif
    }
  }

  ImageRef RhiBase::getSwapchainImage(this const RHI& self) {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(self.m.swapchain.rtvHeap->GetCPUDescriptorHandleForHeapStart(), self.m.imageIndex,
                                            static_cast<UINT>(RTV_DESCRIPTOR_SIZE));
    return {"Swapchain Image",
            self.m.swapchain.backbuffers[self.m.imageIndex].Get(),
            ImageDim::e2D,
            ImageFormat::R8G8B8A8_UNORM,
            {self.m.swapchain.size, 1},
            1,
            1,
            rtvHandle};
  }
  glm::uvec2 RhiBase::getSwapchainSize(this const RHI& self) { return self.m.swapchain.size; }

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

    image.dxSetRtvDsvIndex(m.rtvHeap.count);
    m.rtvHeap.count += image.mips();

    dxUpdateRenderTargetImage(image);
  }

  void RHI::dxRegisterSampledImage(rhi::Image& img) {
    if (img.getTextureIndex() != UINT64_MAX) {
      DX_WARN("Image {} is already registered as a sampled image", img.getName());
      return;
    }

    img.setTextureIndex(m.cbvSrvUavHeap.count++);

    dxUpdateSampledImage(img);
  }

  Pipeline& RHI::dxGetBlitPipeline(ImageFormat format) {
    auto it = m.blitPipelines.find(format);
    if (it != m.blitPipelines.end()) {
      return it->second;
    }

    PipelineBuilder pipelineBuilder{};
    pipelineBuilder.setShader(::shaders::kt::blit).addColorAttachment(format).setName(fmt::format("{} Blit", format));
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

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{
        .Format = raw(image.format()),
    };

    for (auto mip = 0u; mip < image.mips(); ++mip) {
      switch (image.dim()) {
      case ImageDim::e1D:
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
        rtvDesc.Texture1D.MipSlice = mip;
        break;
      case ImageDim::e2D:
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = mip;
        rtvDesc.Texture2D.PlaneSlice = 0;
        break;
      case ImageDim::e3D:
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
        rtvDesc.Texture3D.MipSlice = mip;
        rtvDesc.Texture3D.FirstWSlice = 0;
        rtvDesc.Texture3D.WSize = image.getExtent().z;
        break;
      case ImageDim::eCube:
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice = mip;
        rtvDesc.Texture2DArray.FirstArraySlice = 0;
        rtvDesc.Texture2DArray.ArraySize = 6;
        rtvDesc.Texture2DArray.PlaneSlice = 0;
        break;
      }

      m.device->CreateRenderTargetView(image.dxresource().Get(), &rtvDesc, rtvHandle);
      rtvHandle.Offset(1, static_cast<UINT>(RTV_DESCRIPTOR_SIZE));
    }
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

  void RHI::dxUpdateSampledImage(rhi::Image& img) {
    DX_ASSERT(img.getUsage().has(kt::rhi::ImageUsage::Sampled), "Image {} is not a sampled img", img.getName());
    DX_ASSERT(img.getTextureIndex() != UINT64_MAX, "Image {} is not registered as a sampled img", img.getName());

    auto index = img.getTextureIndex();

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle =
        CD3DX12_CPU_DESCRIPTOR_HANDLE(m.cbvSrvUavHeap.heap->GetCPUDescriptorHandleForHeapStart(), static_cast<INT>(index),
                                      static_cast<UINT>(CBV_SRV_UAV_DESCRIPTOR_SIZE));

    auto format = raw(img.format());
    switch (format) {
    case DXGI_FORMAT_D32_FLOAT:
      format = DXGI_FORMAT_R32_FLOAT;
      break;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
      format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
      break;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
      format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
      break;
    case DXGI_FORMAT_D16_UNORM:
      format = DXGI_FORMAT_R16_UNORM;
      break;
    default:
      break;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
        .Format = format,
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
  }

  void RhiBase::submitGraphicsCmd(this RHI& self, CommandBuffer& cmd, uint64_t waitFor, uint64_t signalTo, uint64_t waitForCopy) {
    if (waitForCopy > 0) {
      self.m.queues.graphics->Wait(self.m.copyFence, waitForCopy);
    }
    if (waitFor > 0) {
      self.m.queues.graphics->Wait(self.m.fence, waitFor);
    }

    std::array<ID3D12CommandList* const, 1> cmdLists = {cmd};
    self.m.queues.graphics->ExecuteCommandLists(cmdLists.size(), cmdLists.data());

    if (signalTo > 0) {
      self.m.fence.signal(self.m.queues.graphics, signalTo);
    }

    self.m.runningAllocs[self.m.frameIndex].push_back(std::move(cmd.dxGetAlloc()));
  }

  void RhiBase::submitComputeCmd(this RHI& self, CommandBuffer& cmd, uint64_t waitFor, uint64_t signalTo, uint64_t waitForCopy) {
    if (waitForCopy > 0) {
      self.m.queues.compute->Wait(self.m.copyFence, waitForCopy);
    }
    if (waitFor > 0) {
      self.m.queues.compute->Wait(self.m.fence, waitFor);
    }

    std::array<ID3D12CommandList* const, 1> cmdLists = {cmd};
    self.m.queues.compute->ExecuteCommandLists(cmdLists.size(), cmdLists.data());

    if (signalTo > 0) {
      self.m.fence.signal(self.m.queues.compute, signalTo);
    }

    self.m.runningAllocs[self.m.frameIndex].push_back(std::move(cmd.dxGetAlloc()));
  }

  Vec<CommandBuffer> RhiBase::allocateGraphicsCommandBuffers(this RHI& self, uint32_t count) {
    Vec<CommandBuffer> cmdBuffers;
    cmdBuffers.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
      ComPtr<ID3D12CommandAllocator> cmdAlloc;
      DX_REQUIRE(SUCCEEDED(self.m.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc))),
                 "Failed to create graphics command allocator");

      ComPtr<ID3D12GraphicsCommandList> cmdList;
      DX_REQUIRE(
          SUCCEEDED(self.m.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&cmdList))),
          "Failed to create graphics command list");
      ComPtr<ID3D12GraphicsCommandList10> cmdList10;
      DX_REQUIRE(SUCCEEDED(cmdList.As(&cmdList10)), "Failed to query ID3D12GraphicsCommandList10 interface");
      cmdBuffers.emplace_back(std::move(cmdAlloc), std::move(cmdList10));
    }
    return cmdBuffers;
  }

  Vec<CommandBuffer> RhiBase::allocateComputeCommandBuffers(this RHI& self, uint32_t count) {
    Vec<CommandBuffer> cmdBuffers;
    cmdBuffers.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
      ComPtr<ID3D12CommandAllocator> cmdAlloc;
      DX_REQUIRE(SUCCEEDED(self.m.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&cmdAlloc))),
                 "Failed to create compute command allocator");

      ComPtr<ID3D12GraphicsCommandList> cmdList;
      DX_REQUIRE(
          SUCCEEDED(self.m.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&cmdList))),
          "Failed to create compute command list");
      ComPtr<ID3D12GraphicsCommandList10> cmdList10;
      DX_REQUIRE(SUCCEEDED(cmdList.As(&cmdList10)), "Failed to query ID3D12GraphicsCommandList10 interface");
      cmdBuffers.emplace_back(std::move(cmdAlloc), std::move(cmdList10));
    }
    return cmdBuffers;
  }

  void RhiBase::submitBufferToDrop(this RHI& self, Buffer& buffer) {
    auto alloc = buffer.dxTakeAllocation();
    if (alloc) {
      self.m.frames[self.m.frameIndex].allocsToDrop.emplace_back(alloc);
    }
  }

  void RhiBase::submitImageToDrop(this RHI& self, Image& image) {
    auto alloc = image.dxTakeAllocation();
    if (alloc) {
      self.m.frames[self.m.frameIndex].allocsToDrop.emplace_back(alloc);
    }
  }
} // namespace kt::rhi