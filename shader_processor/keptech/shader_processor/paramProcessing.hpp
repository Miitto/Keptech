#pragma once

#include "keptech/shaders/resources.hpp"
#include "shader-logger.hpp"
#include "slangFormatting.hpp"
#include <expected>
#include <slang.h>
#include <utility>

namespace kt::shader_processor {
  struct CumulativeOffset {
    uint32_t value; // the actual offset
    uint32_t space; // the associated space
  };

  struct AccessPathNode {
    slang::VariableLayoutReflection* varLayout;
    size_t outer;
  };

  CumulativeOffset getCumulativeOffset(slang::ParameterCategory category, const std::vector<AccessPathNode>& accessPath, size_t leafIndex) {
    CumulativeOffset result{.value = 0, .space = 0};
    while (leafIndex < accessPath.size()) {
      auto& node = accessPath[leafIndex];
      result.value += node.varLayout->getOffset(category);
      result.space += node.varLayout->getBindingSpace(category);
      leafIndex = node.outer;
    }
    return result;
  }

  // TODO: Push descriptor attribute
  void parseParameterInner(slang::VariableLayoutReflection& param, std::vector<AccessPathNode>& accessPath, std::vector<size_t>& leaves,
                           size_t outer = ~0u) {
    size_t me = accessPath.size();
    accessPath.push_back({&param, outer});
    auto& type = *param.getTypeLayout();
    switch (type.getKind()) {
    case slang::TypeReflection::Kind::Struct: {
      auto fieldCount = type.getFieldCount();
      for (uint32_t i = 0; i < fieldCount; ++i) {
        auto& field = *type.getFieldByIndex(i);
        parseParameterInner(field, accessPath, leaves, me);
      }
    } break;
    case slang::TypeReflection::Kind::ConstantBuffer:
    case slang::TypeReflection::Kind::ParameterBlock:
    case slang::TypeReflection::Kind::TextureBuffer:
    case slang::TypeReflection::Kind::ShaderStorageBuffer: {
      auto& containerLayout = *type.getContainerVarLayout();
      if (containerLayout.getCategory() == slang::ParameterCategory::ConstantBuffer) {
        leaves.push_back(me);
      } else {
        parseParameterInner(containerLayout, accessPath, leaves, me);
      }

      auto& elementLayout = *type.getElementVarLayout();
      parseParameterInner(elementLayout, accessPath, leaves, me);
    } break;
    case slang::TypeReflection::Kind::Array: {
      auto& elementLayout = *type.getElementVarLayout();
      parseParameterInner(elementLayout, accessPath, leaves, me);
    } break;
    case slang::TypeReflection::Kind::Resource: {
      leaves.push_back(me);
    } break;
    default:
      break; // Should only get here with the element layout for a constant buffer or the like. Ignore.
    }
  }

  std::expected<std::vector<shaders::ResourceSet>, std::string> parseParameter(slang::VariableLayoutReflection& param) {
    std::vector<AccessPathNode> accessPath;
    std::vector<size_t> leaves;

    parseParameterInner(param, accessPath, leaves);

    if (leaves.empty()) {
      return std::vector<shaders::ResourceSet>{}; // No resources found, return an empty ResourceSet
    }

    std::vector<CumulativeOffset> offsets;
    offsets.reserve(leaves.size());
    for (auto leafIndex : leaves) {
      offsets.push_back(getCumulativeOffset(accessPath[leafIndex].varLayout->getCategory(), accessPath, leafIndex));
    }

    std::vector<std::vector<shaders::ResourceBinding>> resources;
    uint32_t lastSpace = 0u;
    for (const auto& offset : offsets) {
      lastSpace = std::max(lastSpace, offset.space);
    }

    resources.resize(lastSpace + 1);

    for (size_t i = 0; i < leaves.size(); ++i) {
      auto leafIndex = leaves[i];
      auto& offset = offsets[i];
      auto& node = accessPath[leafIndex];
      auto& type = *node.varLayout->getTypeLayout();

      shaders::ResourceBinding binding{
          .binding = offset.value,
          .count = type.isArray() ? static_cast<uint32_t>(type.getElementCount()) : 1u,
      };

      switch (type.getParameterCategory()) {
      case slang::ParameterCategory::ShaderResource: {
        bool isTexArray = (type.getResourceShape() & SLANG_TEXTURE_ARRAY_FLAG) != 0;
        bool isCombined = (type.getResourceShape() & SLANG_TEXTURE_COMBINED_FLAG) != 0;

        SHDR_ASSERT(!isCombined, "Only bindless combined texture samplers are supported");

        switch (type.getResourceShape() & SLANG_RESOURCE_BASE_SHAPE_MASK) {
        case SLANG_TEXTURE_1D: {
          binding.type = isTexArray ? shaders::ShaderResourceType::Texture1DArray : shaders::ShaderResourceType::Texture1D;
        } break;
        case SLANG_TEXTURE_2D: {
          binding.type = isTexArray ? shaders::ShaderResourceType::Texture2DArray : shaders::ShaderResourceType::Texture2D;
        } break;
        case SLANG_TEXTURE_3D: {
          binding.type = isTexArray ? shaders::ShaderResourceType::Texture3DArray : shaders::ShaderResourceType::Texture3D;
        } break;
        case SLANG_TEXTURE_CUBE: {
          SHDR_ASSERT(!isTexArray, "Cube texture arrays are not supported");
          binding.type = shaders::ShaderResourceType::TextureCube;
        } break;
        case SLANG_TEXTURE_BUFFER:
          SHDR_ASSERT(false, "Texture buffers are not supported");
          break;
        case SLANG_STRUCTURED_BUFFER: {
          if (type.getResourceAccess() == SLANG_RESOURCE_ACCESS_READ_WRITE) {
            binding.type = shaders::ShaderResourceType::RWStorageBuffer;
          } else {
            binding.type = shaders::ShaderResourceType::StorageBuffer;
          }
        } break;
        case SLANG_BYTE_ADDRESS_BUFFER:
          SHDR_ASSERT(false, "Byte address buffers are not supported");
          break;
        case SLANG_RESOURCE_UNKNOWN:
          SHDR_ASSERT(false, "Unknown resource type");
          break;
        case SLANG_ACCELERATION_STRUCTURE:
          SHDR_ASSERT(false, "Acceleration structures are not supported");
          break;
        default:
          break;
        }
      } break;
      case slang::ParameterCategory::ConstantBuffer:
        binding.type = shaders::ShaderResourceType::UniformBuffer;
        break;
      case slang::ParameterCategory::SamplerState:
        binding.type = shaders::ShaderResourceType::Sampler;
        break;
      }

      resources[offset.space].push_back(binding);
    }

#ifndef NDEBUG
    SHDR_DEBUG("Shader layout:");
    for (size_t space = 0; space < resources.size(); ++space) {
      auto& resourceSet = resources[space];
      SHDR_DEBUG("  Resource space {}: {} resources", space, resourceSet.size());
      if (resourceSet.empty()) {
        SHDR_WARN("Resource space {} is empty.", space);
      }

      for (size_t i = 0; i < resourceSet.size(); ++i) {
        SHDR_DEBUG("    {}{}: b{} s{}", resourceSet[i].type, resourceSet[i].count > 1 ? fmt::format("[{}]", resourceSet[i].count) : "",
                   resourceSet[i].binding, space);
      }
    }
#endif

    std::vector<shaders::ResourceSet> result;
    result.reserve(resources.size());
    for (size_t space = 0; space < resources.size(); ++space) {
      auto& resourceSet = resources[space];
      if (resourceSet.empty()) {
        continue;
      }
      result.emplace_back(static_cast<uint8_t>(space), std::move(resourceSet));
    }

    return {std::move(result)};
  }
} // namespace kt::shader_processor