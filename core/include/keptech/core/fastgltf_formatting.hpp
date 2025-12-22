#pragma once

#include <fastgltf/core.hpp>
#include <spdlog/fmt/bundled/format.h>
#include <string_view>

template <>
struct fmt::formatter<fastgltf::Error> : fmt::formatter<std::string_view> {
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  template <typename FormatContext>
  auto format(const fastgltf::Error& error, FormatContext& ctx) {
    std::string_view msg;

    using Error = fastgltf::Error;

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
      msg =
          "The file data is invalid, or the file type could not be determined.";
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
};
