#pragma once

#include "d3dx12.h"
#include "keptech/rhi/bufferRef.hpp"
#include "keptech/rhi/descriptorTypes.hpp"
#include "keptech/rhi/imageRef.hpp"
#include <d3d12.h>
#include <span>

namespace kt::rhi {
  class DescriptorLayout;
  using RawDescriptorSet = D3D12_GPU_DESCRIPTOR_HANDLE;

  enum class DescriptorWriteBufferType : uint8_t {
    Uniform,
    Storage,
    RWStorage,
  };

  enum class DescriptorWriteImageType : uint8_t {
    Sampled,
    Storage,
  };

  struct DescriptorWriteInfo {
    uint32_t binding;
    uint32_t arrayIndex;
    DescriptorType type;
    union {
      BufferRef buffer;
      ImageRef image;
    };
    size_t offset = 0;
    size_t range = 0;
    size_t stride = 0;
  };

  class DescriptorSet {
  public:
    DescriptorSet() = default;
    DescriptorSet(CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle, CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle
#ifndef NDEBUG
                  ,
                  uint32_t numDescriptors
#endif
                  )
        : cpuHandle(cpuHandle), gpuHandle(gpuHandle)
#ifndef NDEBUG
          ,
          numDescriptors(numDescriptors)
#endif
    {
    }

    void write(const DescriptorLayout& layout, uint32_t binding, uint32_t arrayIndex, DescriptorWriteBufferType bufferType,
               BufferRef buffer, size_t offset, size_t range, size_t stride);
    void write(const DescriptorLayout& layout, uint32_t binding, uint32_t arrayIndex, DescriptorWriteImageType imageType, ImageRef image);
    void write(const DescriptorLayout& layout, std::span<const DescriptorWriteInfo> writeInfos);

    [[nodiscard]] CD3DX12_CPU_DESCRIPTOR_HANDLE dxGetCpuHandle() const { return cpuHandle; }
    [[nodiscard]] CD3DX12_GPU_DESCRIPTOR_HANDLE dxGetGpuHandle() const { return gpuHandle; }

  private:
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
    CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle{};

#ifndef NDEBUG
    uint32_t numDescriptors = 0;
#endif
  };
} // namespace kt::rhi