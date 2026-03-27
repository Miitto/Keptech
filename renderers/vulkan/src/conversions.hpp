#pragma once

#include "keptech/core/bitflag.hpp"
#include "keptech/core/rendering/buffer.hpp"
#include "keptech/core/rendering/pipeline.hpp"
#include "keptech/core/rendering/texture.hpp"
#include "vk-logger.hpp"
#include <keptech/shaders/shader.h>
#include <system_error>
#include <vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

namespace kt::vkh {
  namespace {
    vk::ShaderStageFlagBits from(shaders::ShaderStages stages) {
      switch (stages) {
      case shaders::ShaderStages::Vertex:
        return vk::ShaderStageFlagBits::eVertex;
      case shaders::ShaderStages::Fragment:
        return vk::ShaderStageFlagBits::eFragment;
      case shaders::ShaderStages::Compute:
        return vk::ShaderStageFlagBits::eCompute;
      }
    }

    vk::ShaderStageFlags from(Bitflag<shaders::ShaderStages> stages) {
      using S = shaders::ShaderStages;
      vk::ShaderStageFlags flags = {};
      if (stages.has(S::Vertex)) {
        flags = flags | vk::ShaderStageFlagBits::eVertex;
      }
      if (stages.has(S::Fragment)) {
        flags = flags | vk::ShaderStageFlagBits::eFragment;
      }
      if (stages.has(S::Compute)) {
        flags = flags | vk::ShaderStageFlagBits::eCompute;
      }

      return flags;
    }

    vk::Format from(TextureFormat format) {
#define C(_KT, _VK)                                                            \
  case TextureFormat::_KT:                                                     \
    return vk::Format::_VK

      switch (format) {
        C(Undefined, eUndefined);
        C(R8UNorm, eR8Unorm);
        C(R8SNorm, eR8Snorm);
        C(R16UNorm, eR16Unorm);
        C(R16SNorm, eR16Snorm);
        C(RG8UNorm, eR8G8Unorm);
        C(RG8SNorm, eR8G8Snorm);
        C(RG16UNorm, eR16G16Unorm);
        C(RG16SNorm, eR16G16Snorm);
        C(RGB8UNorm, eR8G8B8Unorm);
        C(RGB8SNorm, eR8G8B8Snorm);
        C(RGB16UNorm, eR16G16B16Unorm);
        C(RGB16SNorm, eR16G16B16Snorm);
        C(RGBA8UNorm, eR8G8B8A8Unorm);
        C(RGBA8SNorm, eR8G8B8A8Snorm);
        C(RGBA16UNorm, eR16G16B16A16Unorm);
        C(R16F, eR16Sfloat);
        C(RG16F, eR16G16Sfloat);
        C(RGB16F, eR16G16B16Sfloat);
        C(RGBA16F, eR16G16B16A16Sfloat);
        C(R32F, eR32Sfloat);
        C(RG32F, eR32G32Sfloat);
        C(RGB32F, eR32G32B32Sfloat);
        C(RGBA32F, eR32G32B32A32Sfloat);
        C(Depth16, eD16Unorm);
        C(Depth32F, eD32Sfloat);
        C(Depth24Stencil8, eD24UnormS8Uint);
        C(Depth32FStencil8, eD32SfloatS8Uint);
        C(Stencil8, eS8Uint);
      case TextureFormat::Depth24:
        return vk::Format::eUndefined;
      }
#undef C
    }

    TextureFormat from(vk::Format format) {
#define C(_KT, _VK)                                                            \
  case vk::Format::_VK:                                                        \
    return TextureFormat::_KT

      switch (format) {
        C(Undefined, eUndefined);
        C(R8UNorm, eR8Unorm);
        C(R8SNorm, eR8Snorm);
        C(R16UNorm, eR16Unorm);
        C(R16SNorm, eR16Snorm);
        C(RG8UNorm, eR8G8Unorm);
        C(RG8SNorm, eR8G8Snorm);
        C(RG16UNorm, eR16G16Unorm);
        C(RG16SNorm, eR16G16Snorm);
        C(RGB8UNorm, eR8G8B8Unorm);
        C(RGB8SNorm, eR8G8B8Snorm);
        C(RGB16UNorm, eR16G16B16Unorm);
        C(RGB16SNorm, eR16G16B16Snorm);
        C(RGBA8UNorm, eR8G8B8A8Unorm);
        C(RGBA8SNorm, eR8G8B8A8Snorm);
        C(RGBA16UNorm, eR16G16B16A16Unorm);
        C(R16F, eR16Sfloat);
        C(RG16F, eR16G16Sfloat);
        C(RGB16F, eR16G16B16Sfloat);
        C(RGBA16F, eR16G16B16A16Sfloat);
        C(R32F, eR32Sfloat);
        C(RG32F, eR32G32Sfloat);
        C(RGB32F, eR32G32B32Sfloat);
        C(RGBA32F, eR32G32B32A32Sfloat);
        C(Depth16, eD16Unorm);
        C(Depth32F, eD32Sfloat);
        C(Depth24Stencil8, eD24UnormS8Uint);
        C(Depth32FStencil8, eD32SfloatS8Uint);
        C(Stencil8, eS8Uint);
      default:
        VK_WARN("Unsupported Vulkan format: {}. Returning Undefined.",
                vk::to_string(format));
        return TextureFormat::Undefined;
      }
#undef C
    }

    vk::ImageUsageFlags from(Bitflag<TextureUsage> usage) {
      using Usage = TextureUsage;

      vk::ImageUsageFlags flags = {};

      if (usage.has(Usage::Sampled)) {
        flags = flags | vk::ImageUsageFlagBits::eSampled;
      }
      if (usage.has(Usage::RenderTarget)) {
        flags = flags | vk::ImageUsageFlagBits::eColorAttachment;
      }
      if (usage.has(Usage::Storage)) {
        flags = flags | vk::ImageUsageFlagBits::eStorage;
      }
      if (usage.has(Usage::DepthStencil)) {
        flags = flags | vk::ImageUsageFlagBits::eDepthStencilAttachment;
      }
      if (usage.has(Usage::TransferSrc)) {
        flags = flags | vk::ImageUsageFlagBits::eTransferSrc;
      }
      if (usage.has(Usage::TransferDst)) {
        flags = flags | vk::ImageUsageFlagBits::eTransferDst;
      }
      return flags;
    }

    vk::PrimitiveTopology from(Topology topology) {
      switch (topology) {
      case Topology::TriangleList:
        return vk::PrimitiveTopology::eTriangleList;
      case Topology::TriangleStrip:
        return vk::PrimitiveTopology::eTriangleStrip;
      case Topology::LineList:
        return vk::PrimitiveTopology::eLineList;
      case Topology::LineStrip:
        return vk::PrimitiveTopology::eLineStrip;
      case Topology::PointList:
        return vk::PrimitiveTopology::ePointList;
      default:
        return vk::PrimitiveTopology::eTriangleList;
      }
    }

    vk::PolygonMode from(PolygonMode mode) {
      switch (mode) {
      case PolygonMode::Fill:
        return vk::PolygonMode::eFill;
      case PolygonMode::Line:
        return vk::PolygonMode::eLine;
      case PolygonMode::Point:
        return vk::PolygonMode::ePoint;
      default:
        return vk::PolygonMode::eFill;
      }
    }

    vk::CullModeFlags from(CullMode mode) {
      switch (mode) {
      case CullMode::None:
        return vk::CullModeFlagBits::eNone;
      case CullMode::Front:
        return vk::CullModeFlagBits::eFront;
      case CullMode::Back:
        return vk::CullModeFlagBits::eBack;
      case CullMode::FrontAndBack:
        return vk::CullModeFlagBits::eFrontAndBack;
      default:
        return vk::CullModeFlagBits::eNone;
      }
    }

    vk::FrontFace from(FrontFace face) {
      switch (face) {
      case FrontFace::Clockwise:
        return vk::FrontFace::eClockwise;
      case FrontFace::CounterClockwise:
        return vk::FrontFace::eCounterClockwise;
      default:
        return vk::FrontFace::eClockwise;
      }
    }

    vk::BlendFactor from(BlendFactor factor) {
      switch (factor) {
      case BlendFactor::Zero:
        return vk::BlendFactor::eZero;
      case BlendFactor::One:
        return vk::BlendFactor::eOne;
      case BlendFactor::SrcAlpha:
        return vk::BlendFactor::eSrcAlpha;
      case BlendFactor::OneMinusSrcAlpha:
        return vk::BlendFactor::eOneMinusSrcAlpha;
      default:
        return vk::BlendFactor::eOne;
      }
    }

    vk::CompareOp from(DepthCompareOp op) {
      switch (op) {
      case DepthCompareOp::Never:
        return vk::CompareOp::eNever;
      case DepthCompareOp::Less:
        return vk::CompareOp::eLess;
      case DepthCompareOp::Equal:
        return vk::CompareOp::eEqual;
      case DepthCompareOp::LessEqual:
        return vk::CompareOp::eLessOrEqual;
      case DepthCompareOp::Greater:
        return vk::CompareOp::eGreater;
      case DepthCompareOp::NotEqual:
        return vk::CompareOp::eNotEqual;
      case DepthCompareOp::GreaterEqual:
        return vk::CompareOp::eGreaterOrEqual;
      case DepthCompareOp::Always:
        return vk::CompareOp::eAlways;
      default:
        return vk::CompareOp::eLess;
      }
    }

    vk::BufferUsageFlags from(Bitflag<BufferUsage> usage) {
      using Usage = BufferUsage;

      vk::BufferUsageFlags flags = {};

      if (usage.has(Usage::Vertex)) {
        flags = flags | vk::BufferUsageFlagBits::eVertexBuffer;
      }
      if (usage.has(Usage::Index)) {
        flags = flags | vk::BufferUsageFlagBits::eIndexBuffer;
      }
      if (usage.has(Usage::Uniform)) {
        flags = flags | vk::BufferUsageFlagBits::eUniformBuffer;
      }
      if (usage.has(Usage::Storage)) {
        flags = flags | vk::BufferUsageFlagBits::eStorageBuffer;
      }
      if (usage.has(Usage::TransferSrc)) {
        flags = flags | vk::BufferUsageFlagBits::eTransferSrc;
      }
      if (usage.has(Usage::TransferDst)) {
        flags = flags | vk::BufferUsageFlagBits::eTransferDst;
      }
      return flags;
    }

    vma::MemoryUsage from(BufferMemoryType memoryType) {
      switch (memoryType) {
      case BufferMemoryType::Auto:
        return vma::MemoryUsage::eAuto;
      case BufferMemoryType::PreferDevice:
        return vma::MemoryUsage::eAutoPreferDevice;
      case BufferMemoryType::PreferHost:
        return vma::MemoryUsage::eAutoPreferHost;
        break;
      }
    }

    vk::ImageAspectFlags aspectFromFormat(TextureFormat format) {
      vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
      switch (format) {
      case TextureFormat::Depth16:
      case TextureFormat::Depth24:
      case TextureFormat::Depth32F:
        aspectMask = vk::ImageAspectFlagBits::eDepth;
        break;
      case TextureFormat::Depth24Stencil8:
      case TextureFormat::Depth32FStencil8:
        aspectMask =
            vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        break;
      case TextureFormat::Stencil8:
        aspectMask = vk::ImageAspectFlagBits::eStencil;
        break;
      default:
        break;
      }

      return aspectMask;
    }

    vk::ImageAspectFlags aspectFromFormat(vk::Format format) {
      vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
      switch (format) {
      case vk::Format::eD16Unorm:
      case vk::Format::eD32Sfloat:
        aspectMask = vk::ImageAspectFlagBits::eDepth;
        break;
      case vk::Format::eD24UnormS8Uint:
      case vk::Format::eD32SfloatS8Uint:
        aspectMask =
            vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        break;
      case vk::Format::eS8Uint:
        aspectMask = vk::ImageAspectFlagBits::eStencil;
        break;
      default:
        break;
      }

      return aspectMask;
    }

    bool isDepthFormat(vk::Format format) {
      switch (format) {
      case vk::Format::eD16Unorm:
      case vk::Format::eD24UnormS8Uint:
      case vk::Format::eD32Sfloat:
      case vk::Format::eD32SfloatS8Uint:
        return true;
      default:
        return false;
      }
    }

    size_t componentsSize(TextureFormat format) {
      switch (format) {
      case TextureFormat::Undefined:
        return 0;
      case TextureFormat::R8UNorm:
      case TextureFormat::R8SNorm:
      case TextureFormat::Stencil8:
        return 1;
      case TextureFormat::R16UNorm:
      case TextureFormat::R16SNorm:
      case TextureFormat::RG8UNorm:
      case TextureFormat::RG8SNorm:
      case TextureFormat::R16F:
      case TextureFormat::Depth16:
        return 2;
      case TextureFormat::RGB8UNorm:
      case TextureFormat::RGB8SNorm:
      case TextureFormat::Depth24:
        return 3;
      case TextureFormat::RG16UNorm:
      case TextureFormat::RG16SNorm:
      case TextureFormat::RGBA8UNorm:
      case TextureFormat::RGBA8SNorm:
      case TextureFormat::RG16F:
      case TextureFormat::R32F:
      case TextureFormat::Depth32F:
      case TextureFormat::Depth24Stencil8:
        return 4;
      case TextureFormat::Depth32FStencil8:
        return 5;
      case TextureFormat::RGB16UNorm:
      case TextureFormat::RGB16SNorm:
      case TextureFormat::RGB16F:
        return 6;
      case TextureFormat::RGBA16UNorm:
      case TextureFormat::RGBA16F:
      case TextureFormat::RG32F:
        return 8;
      case TextureFormat::RGB32F:
        return 12;
      case TextureFormat::RGBA32F:
        return 16;
      }
    }

    vk::Format from(shaders::DataType type) {
      using T = shaders::DataType;
      switch (type) {
      case T::F32:
        return vk::Format::eR32Sfloat;
      case T::F32_2:
        return vk::Format::eR32G32Sfloat;
      case T::F32_3:
        return vk::Format::eR32G32B32Sfloat;
      case T::F32_4:
        return vk::Format::eR32G32B32A32Sfloat;
      case T::U8:
        return vk::Format::eR8Uint;
      case T::U8_2:
        return vk::Format::eR8G8Uint;
      case T::U8_3:
        return vk::Format::eR8G8B8Uint;
      case T::U8_4:
        return vk::Format::eR8G8B8A8Uint;
      case T::U16:
        return vk::Format::eR16Uint;
      case T::U16_2:
        return vk::Format::eR16G16Uint;
      case T::U16_3:
        return vk::Format::eR16G16B16Uint;
      case T::U16_4:
        return vk::Format::eR16G16B16A16Uint;
      case T::U32:
        return vk::Format::eR32Uint;
      case T::U32_2:
        return vk::Format::eR32G32Uint;
      case T::U32_3:
        return vk::Format::eR32G32B32Uint;
      case T::U32_4:
        return vk::Format::eR32G32B32A32Uint;
      case shaders::DataType::None:
      case shaders::DataType::Void:
        return vk::Format::eUndefined;
      case shaders::DataType::Bool:
        return vk::Format::eR8Uint;
      case shaders::DataType::F16:
        return vk::Format::eR16Sfloat;
      case shaders::DataType::F64:
        return vk::Format::eR64Sfloat;
      case shaders::DataType::F16_2:
        return vk::Format::eR16G16Sfloat;
      case shaders::DataType::F64_2:
        return vk::Format::eR64G64Sfloat;
      case shaders::DataType::F16_3:
        return vk::Format::eR16G16B16Sfloat;
      case shaders::DataType::F64_3:
        return vk::Format::eR64G64B64Sfloat;
      case shaders::DataType::F16_4:
        return vk::Format::eR16G16B16A16Sfloat;
      case shaders::DataType::F64_4:
        return vk::Format::eR64G64B64A64Sfloat;
      case shaders::DataType::I8:
        return vk::Format::eR8Sint;
      case shaders::DataType::I16:
        return vk::Format::eR16Sint;
      case shaders::DataType::I32:
        return vk::Format::eR32Sint;
      case shaders::DataType::I64:
        return vk::Format::eR64Sint;
      case shaders::DataType::I8_2:
        return vk::Format::eR8G8Sint;
      case shaders::DataType::I16_2:
        return vk::Format::eR16G16Sint;
      case shaders::DataType::I32_2:
        return vk::Format::eR32G32Sint;
      case shaders::DataType::I64_2:
        return vk::Format::eR64G64Sint;
      case shaders::DataType::I8_3:
        return vk::Format::eR8G8B8Sint;
      case shaders::DataType::I16_3:
        return vk::Format::eR16G16B16Sint;
      case shaders::DataType::I32_3:
        return vk::Format::eR32G32B32Sint;
      case shaders::DataType::I64_3:
        return vk::Format::eR64G64B64Sint;
      case shaders::DataType::I8_4:
        return vk::Format::eR8G8B8A8Sint;
      case shaders::DataType::I16_4:
        return vk::Format::eR16G16B16A16Sint;
      case shaders::DataType::I32_4:
        return vk::Format::eR32G32B32A32Sint;
      case shaders::DataType::I64_4:
        return vk::Format::eR64G64B64A64Sint;
      case shaders::DataType::U64:
        return vk::Format::eR64Uint;
      case shaders::DataType::U64_2:
        return vk::Format::eR64G64Uint;
      case shaders::DataType::U64_3:
        return vk::Format::eR64G64B64Uint;
      case shaders::DataType::U64_4:
        return vk::Format::eR64G64B64A64Uint;
      case shaders::DataType::F32_4x4:
        return vk::Format::eR32G32B32A32Sfloat;
      case shaders::DataType::Sampler2D:
        return vk::Format::eUndefined;
      }
    }

    uint32_t getSize(shaders::DataType type) {
      using T = shaders::DataType;
      switch (type) {
      case T::F32:
        return 4;
      case T::F32_2:
        return 8;
      case T::F32_3:
        return 12;
      case T::F32_4:
        return 16;
      case T::U8:
        return 1;
      case T::U8_2:
        return 2;
      case T::U8_3:
        return 3;
      case T::U8_4:
        return 4;
      case T::U16:
        return 2;
      case T::U16_2:
        return 4;
      case T::U16_3:
        return 6;
      case T::U16_4:
        return 8;
      case T::U32:
        return 4;
      case T::U32_2:
        return 8;
      case T::U32_3:
        return 12;
      case T::U32_4:
        return 16;
      case shaders::DataType::None:
      case shaders::DataType::Void:
        return 0;
      case shaders::DataType::Bool:
        return 1;
      case shaders::DataType::F16:
        return 2;
      case shaders::DataType::F64:
        return 8;
      case shaders::DataType::F16_2:
        return 4;
      case shaders::DataType::F64_2:
        return 16;
      case shaders::DataType::F16_3:
        return 6;
      case shaders::DataType::F64_3:
        return 24;
      case shaders::DataType::F16_4:
        return 8;
      case shaders::DataType::F64_4:
        return 32;
      case shaders::DataType::I8:
        return 1;
      case shaders::DataType::I16:
        return 2;
      case shaders::DataType::I32:
        return 4;
      case shaders::DataType::I64:
        return 8;
      case shaders::DataType::I8_2:
        return 2;
      case shaders::DataType::I16_2:
        return 4;
      case shaders::DataType::I32_2:
        return 8;
      case shaders::DataType::I64_2:
        return 16;
      case shaders::DataType::I8_3:
        return 3;
      case shaders::DataType::I16_3:
        return 6;
      case shaders::DataType::I32_3:
        return 12;
      case shaders::DataType::I64_3:
        return 24;
      case shaders::DataType::I8_4:
        return 4;
      case shaders::DataType::I16_4:
        return 8;
      case shaders::DataType::I32_4:
        return 16;
      case shaders::DataType::I64_4:
        return 32;
      case shaders::DataType::U64:
        return 8;
      case shaders::DataType::U64_2:
        return 16;
      case shaders::DataType::U64_3:
        return 24;
      case shaders::DataType::U64_4:
        return 32;
      case shaders::DataType::F32_4x4:
        return 64;
      case shaders::DataType::Sampler2D:
        return 0;
      }
    }

    vk::Filter from(SamplerFilter filter) {
      return static_cast<vk::Filter>(filter);
    }

    vk::SamplerAddressMode from(SamplerAddressMode mode) {
      return static_cast<vk::SamplerAddressMode>(mode);
    }

    vk::SamplerMipmapMode fromMip(SamplerFilter mode) {
      return static_cast<vk::SamplerMipmapMode>(mode);
    }

  } // namespace

} // namespace kt::vkh
