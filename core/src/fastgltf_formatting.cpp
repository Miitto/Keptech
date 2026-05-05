#include "keptech/core/fastgltf_formatting.hpp"

fmt::format_context::iterator fmt::formatter<fastgltf::Error>::format(const fastgltf::Error& error, fmt::format_context& ctx) const {
  using Error = fastgltf::Error;

  std::string_view msg;

  switch (error) {
  case fastgltf::Error::None:
    msg = "No error";
    break;
  case Error::InvalidPath:
    msg = "The glTF directory passed to load*GLTF is invalid.";
    break;
  case Error::MissingExtensions:
    msg = "One or more extensions are required by the glTF but not enabled "
          "in the Parser.";
    break;
  case Error::UnknownRequiredExtension:
    msg = "An extension required by the glTF is not supported by fastgltf.";
    break;
  case Error::InvalidJson:
    msg = "An error occurred while parsing the JSON.";
    break;
  case Error::InvalidGltf:
    msg = "The glTF is either missing something or has invalid data.";
    break;
  case Error::InvalidOrMissingAssetField:
    msg = "The glTF asset object is missing or invalid.";
    break;
  case Error::InvalidGLB:
    msg = "The GLB container is invalid.";
    break;
    /**
     * A field is missing in the JSON.
     * @note This is only used internally.
     */
  case Error::MissingField:
    msg = "A field is missieng in the JSON";
    break;
  case Error::MissingExternalBuffer:
    msg = "With Options::LoadExternalBuffers, an external buffer was not "
          "found.";
    break;
  case Error::UnsupportedVersion:
    msg = "The glTF version is not supported by fastgltf.";
    break;
  case Error::InvalidURI:
    msg = "A URI from a buffer or image failed to be parsed.";
    break;
  case Error::InvalidFileData:
    msg = "The file data is invalid, or the file type could not be determined.";
    break;
  case Error::FailedWritingFiles:
    msg = "The exporter failed to write some files (buffers/images) to disk.";
    break;
  case Error::FileBufferAllocationFailed:
    msg = "The constructor of GltfDataBuffer failed to allocate a "
          "sufficiently large buffer.";
    break;
  }

  return fmt::formatter<std::string_view>::format(msg, ctx);
}

fmt::format_context::iterator fmt::formatter<fastgltf::AccessorType>::format(const fastgltf::AccessorType& type,
                                                                             fmt::format_context& ctx) const {
  std::string_view msg;

  switch (type) {
  case fastgltf::AccessorType::Scalar:
    msg = "Scalar";
    break;
  case fastgltf::AccessorType::Vec2:
    msg = "Vec2";
    break;
  case fastgltf::AccessorType::Vec3:
    msg = "Vec3";
    break;
  case fastgltf::AccessorType::Vec4:
    msg = "Vec4";
    break;
  case fastgltf::AccessorType::Mat2:
    msg = "Mat2";
    break;
  case fastgltf::AccessorType::Mat3:
    msg = "Mat3";
    break;
  case fastgltf::AccessorType::Mat4:
    msg = "Mat4";
    break;
  case fastgltf::AccessorType::Invalid:
    msg = "Invalid accessor type";
    break;
  }

  return fmt::formatter<std::string_view>::format(msg, ctx);
}

fmt::format_context::iterator fmt::formatter<fastgltf::ComponentType>::format(const fastgltf::ComponentType& type,
                                                                              fmt::format_context& ctx) const {
  std::string_view msg;

  switch (type) {
  case fastgltf::ComponentType::Byte:
    msg = "Byte";
    break;
  case fastgltf::ComponentType::UnsignedByte:
    msg = "UByte";
    break;
  case fastgltf::ComponentType::Short:
    msg = "Short";
    break;
  case fastgltf::ComponentType::UnsignedShort:
    msg = "UShort";
    break;
  case fastgltf::ComponentType::Int:
    msg = "Int";
    break;
  case fastgltf::ComponentType::UnsignedInt:
    msg = "UInt";
    break;
  case fastgltf::ComponentType::Float:
    msg = "Float";
    break;
  case fastgltf::ComponentType::Double:
    msg = "Double";
    break;
  case fastgltf::ComponentType::Invalid:
    msg = "Invalid component type";
    break;
  }

  return fmt::formatter<std::string_view>::format(msg, ctx);
}
