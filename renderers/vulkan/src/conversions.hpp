#pragma once

#include "keptech/core/bitflag.hpp"
#include "keptech/core/rendering/buffer.hpp"
#include "keptech/core/rendering/pipeline.hpp"
#include "keptech/core/rendering/texture.hpp"
#include <keptech/shaders/shader.h>
#include <vk_mem_alloc.hpp>
#include <vulkan/vulkan.hpp>

namespace keptech::vkh {
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

    vk::Format from(TextureFormat format, vk::Format defaultFormat) {
      using Format = TextureFormat;
      switch (format) {
      case Format::RGB8:
        return vk::Format::eR8G8B8Unorm;
      case Format::RGBA8:
        return vk::Format::eR8G8B8A8Unorm;
      case Format::Default:
        return defaultFormat;
      case Format::Undefined:
        return vk::Format::eUndefined;
      case Format::R8:
        return vk::Format::eR8Unorm;
      case Format::R16F:
        return vk::Format::eR16Sfloat;
      case Format::R32F:
        return vk::Format::eR32Sfloat;
      case Format::RG8:
        return vk::Format::eR8G8Unorm;
      case Format::RG16F:
        return vk::Format::eR16G16Sfloat;
      case Format::RG32F:
        return vk::Format::eR32G32Sfloat;
      case Format::RGB16F:
        return vk::Format::eR16G16B16Sfloat;
      case Format::RGB32F:
        return vk::Format::eR32G32B32Sfloat;
      case Format::RGBA16F:
        return vk::Format::eR16G16B16A16Sfloat;
      case Format::RGBA32F:
        return vk::Format::eR32G32B32A32Sfloat;
      case Format::Depth16:
        return vk::Format::eD16Unorm;
      case Format::Depth24Stencil8:
        return vk::Format::eD24UnormS8Uint;
      }
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
      case BufferMemoryType::GpuOnly:
        return vma::MemoryUsage::eGpuOnly;
      case BufferMemoryType::CpuToGpu:
        return vma::MemoryUsage::eCpuToGpu;
      case BufferMemoryType::GpuToCpu:
        return vma::MemoryUsage::eGpuToCpu;
      }
    }

    vk::ImageAspectFlags aspectFromFormat(TextureFormat format) {
      vk::ImageAspectFlags aspectMask = vk::ImageAspectFlagBits::eColor;
      switch (format) {
      case TextureFormat::Depth16:
        aspectMask = vk::ImageAspectFlagBits::eDepth;
        break;
      case TextureFormat::Depth24Stencil8:
        aspectMask =
            vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
        break;
      default:
        break;
      }

      return aspectMask;
    }

    vk::Format from(shaders::DataType type, vk::Format defaultFormat) {
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
      default:
        return defaultFormat;
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
        return 0;
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
  } // namespace

} // namespace keptech::vkh
