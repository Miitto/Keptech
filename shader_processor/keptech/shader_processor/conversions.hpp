#pragma once

#include "keptech/shaders/shader.hpp"
#include <slang-com-ptr.h>
#include <slang.h>
#include <stdexcept>
#include <utility>
#include <vector>

const char* slangStagetoString(SlangStage stage) {
  switch (stage) {
  case SLANG_STAGE_VERTEX:
    return "vertex";
  case SLANG_STAGE_FRAGMENT:
    return "fragment";
  case SLANG_STAGE_COMPUTE:
    return "compute";
  case SLANG_STAGE_NONE:
    return "none";
  case SLANG_STAGE_HULL:
    return "hull";
  case SLANG_STAGE_DOMAIN:
    return "domain";
  case SLANG_STAGE_GEOMETRY:
    return "geometry";
  case SLANG_STAGE_RAY_GENERATION:
    return "ray generation";
  case SLANG_STAGE_INTERSECTION:
    return "intersection";
  case SLANG_STAGE_ANY_HIT:
    return "any hit";
  case SLANG_STAGE_CLOSEST_HIT:
    return "closest hit";
  case SLANG_STAGE_MISS:
    return "miss";
  case SLANG_STAGE_CALLABLE:
    return "callable";
  case SLANG_STAGE_MESH:
    return "mesh";
  case SLANG_STAGE_AMPLIFICATION:
    return "amplification";
  case SLANG_STAGE_DISPATCH:
    return "dispatch";
  case SLANG_STAGE_COUNT:
    return "count";
  }

  throw std::runtime_error("Unsupported shader stage");
}

kt::shaders::ShaderStages slangStagetoKeptechStage(SlangStage stage) {
  switch (stage) {
  case SLANG_STAGE_VERTEX:
    return kt::shaders::ShaderStages::Vertex;
  case SLANG_STAGE_FRAGMENT:
    return kt::shaders::ShaderStages::Fragment;
  case SLANG_STAGE_GEOMETRY:
    return kt::shaders::ShaderStages::Geometry;
  case SLANG_STAGE_COMPUTE:
    return kt::shaders::ShaderStages::Compute;
  case SLANG_STAGE_MESH:
    return kt::shaders::ShaderStages::Mesh;
  case SLANG_STAGE_AMPLIFICATION:
    return kt::shaders::ShaderStages::Task;
  default:
    throw std::runtime_error("Unsupported shader stage");
  }
}

const std::vector<kt::shaders::DataType> slangTypeToKeptechTypes(slang::TypeReflection* type) {
  using namespace kt::shaders;

  switch (type->getKind()) {
  case slang::TypeReflection::Kind::Scalar: {
    auto scalarType = type->getScalarType();
    switch (scalarType) {
    case slang::TypeReflection::None:
      return {DataType::None};
    case slang::TypeReflection::Void:
      return {DataType::Void};
    case slang::TypeReflection::Bool:
      return {DataType::Bool};
    case slang::TypeReflection::Int32:
      return {DataType::I32};
    case slang::TypeReflection::UInt32:
      return {DataType::U32};
    case slang::TypeReflection::Int64:
      return {DataType::I64};
    case slang::TypeReflection::UInt64:
      return {DataType::U64};
    case slang::TypeReflection::Float16:
      return {DataType::F16};
    case slang::TypeReflection::Float32:
      return {DataType::F32};
    case slang::TypeReflection::Float64:
      return {DataType::F64};
    case slang::TypeReflection::Int8:
      return {DataType::I8};
    case slang::TypeReflection::UInt8:
      return {DataType::U8};
    case slang::TypeReflection::Int16:
      return {DataType::I16};
    case slang::TypeReflection::UInt16:
      return {DataType::U16};
    case slang::TypeReflection::IntPtr:
    case slang::TypeReflection::UIntPtr:
    case slang::TypeReflection::BFloat16:
    case slang::TypeReflection::FloatE4M3:
    case slang::TypeReflection::FloatE5M2:
      throw std::runtime_error("Unsupported scalar type for resource type extraction");
    }
  } break;
  case slang::TypeReflection::Kind::Vector: {
    auto elemType = type->getElementType()->getScalarType();
    auto elemCount = type->getElementCount();
    switch (elemCount) {
    case 2:
      switch (elemType) {
      case slang::TypeReflection::None:
      case slang::TypeReflection::Void:
      case slang::TypeReflection::Bool:
      case slang::TypeReflection::IntPtr:
      case slang::TypeReflection::UIntPtr:
      case slang::TypeReflection::BFloat16:
      case slang::TypeReflection::FloatE4M3:
      case slang::TypeReflection::FloatE5M2:
        throw std::runtime_error("Unsupported type in vector for resource type extraction");
      case slang::TypeReflection::Int32:
        return {DataType::I32_2};
      case slang::TypeReflection::UInt32:
        return {DataType::U32_2};
      case slang::TypeReflection::Int64:
        return {DataType::I64_2};
      case slang::TypeReflection::UInt64:
        return {DataType::U64_2};
      case slang::TypeReflection::Float16:
        return {DataType::F16_2};
      case slang::TypeReflection::Float32:
        return {DataType::F32_2};
      case slang::TypeReflection::Float64:
        return {DataType::F64_2};
      case slang::TypeReflection::Int8:
        return {DataType::I8_2};
      case slang::TypeReflection::UInt8:
        return {DataType::U8_2};
      case slang::TypeReflection::Int16:
        return {DataType::I16_2};
      case slang::TypeReflection::UInt16:
        return {DataType::U16_2};
      }
    case 3:
      switch (elemType) {
      case slang::TypeReflection::None:
      case slang::TypeReflection::Void:
      case slang::TypeReflection::Bool:
      case slang::TypeReflection::IntPtr:
      case slang::TypeReflection::UIntPtr:
      case slang::TypeReflection::BFloat16:
      case slang::TypeReflection::FloatE4M3:
      case slang::TypeReflection::FloatE5M2:
        throw std::runtime_error("Unsupported type in vector for resource type extraction");
      case slang::TypeReflection::Int32:
        return {DataType::I32_3};
      case slang::TypeReflection::UInt32:
        return {DataType::U32_3};
      case slang::TypeReflection::Int64:
        return {DataType::I64_3};
      case slang::TypeReflection::UInt64:
        return {DataType::U64_3};
      case slang::TypeReflection::Float16:
        return {DataType::F16_3};
      case slang::TypeReflection::Float32:
        return {DataType::F32_3};
      case slang::TypeReflection::Float64:
        return {DataType::F64_3};
      case slang::TypeReflection::Int8:
        return {DataType::I8_3};
      case slang::TypeReflection::UInt8:
        return {DataType::U8_3};
      case slang::TypeReflection::Int16:
        return {DataType::I16_3};
      case slang::TypeReflection::UInt16:
        return {DataType::U16_3};
      }

    case 4:
      switch (elemType) {
      case slang::TypeReflection::None:
      case slang::TypeReflection::Void:
      case slang::TypeReflection::Bool:
      case slang::TypeReflection::IntPtr:
      case slang::TypeReflection::UIntPtr:
      case slang::TypeReflection::BFloat16:
      case slang::TypeReflection::FloatE4M3:
      case slang::TypeReflection::FloatE5M2:
        throw std::runtime_error("Unsupported type in vector for resource type extraction");
      case slang::TypeReflection::Int32:
        return {DataType::I32_4};
      case slang::TypeReflection::UInt32:
        return {DataType::U32_4};
      case slang::TypeReflection::Int64:
        return {DataType::I64_4};
      case slang::TypeReflection::UInt64:
        return {DataType::U64_4};
      case slang::TypeReflection::Float16:
        return {DataType::F16_4};
      case slang::TypeReflection::Float32:
        return {DataType::F32_4};
      case slang::TypeReflection::Float64:
        return {DataType::F64_4};
      case slang::TypeReflection::Int8:
        return {DataType::I8_4};
      case slang::TypeReflection::UInt8:
        return {DataType::U8_4};
      case slang::TypeReflection::Int16:
        return {DataType::I16_4};
      case slang::TypeReflection::UInt16:
        return {DataType::U16_4};
      }

    default:
      std::unreachable();
    }
  } break;
  case slang::TypeReflection::Kind::Array: {
    auto typeElem = type->getElementType();
    auto t = slangTypeToKeptechTypes(typeElem)[0];
    auto elemCount = type->getElementCount();
    std::vector<DataType> arrayTypes;
    arrayTypes.reserve(elemCount);
    for (size_t i = 0; i < elemCount; ++i) {
      arrayTypes.push_back(t);
    }
    return arrayTypes;
  }
  case slang::TypeReflection::Kind::Struct: {
    auto fieldCount = type->getFieldCount();
    std::vector<DataType> fieldTypes{};
    for (uint32_t i = 0; i < fieldCount; ++i) {
      slang::VariableReflection* field = type->getFieldByIndex(i);
      auto fieldType = field->getType();
      auto t = slangTypeToKeptechTypes(fieldType);
      fieldTypes.append_range(t);
    }
    return fieldTypes;
  }
  case slang::TypeReflection::Kind::Matrix: {
    auto elemType = type->getElementType()->getScalarType();
    auto rowCount = type->getRowCount();
    auto colCount = type->getColumnCount();

    if (rowCount != 4 || colCount != 4) {
      throw std::runtime_error("Unsupported matrix size in type for resource type extraction");
    }
    if (elemType != slang::TypeReflection::Float32) {
      throw std::runtime_error("Unsupported matrix element type for resource type extraction");
    }
  }

    return {DataType::F32_4x4};
  }
  throw std::runtime_error("Unsupported type kind for resource type extraction");
}
