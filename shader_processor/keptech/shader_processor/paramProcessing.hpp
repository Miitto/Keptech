#pragma once

#include "conversions.hpp"
#include "keptech/shaders/info.hpp"
#include "keptech/shaders/resources.hpp"
#include "shader-logger.hpp"
#include "slangFormatting.hpp"
#include <expected>
#include <slang.h>
#include <spdlog/fmt/bundled/ranges.h>
#include <utility>

namespace kt::shader_processor {

  struct LayoutManager {
    struct AccessPath;

    void printVariableLayout(slang::VariableLayoutReflection* variableLayout, AccessPath accessPath) {
      printVaryingParameterInfo(variableLayout, accessPath);

      ExtendedAccessPath variablePath(accessPath, variableLayout);

      printTypeLayout(variableLayout->getTypeLayout(), variablePath);

      auto typeLayout = variableLayout->getTypeLayout();
      switch (typeLayout->getKind()) {
      case slang::TypeReflection::Kind::Resource: {
        auto shape = typeLayout->getResourceShape();
        bool isTexArray = (shape & SLANG_TEXTURE_ARRAY_FLAG) != 0;
        auto baseShape = shape & SLANG_RESOURCE_BASE_SHAPE_MASK;
        switch (baseShape) {
        case SLANG_STRUCTURED_BUFFER: {
          auto elementTypeLayout = typeLayout->getElementTypeLayout();
          size_t stride = elementTypeLayout->getStride();
          auto access = typeLayout->getResourceAccess();
          bool isWriteable = (access == SLANG_RESOURCE_ACCESS_WRITE || access == SLANG_RESOURCE_ACCESS_READ_WRITE);
          shaders::ShaderResourceType resourceType =
              isWriteable ? shaders::ShaderResourceType::RWStorageBuffer : shaders::ShaderResourceType::StorageBuffer;

          auto offset = calculateCumulativeOffset(variableLayout, variableLayout->getCategory(), accessPath);

          if (resources->sets.size() <= offset.space) {
            resources->sets.resize(offset.space + 1);
          }

          auto bindingIt = std::find_if(
              resources->sets[offset.space].resources.begin(), resources->sets[offset.space].resources.end(),
              [resourceType, offset](const shaders::ResourceBinding& b) { return b.binding == offset.value && b.type == resourceType; });
          if (bindingIt == resources->sets[offset.space].resources.end()) {
            shaders::ResourceBinding bindingInfo{
                .name = fmt::format("{}", fmt::join(offset.name, ".")),
                .type = resourceType,
                .binding = static_cast<uint32_t>(offset.value),
                .count = 1,
                .isPush = false,
                .bufferInfo = {.sizeOrStride = stride},
            };
            resources->sets[offset.space].resources.push_back(bindingInfo);
            bindingIt = std::prev(resources->sets[offset.space].resources.end());
          }
        } break;
        case SLANG_TEXTURE_1D: {
          auto offset = calculateCumulativeOffset(variableLayout, variableLayout->getCategory(), accessPath);
          if (resources->sets.size() <= offset.space) {
            resources->sets.resize(offset.space + 1);
          }

          resources->sets[offset.space].resources.push_back(shaders::ResourceBinding{
              .name = fmt::format("{}", fmt::join(offset.name, ".")),
              .type = isTexArray ? shaders::ShaderResourceType::Texture1DArray : shaders::ShaderResourceType::Texture1D,
              .binding = static_cast<uint32_t>(offset.value),
              .count = 1,
              .isPush = false,
          });
        } break;
        case SLANG_TEXTURE_2D: {
          auto offset = calculateCumulativeOffset(variableLayout, variableLayout->getCategory(), accessPath);
          if (resources->sets.size() <= offset.space) {
            resources->sets.resize(offset.space + 1);
          }

          resources->sets[offset.space].resources.push_back(shaders::ResourceBinding{
              .name = fmt::format("{}", fmt::join(offset.name, ".")),
              .type = isTexArray ? shaders::ShaderResourceType::Texture2DArray : shaders::ShaderResourceType::Texture2D,
              .binding = static_cast<uint32_t>(offset.value),
              .count = 1,
              .isPush = false,
          });
        } break;
        case SLANG_TEXTURE_3D: {
          auto offset = calculateCumulativeOffset(variableLayout, variableLayout->getCategory(), accessPath);
          if (resources->sets.size() <= offset.space) {
            resources->sets.resize(offset.space + 1);
          }

          resources->sets[offset.space].resources.push_back(shaders::ResourceBinding{
              .name = fmt::format("{}", fmt::join(offset.name, ".")),
              .type = isTexArray ? shaders::ShaderResourceType::Texture3DArray : shaders::ShaderResourceType::Texture3D,
              .binding = static_cast<uint32_t>(offset.value),
              .count = 1,
              .isPush = false,
          });
        } break;
        case SLANG_TEXTURE_CUBE: {
          SHDR_ASSERT(!isTexArray, "Cube textures cannot be arrays.");
          auto offset = calculateCumulativeOffset(variableLayout, variableLayout->getCategory(), accessPath);
          if (resources->sets.size() <= offset.space) {
            resources->sets.resize(offset.space + 1);
          }

          resources->sets[offset.space].resources.push_back(shaders::ResourceBinding{
              .name = fmt::format("{}", fmt::join(offset.name, ".")),
              .type = shaders::ShaderResourceType::TextureCube,
              .binding = static_cast<uint32_t>(offset.value),
              .count = 1,
              .isPush = false,
          });
        } break;
        case SLANG_BYTE_ADDRESS_BUFFER: {
          SHDR_ABORT("Byte address buffers are not supported.");
        } break;
        }
      } break;
      case slang::TypeReflection::Kind::Array:
      case slang::TypeReflection::Kind::Vector:
      case slang::TypeReflection::Kind::Matrix:
      case slang::TypeReflection::Kind::Scalar: {
        if (accessPath.deepestBuffer == nullptr) {
          if (variableLayout->getCategory() == slang::ParameterCategory::Uniform) {
            auto offset = calculateCumulativeOffset(variableLayout, variableLayout->getCategory(), accessPath);
            if (resources->pushConstants.size < offset.value + variableLayout->getTypeLayout()->getSize()) {
              resources->pushConstants.size = offset.value + variableLayout->getTypeLayout()->getSize();
            }
            resources->pushConstants.space = offset.space;
            resources->pushConstants
                .fieldOffsets[fmt::format("{}{}", fmt::join(offset.name, "."),
                                          variableLayout->getTypeLayout()->getKind() == slang::TypeReflection::Kind::Array ? "[]" : "")] =
                shaders::FieldInfo{offset.value, variableLayout->getTypeLayout()->getSize(), variableLayout->getTypeLayout()->getStride()};

            SHDR_DEBUG("Push constant field {}: offset {}, size {} with cats:", fmt::format("{}", fmt::join(offset.name, ".")),
                       offset.value, variableLayout->getTypeLayout()->getSize());
            auto catCount = variableLayout->getCategoryCount();
            for (int i = 0; i < catCount; ++i) {
              auto layoutUnit = variableLayout->getCategoryByIndex(i);
              SHDR_DEBUG("  {}: offset {}, space {}", layoutUnit, variableLayout->getOffset(layoutUnit),
                         variableLayout->getBindingSpace(layoutUnit));
            }
          }
        } else {
          auto bufferVar = accessPath.deepestBuffer->variableLayout;

          size_t sizeOrStride = 0;
          shaders::ShaderResourceType type = shaders::ShaderResourceType::UniformBuffer;
          switch (bufferVar->getTypeLayout()->getKind()) {
          case slang::TypeReflection::Kind::ParameterBlock:
          case slang::TypeReflection::Kind::ConstantBuffer:
            sizeOrStride = bufferVar->getTypeLayout()->getElementTypeLayout()->getSize();
            break;
          case slang::TypeReflection::Kind::Resource: {
            auto shape = bufferVar->getTypeLayout()->getResourceShape();
            if ((shape & SLANG_RESOURCE_BASE_SHAPE_MASK) == SLANG_STRUCTURED_BUFFER) {
              auto access = bufferVar->getTypeLayout()->getResourceAccess();
              bool writable = (access == SLANG_RESOURCE_ACCESS_WRITE || access == SLANG_RESOURCE_ACCESS_READ_WRITE);
              if (writable) {
                type = shaders::ShaderResourceType::RWStorageBuffer;
              } else {
                type = shaders::ShaderResourceType::StorageBuffer;
              }
              sizeOrStride = bufferVar->getTypeLayout()->getElementTypeLayout()->getStride();
            } else {
              SHDR_ABORT("Unexpected resource type for buffer variable: {}", shape);
            }
          } break;
          default:
            SHDR_ABORT("Unexpected type for buffer variable: {}", bufferVar->getTypeLayout()->getKind());
          }

          AccessPath bufferAccessPath{};
          bufferAccessPath.leaf = accessPath.deepestBuffer;
          auto [byteOffset, _s, varName] = calculateCumulativeOffset(variableLayout, variableLayout->getCategory(), accessPath);
          auto [binding, space, bufName] = calculateCumulativeOffset(bufferVar->getCategory(), bufferAccessPath);

          std::vector<std::string> name = bufName;
          name.append_range(varName);

          if (resources->sets.size() <= space) {
            resources->sets.resize(space + 1);
          }
          auto bindingIt =
              std::find_if(resources->sets[space].resources.begin(), resources->sets[space].resources.end(),
                           [binding, type](const shaders::ResourceBinding& b) { return b.binding == binding && b.type == type; });
          if (bindingIt == resources->sets[space].resources.end()) {
            shaders::ResourceBinding bindingInfo{
                .name = fmt::format("{}", fmt::join(bufName, ".")),
                .type = type,
                .binding = static_cast<uint32_t>(binding),
                .count = 1,
                .isPush = false,
                .bufferInfo = {.sizeOrStride = sizeOrStride},
            };
            resources->sets[space].resources.push_back(bindingInfo);
            bindingIt = std::prev(resources->sets[space].resources.end());
          }
          bindingIt->bufferInfo.fieldOffsets[fmt::format(
              "{}{}", fmt::join(name, "."), variableLayout->getTypeLayout()->getKind() == slang::TypeReflection::Kind::Array ? "[]" : "")] =
              shaders::FieldInfo{byteOffset, variableLayout->getTypeLayout()->getSize(), variableLayout->getTypeLayout()->getStride()};
        }
      }
      default:
        break;
      }
    }

    // ### Type Layouts
    //
    void printTypeLayout(slang::TypeLayoutReflection* typeLayout, AccessPath accessPath) { printKindSpecificInfo(typeLayout, accessPath); }

    // #### Kind-Specific Information
    //
    void printKindSpecificInfo(slang::TypeLayoutReflection* typeLayout, AccessPath accessPath) {
      switch (typeLayout->getKind()) {
      case slang::TypeReflection::Kind::Struct: {
        int fieldCount = typeLayout->getFieldCount();
        for (int f = 0; f < fieldCount; f++) {
          auto field = typeLayout->getFieldByIndex(f);
          printVariableLayout(field, accessPath);
        }
      } break;

      case slang::TypeReflection::Kind::Array: {
        printTypeLayout(typeLayout->getElementVarLayout()->getTypeLayout(), AccessPath());
      } break;

      case slang::TypeReflection::Kind::Matrix:
      case slang::TypeReflection::Kind::Vector: {
        printTypeLayout(typeLayout->getElementTypeLayout(), AccessPath());
      } break;

      // #### Single-Element Containers
      //
      case slang::TypeReflection::Kind::ConstantBuffer:
      case slang::TypeReflection::Kind::ParameterBlock:
      case slang::TypeReflection::Kind::TextureBuffer:
      case slang::TypeReflection::Kind::ShaderStorageBuffer: {
        auto containerVarLayout = typeLayout->getContainerVarLayout();
        auto elementVarLayout = typeLayout->getElementVarLayout();

        AccessPath innerOffsets = accessPath;
        if (containerVarLayout->getTypeLayout()->getSize(slang::ParameterCategory::ConstantBuffer) != 0) {
          innerOffsets.deepestBuffer = innerOffsets.leaf;
        }
        if (containerVarLayout->getTypeLayout()->getSize(slang::ParameterCategory::SubElementRegisterSpace) != 0) {
          innerOffsets.deepestParameterBlock = innerOffsets.leaf;
        }

        ExtendedAccessPath elementOffsets(innerOffsets, elementVarLayout);

        printTypeLayout(elementVarLayout->getTypeLayout(), elementOffsets);
      } break;

      case slang::TypeReflection::Kind::Resource: {
        if ((typeLayout->getResourceShape() & SLANG_RESOURCE_BASE_SHAPE_MASK) == SLANG_STRUCTURED_BUFFER) {
          AccessPath innerOffsets = accessPath;
          if (typeLayout->getSize(slang::ParameterCategory::ShaderResource) != 0)
            innerOffsets.deepestBuffer = innerOffsets.leaf;
          printTypeLayout(typeLayout->getElementTypeLayout(), innerOffsets);
        }
      } break;
      default:
        break;
      }
    }

    // Programs and Scopes
    // -------------------
    //
    void printProgramLayout(slang::ProgramLayout* programLayout, shaders::ShaderInfo& shaderInfo) {

      AccessPath rootOffsets;
      rootOffsets.valid = true;

      vertexInfo = nullptr;

      {
        resources = &shaderInfo.globalResources;
        printScope(programLayout->getGlobalParamsVarLayout(), rootOffsets);
      }

      int entryPointCount = programLayout->getEntryPointCount();
      for (int i = 0; i < entryPointCount; ++i) {
        shaderInfo.entryPointResources.push_back(shaders::Resources{});
        resources = &shaderInfo.entryPointResources.back();

        auto entryPoint = programLayout->getEntryPointByIndex(i);
        if (entryPoint->getStage() == SLANG_STAGE_VERTEX) {
          vertexInfo = &shaderInfo.vertex;
        } else {
          vertexInfo = nullptr;
        }

        printScope(entryPoint->getVarLayout(), rootOffsets);
      }
    }

    // ### Global Scope
    //
    void printScope(slang::VariableLayoutReflection* scopeVarLayout, AccessPath accessPath) {
      ExtendedAccessPath scopeOffsets(accessPath, scopeVarLayout);

      auto scopeTypeLayout = scopeVarLayout->getTypeLayout();
      switch (scopeTypeLayout->getKind()) {
      // #### Parameters are Grouped Into a Structure
      //
      case slang::TypeReflection::Kind::Struct: {
        int paramCount = scopeTypeLayout->getFieldCount();
        for (int i = 0; i < paramCount; i++) {
          auto param = scopeTypeLayout->getFieldByIndex(i);

          printVariableLayout(param, scopeOffsets);
        }
      } break;

      // #### Wrapped in a Constant Buffer If Needed
      //
      case slang::TypeReflection::Kind::ConstantBuffer:
        printScope(scopeTypeLayout->getElementVarLayout(), scopeOffsets);
        break;

      // #### Wrapped in a Parameter Block If Needed
      //
      case slang::TypeReflection::Kind::ParameterBlock:
        printScope(scopeTypeLayout->getElementVarLayout(), scopeOffsets);
        break;

      default:
        printVariableLayout(scopeVarLayout, accessPath);
        break;
      }
    }

    // #### Varying Parameters
    //
    void printVaryingParameterInfo(slang::VariableLayoutReflection* variableLayout, AccessPath accessPath) {
      auto category = variableLayout->getCategory();
      bool isVarInput = (category == slang::ParameterCategory::VaryingInput);
      auto kind = variableLayout->getTypeLayout()->getKind();
      bool isVar = kind == slang::TypeReflection::Kind::Scalar || kind == slang::TypeReflection::Kind::Vector ||
                   kind == slang::TypeReflection::Kind::Matrix;

      if (isVar && isVarInput) {
        auto semanticName = variableLayout->getSemanticName();
        if (!semanticName) {
          SHDR_WARN("Varying parameter {} has no semantic. This will cause errors with DX12.", variableLayout->getName());
        }

        if (vertexInfo) {
          uint32_t vertexBufferSlot = 0;
          auto var = accessPath.leaf;
          while (var) {
            auto attribCount = var->variableLayout->getVariable()->getUserAttributeCount();
            for (uint32_t idx = 0; idx < attribCount; ++idx) {
              auto attribute = var->variableLayout->getVariable()->getUserAttributeByIndex(idx);
              if (strcmp(attribute->getName(), "vertexBuffer") == 0) {
                int value = 0;
                attribute->getArgumentValueInt(0, &value);
                vertexBufferSlot = static_cast<uint32_t>(value);
                break;
              }
            }
            var = var->outer;
          }

          auto offsets = calculateCumulativeOffset(variableLayout, variableLayout->getCategory(), accessPath);

          if (vertexInfo->layout.size() <= vertexBufferSlot) {
            vertexInfo->layout.resize(vertexBufferSlot + 1);
          }
          auto types = slangTypeToKeptechTypes(variableLayout->getType());
          for (auto type : types) {
            vertexInfo->layout[vertexBufferSlot].layout.push_back(shaders::VertexLayoutEntry{
                .type = type, .semantic = variableLayout->getSemanticName(), .semanticIndex = variableLayout->getSemanticIndex()});
          }
        }
      }
    }

    // Calculating Cumulative Offsets
    // ------------------------------
    //
    struct CumulativeOffset {
      size_t value = 0;
      size_t space = 0;
      std::vector<std::string> name;
    };

    // ### Access Paths

    struct AccessPathNode {
      slang::VariableLayoutReflection* variableLayout = nullptr;
      AccessPathNode* outer = nullptr;
    };

    struct AccessPath {
      AccessPath() {}

      bool valid = false;
      AccessPathNode* deepestBuffer = nullptr;
      AccessPathNode* deepestParameterBlock = nullptr;
      AccessPathNode* leaf = nullptr;
    };

    CumulativeOffset calculateCumulativeOffset(slang::VariableLayoutReflection* variableLayout, slang::ParameterCategory layoutUnit,
                                               AccessPath accessPath) {
      CumulativeOffset result = calculateCumulativeOffset(layoutUnit, accessPath);
      result.value += variableLayout->getOffset(layoutUnit);
      result.space += variableLayout->getBindingSpace(layoutUnit);
      if (variableLayout->getName() != nullptr)
        result.name.push_back(variableLayout->getName());

      return result;
    }

    // ### Tracking Access Paths

    struct ExtendedAccessPath : AccessPath {
      ExtendedAccessPath(AccessPath const& base, slang::VariableLayoutReflection* variableLayout) : AccessPath(base) {
        if (!valid)
          return;

        element.variableLayout = variableLayout;
        element.outer = leaf;

        leaf = &element;
      }

      AccessPathNode element;
    };

    // ### Accumulating Offsets Along An Access Path

    CumulativeOffset calculateCumulativeOffset(slang::ParameterCategory layoutUnit, AccessPath accessPath) {
      CumulativeOffset result{};
      switch (layoutUnit) {
      // #### Layout Units That Don't Require Special Handling
      //
      default:
        for (auto node = accessPath.leaf; node != nullptr; node = node->outer) {
          SHDR_ASSERT(node->variableLayout != nullptr);
          result.value += node->variableLayout->getOffset(layoutUnit);
          if (node->variableLayout->getName() != nullptr)
            result.name.insert(result.name.begin(), node->variableLayout->getName());
        }
        break;

      // #### Bytes
      //
      case slang::ParameterCategory::Uniform:
        for (auto node = accessPath.leaf; node != accessPath.deepestBuffer; node = node->outer) {
          result.value += node->variableLayout->getOffset(layoutUnit);
          if (node->variableLayout->getName() != nullptr)
            result.name.insert(result.name.begin(), node->variableLayout->getName());
        }
        break;

      // #### Layout Units That Care About Spaces
      //
      case slang::ParameterCategory::ConstantBuffer:
      case slang::ParameterCategory::ShaderResource:
      case slang::ParameterCategory::UnorderedAccess:
      case slang::ParameterCategory::SamplerState:
      case slang::ParameterCategory::DescriptorTableSlot:
        for (auto node = accessPath.leaf; node != accessPath.deepestParameterBlock; node = node->outer) {
          result.value += node->variableLayout->getOffset(layoutUnit);
          result.space += node->variableLayout->getBindingSpace(layoutUnit);
          if (node->variableLayout->getName() != nullptr)
            result.name.insert(result.name.begin(), node->variableLayout->getName());
        }
        for (auto node = accessPath.deepestParameterBlock; node != nullptr; node = node->outer) {
          result.space += node->variableLayout->getOffset(slang::ParameterCategory::SubElementRegisterSpace);
          if (node->variableLayout->getName() != nullptr)
            result.name.insert(result.name.begin(), node->variableLayout->getName());
        }
        break;
      }
      return result;
    }

    shaders::Vertex* vertexInfo = nullptr;
    shaders::Resources* resources = nullptr;
  };
} // namespace kt::shader_processor