#include "imageFile.hpp"
#include "stbCubeImageFile.hpp"
#include "stbImageFile.hpp"
#include <filesystem>

namespace kt {
  Result<std::unique_ptr<ImageFile>, ImageFileLoadError, ImageFileLoadError::None> ImageFile::fromFile(std::string name,
                                                                                                       const std::string& path) {
    std::filesystem::path filePath(path);

    auto extension = filePath.extension();
    if (extension == "ktx" || extension == "ktx2") {
      KT_ABORT("KTX/KTX2 loading not implemented yet");
    } else {
      auto result = StbImageFile::fromFile(std::move(name), path);
      if (result.isError()) {
        return result.error();
      }
      return {std::make_unique<StbImageFile>(std::move(result.value()))};
    }
  }

  Result<std::unique_ptr<ImageFile>, ImageFileLoadError, ImageFileLoadError::None>
  ImageFile::cubeFromFile(std::string name, const std::string_view& posX, const std::string_view& negX, const std::string_view& posY,
                          const std::string_view& negY, const std::string_view& posZ, const std::string_view& negZ) {
    auto result = StbCubeImageFile::fromFile(std::move(name), std::string(posX), std::string(negX), std::string(posY), std::string(negY),
                                             std::string(posZ), std::string(negZ));
    if (result.isError()) {
      return result.error();
    }

    return {std::make_unique<StbCubeImageFile>(std::move(result.value()))};
  }
} // namespace kt

fmt::format_context::iterator fmt::formatter<kt::ImageFileLoadError>::format(const kt::ImageFileLoadError& error,
                                                                             fmt::format_context& ctx) const {
  switch (error) {
  case kt::ImageFileLoadError::None:
    return fmt::formatter<std::string_view>::format("None", ctx);
  case kt::ImageFileLoadError::FileNotFound:
    return fmt::formatter<std::string_view>::format("FileNotFound", ctx);
  case kt::ImageFileLoadError::InvalidFormat:
    return fmt::formatter<std::string_view>::format("InvalidFormat", ctx);
  case kt::ImageFileLoadError::LoadError:
    return fmt::formatter<std::string_view>::format("LoadError", ctx);
  }
}