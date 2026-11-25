#pragma once

#include "keptech/core/bitflag.hpp"

#ifdef KT_VULKAN
#include <Volk/volk.h>
#define F(vk, dx12) vk // NOLINT
#define VKF(vk) vk     // NOLINT
#else
#include <dxgiformat.h>
#define F(vk, dx12) dx12 // NOLINT
#define VKF(vk) 0        // NOLINT
#endif

namespace kt::rhi {
  enum class AccessFlags : uint32_t {
    IndirectCommandRead = F(VK_ACCESS_INDIRECT_COMMAND_READ_BIT, 0),
    IndexRead = F(VK_ACCESS_INDEX_READ_BIT, 0),
    VertexAttributeRead = F(VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT, 0),
    UniformRead = F(VK_ACCESS_UNIFORM_READ_BIT, 0),
    InputAttachmentRead = F(VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, 0),
    ShaderRead = F(VK_ACCESS_SHADER_READ_BIT, 0),
    ShaderWrite = F(VK_ACCESS_SHADER_WRITE_BIT, 0),
    ColorAttachmentRead = F(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT, 0),
    ColorAttachmentWrite = F(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0),
    DepthStencilAttachmentRead = F(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, 0),
    DepthStencilAttachmentWrite = F(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 0),
    TransferRead = F(VK_ACCESS_TRANSFER_READ_BIT, 0),
    TransferWrite = F(VK_ACCESS_TRANSFER_WRITE_BIT, 0),
    HostRead = F(VK_ACCESS_HOST_READ_BIT, 0),
    HostWrite = F(VK_ACCESS_HOST_WRITE_BIT, 0),
    MemoryRead = F(VK_ACCESS_MEMORY_READ_BIT, 0),
    MemoryWrite = F(VK_ACCESS_MEMORY_WRITE_BIT, 0),
    ShaderSampledRead = F(VK_ACCESS_SHADER_SAMPLED_READ_BIT, 0),
    ShaderStorageRead = F(VK_ACCESS_SHADER_STORAGE_READ_BIT, 0),
    ShaderStorageWrite = F(VK_ACCESS_SHADER_STORAGE_WRITE_BIT, 0),
  };
}

DEFINE_BITFLAG_ENUM_OPERATORS(kt::rhi::AccessFlags)