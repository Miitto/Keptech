#include "descriptorSet.hpp"
#include "dx/descriptorLayout.hpp"
#include "rhi.hpp"

namespace kt::rhi {
  void DescriptorSet::write(const DescriptorLayout& layout, uint32_t binding, uint32_t arrayIndex, DescriptorWriteBufferType bufferType,
                            BufferRef buffer, size_t offset, size_t r, size_t stride) {
    const auto& ranges = layout.dxGetRanges();
    auto device = RHI::get().dxGetDevice();

    uint32_t descriptorIndex = 0;
    for (const auto& range : ranges) {
      if (range.BaseShaderRegister == binding) {
        if (bufferType == DescriptorWriteBufferType::Uniform) {
          auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(this->cpuHandle, descriptorIndex + arrayIndex, CBV_SRV_UAV_DESCRIPTOR_SIZE);
          D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{
              .BufferLocation = buffer.dxGetResource()->GetGPUVirtualAddress() + offset,
              .SizeInBytes = static_cast<UINT>(r),
          };
          device->CreateConstantBufferView(&cbvDesc, cpuHandle);
        } else if (bufferType == DescriptorWriteBufferType::Storage) {
          auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(this->cpuHandle, descriptorIndex + arrayIndex, CBV_SRV_UAV_DESCRIPTOR_SIZE);
          D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
              .Format = DXGI_FORMAT_UNKNOWN,
              .ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
              .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
              .Buffer =
                  {
                      .FirstElement = 0,
                      .NumElements = static_cast<UINT>(r / stride),
                      .StructureByteStride = static_cast<UINT>(stride),
                      .Flags = D3D12_BUFFER_SRV_FLAG_NONE,
                  },
          };
          device->CreateShaderResourceView(buffer.dxGetResource(), &srvDesc, cpuHandle);
        } else if (bufferType == DescriptorWriteBufferType::RWStorage) {
          auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(this->cpuHandle, descriptorIndex + arrayIndex, CBV_SRV_UAV_DESCRIPTOR_SIZE);
          D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{
              .ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
              .Buffer =
                  {
                      .FirstElement = 0,
                      .NumElements = static_cast<UINT>(r / stride),
                      .StructureByteStride = static_cast<UINT>(stride),
                      .CounterOffsetInBytes = 0,
                      .Flags = D3D12_BUFFER_UAV_FLAG_NONE,
                  },
          };
          device->CreateUnorderedAccessView(buffer.dxGetResource(), nullptr, &uavDesc, cpuHandle);
        } else {
          DX_ABORT("Unknown buffer type for descriptor write");
        }
        return;
      }
      descriptorIndex += range.NumDescriptors;
    }
  }

  void DescriptorSet::write(const DescriptorLayout& layout, uint32_t binding, uint32_t arrayIndex, DescriptorWriteImageType imageType,
                            ImageRef image) {
    const auto& ranges = layout.dxGetRanges();
    auto device = RHI::get().dxGetDevice();

    uint32_t descriptorIndex = 0;
    for (const auto& range : ranges) {
      if (range.BaseShaderRegister == binding) {
        if (imageType == DescriptorWriteImageType::Sampled) {
          auto cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(this->cpuHandle, descriptorIndex + arrayIndex, CBV_SRV_UAV_DESCRIPTOR_SIZE);
          D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
              .Format = raw(image.format()),
              .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
          };

          switch (image.dim()) {
          case ImageDim::e1D:
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
            srvDesc.Texture1D.MipLevels = image.mips();
            break;
          case ImageDim::e2D:
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = image.mips();
            break;
          case ImageDim::e3D:
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
            srvDesc.Texture3D.MipLevels = image.mips();
            break;
          case ImageDim::eCube:
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.TextureCube.MipLevels = image.mips();
            break;
          }
          device->CreateShaderResourceView(image.dxGetResource(), &srvDesc, cpuHandle);
        }
      }
      descriptorIndex += range.NumDescriptors;
    }
  }

  void DescriptorSet::write(const DescriptorLayout& layout, std::span<const DescriptorWriteInfo> writeInfos) {
    for (const auto& info : writeInfos) {
      switch (info.type) {
      case DescriptorType::UniformBuffer:
        write(layout, info.binding, info.arrayIndex, DescriptorWriteBufferType::Uniform, info.buffer, info.offset, info.range, 0);
        break;
      case DescriptorType::StorageBuffer:
        write(layout, info.binding, info.arrayIndex, DescriptorWriteBufferType::Storage, info.buffer, info.offset, info.range, 0);
        break;
      case DescriptorType::RWStorageBuffer:
        write(layout, info.binding, info.arrayIndex, DescriptorWriteBufferType::RWStorage, info.buffer, info.offset, info.range, 0);
        break;
      case DescriptorType::SampledImage:
        write(layout, info.binding, info.arrayIndex, DescriptorWriteImageType::Sampled, info.image);
        break;
      default:
        DX_ABORT("Unsupported descriptor type for write: {}", static_cast<int>(info.type));
      }
    }
  }

} // namespace kt::rhi