#pragma once

#include "keptech/shaders/info.hpp"
#include "keptech/shaders/resources.hpp"
#include "shader-logger.hpp"
#include "slangFormatting.hpp"
#include <expected>
#include <slang.h>
#include <utility>

namespace kt::shader_processor {

#define ARRAY() for (int _i = (beginArray(), 1); _i; _i = (endArray(), 0))
#define SCOPE() ScopedObject scope##__COUNTER__(*this)

  struct LayoutManager {
    struct AccessPath;

    void printVariable(slang::VariableReflection* varLayout) {
      SCOPE();

      const char* name = varLayout->getName();
      slang::TypeReflection* type = varLayout->getType();

      key("name");
      printQuotesString(name);
      key("type");
      printType(type);
    }

    void printType(slang::TypeReflection* type) {
      SCOPE();

      const char* name = type->getName();
      slang::TypeReflection::Kind kind = type->getKind();
      key("name");
      printQuotesString(name);
      key("kind");
      print(kind);

      printCommonTypeInfo(type);

      switch (kind) {
      case slang::TypeReflection::Kind::Struct: {
        key("fields");
        int fieldCount = type->getFieldCount();

        ARRAY();
        for (int i = 0; i < fieldCount; ++i) {
          element();
          auto fieldLayout = type->getFieldByIndex(i);
          printVariable(fieldLayout);
        }
      } break;
      case slang::TypeReflection::Kind::Array:
      case slang::TypeReflection::Kind::Vector:
      case slang::TypeReflection::Kind::Matrix: {
        key("elementType");
        printType(type->getElementType());
      } break;
      case slang::TypeReflection::Kind::Resource: {
        key("resultType");
        printType(type->getResourceResultType());
      } break;
      case slang::TypeReflection::Kind::ConstantBuffer:
      case slang::TypeReflection::Kind::ParameterBlock:
      case slang::TypeReflection::Kind::TextureBuffer:
      case slang::TypeReflection::Kind::ShaderStorageBuffer: {
        key("element type");
        printType(type->getElementType());
      } break;
      default:
        break;
      }
    }

    void printPossiblyUnbounded(size_t value) {
      if (value == ~size_t(0)) {
        printf("unbounded");
      } else {
        printf("%u", unsigned(value));
      }
    }

    void printCommonTypeInfo(slang::TypeReflection* type) {
      switch (type->getKind()) {
      // #### Scalar Types
      //
      case slang::TypeReflection::Kind::Scalar: {
        key("scalar type");
        print(type->getScalarType());
      } break;

      // #### Array Types
      //
      case slang::TypeReflection::Kind::Array: {
        key("element count");
        printPossiblyUnbounded(type->getElementCount());
      } break;

      // #### Vector Types
      //
      case slang::TypeReflection::Kind::Vector: {
        key("element count");
        print(type->getElementCount());
      } break;

      // #### Matrix Types
      //
      case slang::TypeReflection::Kind::Matrix: {
        key("row count");
        print(type->getRowCount());

        key("column count");
        print(type->getColumnCount());
      } break;

      // #### Resource Types
      //
      case slang::TypeReflection::Kind::Resource: {
        key("shape");
        print(type->getResourceShape());

        key("access");
        print(type->getResourceAccess());
      } break;

      default:
        break;
      }
    }

    void printVariableLayout(slang::VariableLayoutReflection* variableLayout, AccessPath accessPath) {
      SCOPE();

      key("name");
      printQuotesString(variableLayout->getName());

      printOffsets(variableLayout, accessPath);

      printVaryingParameterInfo(variableLayout);

      ExtendedAccessPath variablePath(accessPath, variableLayout);

      key("type layout");
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

          if (info->resources.size() <= offset.space) {
            info->resources.resize(offset.space + 1);
          }

          auto bindingIt = std::find_if(
              info->resources[offset.space].resources.begin(), info->resources[offset.space].resources.end(),
              [resourceType, offset](const shaders::ResourceBinding& b) { return b.binding == offset.value && b.type == resourceType; });
          if (bindingIt == info->resources[offset.space].resources.end()) {
            shaders::ResourceBinding bindingInfo{
                .name = fmt::format("{}", fmt::join(offset.name, ".")),
                .type = resourceType,
                .binding = static_cast<uint32_t>(offset.value),
                .count = 1,
                .isPush = false,
                .bufferInfo = {.sizeOrStride = stride},
            };
            info->resources[offset.space].resources.push_back(bindingInfo);
            bindingIt = std::prev(info->resources[offset.space].resources.end());
          }
        } break;
        case SLANG_TEXTURE_1D: {
          auto offset = calculateCumulativeOffset(variableLayout, variableLayout->getCategory(), accessPath);
          if (info->resources.size() <= offset.space) {
            info->resources.resize(offset.space + 1);
          }

          info->resources[offset.space].resources.push_back(shaders::ResourceBinding{
              .name = fmt::format("{}", fmt::join(offset.name, ".")),
              .type = isTexArray ? shaders::ShaderResourceType::Texture1DArray : shaders::ShaderResourceType::Texture1D,
              .binding = static_cast<uint32_t>(offset.value),
              .count = 1,
              .isPush = false,
          });
        } break;
        case SLANG_TEXTURE_2D: {
          auto offset = calculateCumulativeOffset(variableLayout, variableLayout->getCategory(), accessPath);
          if (info->resources.size() <= offset.space) {
            info->resources.resize(offset.space + 1);
          }

          info->resources[offset.space].resources.push_back(shaders::ResourceBinding{
              .name = fmt::format("{}", fmt::join(offset.name, ".")),
              .type = isTexArray ? shaders::ShaderResourceType::Texture2DArray : shaders::ShaderResourceType::Texture2D,
              .binding = static_cast<uint32_t>(offset.value),
              .count = 1,
              .isPush = false,
          });
        } break;
        case SLANG_TEXTURE_3D: {
          auto offset = calculateCumulativeOffset(variableLayout, variableLayout->getCategory(), accessPath);
          if (info->resources.size() <= offset.space) {
            info->resources.resize(offset.space + 1);
          }

          info->resources[offset.space].resources.push_back(shaders::ResourceBinding{
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
          if (info->resources.size() <= offset.space) {
            info->resources.resize(offset.space + 1);
          }

          info->resources[offset.space].resources.push_back(shaders::ResourceBinding{
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
      case slang::TypeReflection::Kind::Vector:
      case slang::TypeReflection::Kind::Matrix:
      case slang::TypeReflection::Kind::Scalar: {
        if (accessPath.deepestBufer == nullptr) {
          // TODO: Push constant
        } else {
          auto bufferVar = accessPath.deepestBufer->variableLayout;

          size_t sizeOrStride = 0;
          shaders::ShaderResourceType type = shaders::ShaderResourceType::UniformBuffer;
          switch (bufferVar->getTypeLayout()->getKind()) {
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

          auto [byteOffset, _s, varName] = calculateCumulativeOffset(variableLayout, variableLayout->getCategory(), accessPath);
          auto [binding, space, bufName] = calculateCumulativeOffset(bufferVar, bufferVar->getCategory(), accessPath);

          std::vector<std::string> name = bufName;
          name.append_range(varName);

          if (info->resources.size() <= space) {
            info->resources.resize(space + 1);
          }
          auto bindingIt =
              std::find_if(info->resources[space].resources.begin(), info->resources[space].resources.end(),
                           [binding, type](const shaders::ResourceBinding& b) { return b.binding == binding && b.type == type; });
          if (bindingIt == info->resources[space].resources.end()) {
            shaders::ResourceBinding bindingInfo{
                .name = fmt::format("{}", fmt::join(bufName, ".")),
                .type = type,
                .binding = static_cast<uint32_t>(binding),
                .count = 1,
                .isPush = false,
                .bufferInfo = {.sizeOrStride = sizeOrStride},
            };
            info->resources[space].resources.push_back(bindingInfo);
            bindingIt = std::prev(info->resources[space].resources.end());
          }
          bindingIt->bufferInfo.fieldOffsets[fmt::format("{}", fmt::join(name, "."))] = byteOffset;
        }
      }
      default:
        break;
      }
    }

    void printRelativeOffsets(slang::VariableLayoutReflection* variableLayout) {
      key("relative");
      int usedLayoutUnitCount = variableLayout->getCategoryCount();
      ARRAY();
      for (int i = 0; i < usedLayoutUnitCount; ++i) {
        element();

        auto layoutUnit = variableLayout->getCategoryByIndex(i);
        printOffset(variableLayout, layoutUnit);
      }
    }

    void printOffset(slang::VariableLayoutReflection* variableLayout, slang::ParameterCategory layoutUnit) {
      printOffset(layoutUnit, variableLayout->getOffset(layoutUnit), variableLayout->getBindingSpace(layoutUnit));
    }

    void printOffset(slang::ParameterCategory layoutUnit, size_t offset, size_t spaceOffset) {
      SCOPE();

      key("value");
      print(offset);
      key("unit");
      print(layoutUnit);

      // #### Spaces / Sets

      switch (layoutUnit) {
      default:
        break;

      case slang::ParameterCategory::ConstantBuffer:
      case slang::ParameterCategory::ShaderResource:
      case slang::ParameterCategory::UnorderedAccess:
      case slang::ParameterCategory::SamplerState:
      case slang::ParameterCategory::DescriptorTableSlot:
        key("space");
        print(spaceOffset);
        break;
      }
    }

    // ### Type Layouts
    //
    void printTypeLayout(slang::TypeLayoutReflection* typeLayout, AccessPath accessPath) {
      SCOPE();

      key("name");
      printQuotesString(typeLayout->getName());
      key("kind");
      print(typeLayout->getKind());
      printCommonTypeInfo(typeLayout->getType());

      printSizes(typeLayout);

      printKindSpecificInfo(typeLayout, accessPath);
    }

    // #### Size
    //
    void printSizes(slang::TypeLayoutReflection* typeLayout) {
      key("size");

      int usedLayoutUnitCount = typeLayout->getCategoryCount();
      ARRAY()
      for (int i = 0; i < usedLayoutUnitCount; ++i) {
        element();

        auto layoutUnit = typeLayout->getCategoryByIndex(i);
        printSize(typeLayout, layoutUnit);
      }

      // #### Alignment and Stride
      if (typeLayout->getSize() != 0) {
        key("alignment in bytes");
        print(typeLayout->getAlignment());

        key("stride in bytes");
        print(typeLayout->getStride());
      }
    }

    void printSize(slang::TypeLayoutReflection* typeLayout, slang::ParameterCategory layoutUnit) {
      printSize(layoutUnit, typeLayout->getSize(layoutUnit));
    }

    void printSize(slang::ParameterCategory layoutUnit, size_t size) {
      SCOPE();

      key("value");
      printPossiblyUnbounded(size);
      key("unit");
      print(layoutUnit);
    }

    // #### Kind-Specific Information
    //
    void printKindSpecificInfo(slang::TypeLayoutReflection* typeLayout, AccessPath accessPath) {
      switch (typeLayout->getKind()) {
      // #### Structure Type Layouts
      //
      case slang::TypeReflection::Kind::Struct: {
        key("fields");

        int fieldCount = typeLayout->getFieldCount();
        ARRAY()
        for (int f = 0; f < fieldCount; f++) {
          element();

          auto field = typeLayout->getFieldByIndex(f);
          printVariableLayout(field, accessPath);
        }
      } break;

      // #### Array Type Layouts
      //
      case slang::TypeReflection::Kind::Array: {
        key("element type layout");
        printTypeLayout(typeLayout->getElementTypeLayout(), AccessPath());
      } break;

      // #### Matrix Type Layouts
      //
      case slang::TypeReflection::Kind::Matrix: {
        key("matrix layout mode");
        print(typeLayout->getMatrixLayoutMode());

        key("element type layout");
        printTypeLayout(typeLayout->getElementTypeLayout(), AccessPath());
      } break;

      case slang::TypeReflection::Kind::Vector: {
        key("element type layout");
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
        innerOffsets.deepestBufer = innerOffsets.leaf;
        if (containerVarLayout->getTypeLayout()->getSize(slang::ParameterCategory::SubElementRegisterSpace) != 0) {
          innerOffsets.deepestParameterBlock = innerOffsets.leaf;
        }

        key("container");
        {
          SCOPE();
          printOffsets(containerVarLayout, innerOffsets);
        }

        key("content");
        {
          SCOPE();

          printOffsets(elementVarLayout, innerOffsets);

          ExtendedAccessPath elementOffsets(innerOffsets, elementVarLayout);

          key("type layout");
          printTypeLayout(elementVarLayout->getTypeLayout(), elementOffsets);
        }
      } break;

      case slang::TypeReflection::Kind::Resource: {
        if ((typeLayout->getResourceShape() & SLANG_RESOURCE_BASE_SHAPE_MASK) == SLANG_STRUCTURED_BUFFER) {
          key("element type layout");
          AccessPath innerOffsets = accessPath;
          innerOffsets.deepestBufer = innerOffsets.leaf;
          printTypeLayout(typeLayout->getElementTypeLayout(), innerOffsets);

        } else {
          key("result type");
          printType(typeLayout->getResourceResultType());
        }
      } break;

      default:
        break;
      }
    }

    // Programs and Scopes
    // -------------------
    //
    void printProgramLayout(slang::ProgramLayout* programLayout) {
      SCOPE();

      AccessPath rootOffsets;
      rootOffsets.valid = true;

      key("global scope");
      {
        SCOPE();
        printScope(programLayout->getGlobalParamsVarLayout(), rootOffsets);
      }

      key("entry points");
      int entryPointCount = programLayout->getEntryPointCount();
      ARRAY()
      for (int i = 0; i < entryPointCount; ++i) {
        element();
        printEntryPointLayout(programLayout->getEntryPointByIndex(i), rootOffsets);
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
        key("parameters");

        int paramCount = scopeTypeLayout->getFieldCount();
        for (int i = 0; i < paramCount; i++) {
          element();

          auto param = scopeTypeLayout->getFieldByIndex(i);

          printVariableLayout(param, scopeOffsets);
        }
      } break;

      // #### Wrapped in a Constant Buffer If Needed
      //
      case slang::TypeReflection::Kind::ConstantBuffer:
        key("automatically-introduced constant buffer");
        {
          SCOPE();
          printOffsets(scopeTypeLayout->getContainerVarLayout(), scopeOffsets);
        }

        printScope(scopeTypeLayout->getElementVarLayout(), scopeOffsets);
        break;

      // #### Wrapped in a Parameter Block If Needed
      //
      case slang::TypeReflection::Kind::ParameterBlock:
        key("automatically-introduced parameter block");
        {
          SCOPE();
          printOffsets(scopeTypeLayout->getContainerVarLayout(), scopeOffsets);
        }

        printScope(scopeTypeLayout->getElementVarLayout(), scopeOffsets);
        break;

      default:
        // Note that this default case is never expected to
        // arise with the current Slang compiler and reflection
        // API, but we include it here as a kind of failsafe.
        //
        key("variable layout");
        printVariableLayout(scopeVarLayout, accessPath);
        break;
      }
    }

    // ### Entry Points
    //
    void printEntryPointLayout(slang::EntryPointReflection* entryPointLayout, AccessPath accessPath) {
      SCOPE();

      key("stage");
      print(entryPointLayout->getStage());

      printStageSpecificInfo(entryPointLayout);

      printScope(entryPointLayout->getVarLayout(), accessPath);

      auto resultVariableLayout = entryPointLayout->getResultVarLayout();
      if (resultVariableLayout->getTypeLayout()->getKind() != slang::TypeReflection::Kind::None) {
        key("result");
        printVariableLayout(resultVariableLayout, accessPath);
      }
    }

    // #### Stage-Specific Information
    //
    void printStageSpecificInfo(slang::EntryPointReflection* entryPointLayout) {
      switch (entryPointLayout->getStage()) {
      default:
        break;

      case SLANG_STAGE_COMPUTE: {
        static const int kAxisCount = 3;
        SlangUInt sizes[kAxisCount];
        entryPointLayout->getComputeThreadGroupSize(kAxisCount, sizes);

        key("thread group size");
        SCOPE();
        key("x");
        print(sizes[0]);
        key("y");
        print(sizes[1]);
        key("z");
        print(sizes[2]);
      } break;

      case SLANG_STAGE_FRAGMENT:
        key("uses any sample-rate inputs");
        printBool(entryPointLayout->usesAnySampleRateInput());
        break;
      }
    }

    // #### Varying Parameters
    //
    void printVaryingParameterInfo(slang::VariableLayoutReflection* variableLayout) {
      if (auto semanticName = variableLayout->getSemanticName()) {
        key("semantic");
        SCOPE();
        key("name");
        printQuotesString(semanticName);
        key("index");
        print(variableLayout->getSemanticIndex());
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
      AccessPathNode* deepestBufer = nullptr;
      AccessPathNode* deepestParameterBlock = nullptr;
      AccessPathNode* leaf = nullptr;
    };

    void printCumulativeOffsets(slang::VariableLayoutReflection* variableLayout, AccessPath accessPath) {
      key("cumulative");

      int usedLayoutUnitCount = variableLayout->getCategoryCount();
      ARRAY();
      for (int i = 0; i < usedLayoutUnitCount; ++i) {
        element();

        auto layoutUnit = variableLayout->getCategoryByIndex(i);
        printCumulativeOffset(variableLayout, layoutUnit, accessPath);
      }
    }

    CumulativeOffset calculateCumulativeOffset(slang::VariableLayoutReflection* variableLayout, slang::ParameterCategory layoutUnit,
                                               AccessPath accessPath) {
      CumulativeOffset result = calculateCumulativeOffset(layoutUnit, accessPath);
      result.value += variableLayout->getOffset(layoutUnit);
      result.space += variableLayout->getBindingSpace(layoutUnit);
      if (variableLayout->getName() != nullptr)
        result.name.push_back(variableLayout->getName());

      return result;
    }

    void printCumulativeOffset(slang::VariableLayoutReflection* variableLayout, slang::ParameterCategory layoutUnit,
                               AccessPath accessPath) {
      CumulativeOffset cumulativeOffset = calculateCumulativeOffset(variableLayout, layoutUnit, accessPath);

      printOffset(layoutUnit, cumulativeOffset.value, cumulativeOffset.space);
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
            result.name.push_back(node->variableLayout->getName());
        }
        break;

      // #### Bytes
      //
      case slang::ParameterCategory::Uniform:
        for (auto node = accessPath.leaf; node != accessPath.deepestBufer; node = node->outer) {
          result.value += node->variableLayout->getOffset(layoutUnit);
          if (node->variableLayout->getName() != nullptr)
            result.name.push_back(node->variableLayout->getName());
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
            result.name.push_back(node->variableLayout->getName());
        }
        for (auto node = accessPath.deepestParameterBlock; node != nullptr; node = node->outer) {
          result.space += node->variableLayout->getOffset(slang::ParameterCategory::SubElementRegisterSpace);
          if (node->variableLayout->getName() != nullptr)
            result.name.push_back(node->variableLayout->getName());
        }
        break;
      }
      return result;
    }

    void printOffsets(slang::VariableLayoutReflection* variableLayout, AccessPath accessPath) {
      key("offset");
      {
        SCOPE();

        if (accessPath.valid) {
          printCumulativeOffsets(variableLayout, accessPath);
        } else {
          printRelativeOffsets(variableLayout);
        }
      }
    }

    void beginObject() { ++depth; }
    void endObject() { --depth; }
    void beginArray() { ++depth; }
    void endArray() { --depth; }

    struct ScopedObject {
      LayoutManager& manager;
      ScopedObject(LayoutManager& manager) : manager(manager) { manager.beginObject(); }
      ~ScopedObject() { manager.endObject(); }
    };

    void newLine() {
      SHDR_TRACE("{}{}", std::string(depth * 2, ' '), currentLine);
      currentLine.clear();
    }

    bool afterArrayElement = true;

    void element() {
      newLine();
      currentLine += "- ";
      afterArrayElement = true;
    }

    void key(const char* key) {
      if (!afterArrayElement) {
        newLine();
      }
      currentLine += fmt::format("{}: ", key);
      afterArrayElement = false;
    }
    void printQuotesString(const char* str) {
      if (str) {
        currentLine += fmt::format("\"{}\"", str);
      } else {
        currentLine += "null";
      }
    }

    void printBool(bool value) { currentLine += value ? "true" : "false"; }
    template <typename T> void print(const T& value) { currentLine += fmt::format("{}", value); }

    size_t depth = 0;
    std::string currentLine;

    shaders::ShaderInfo* info;
  };
} // namespace kt::shader_processor