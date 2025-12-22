#include "keptech/vulkan/gltf/loaded.hpp"
#include <fastgltf/core.hpp>

#include <keptech/core/fastgltf_formatting.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace {
  vk::Filter getFilter(fastgltf::Filter filter) {
    using fastgltf::Filter;
    switch (filter) {
    case Filter::Nearest:
    case Filter::NearestMipMapNearest:
    case Filter::LinearMipMapNearest:
      return vk::Filter::eNearest;
    case Filter::Linear:
    case Filter::NearestMipMapLinear:
    case Filter::LinearMipMapLinear:
      return vk::Filter::eLinear;
    default:
      return vk::Filter::eNearest;
    }
  }

  vk::SamplerMipmapMode getMipmapMode(fastgltf::Filter filter) {
    using fastgltf::Filter;
    switch (filter) {
    case Filter::NearestMipMapNearest:
    case Filter::LinearMipMapNearest:
      return vk::SamplerMipmapMode::eNearest;
    default:
      return vk::SamplerMipmapMode::eLinear;
    }
  }
} // namespace

namespace keptech::vulkan::gltf {
  std::expected<LoadedGltf, std::string>
  LoadedGltf::fromFile(std::string_view spath) {

    std::filesystem::path path{spath};

    fastgltf::Parser parser{};

    constexpr auto options = fastgltf::Options::DontRequireValidAssetMember |
                             fastgltf::Options::AllowDouble |
                             fastgltf::Options::LoadExternalBuffers;

    auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);
    if (!bool(gltfFile)) {
      return std::unexpected(
          fmt::format("Failed to load glTF file: {}", gltfFile.error()));
    }

    auto asset_res =
        parser.loadGltf(gltfFile.get(), path.parent_path(), options);
    if (!bool(asset_res)) {
      return std::unexpected(
          fmt::format("Failed to parse glTF file: {}", asset_res.error()));
    }

    auto& asset = asset_res.get();

    LoadedGltf loadedGltf{};

    for (auto& mesh : asset.meshes) {
      loadedGltf.meshses.emplace(
          mesh.name, std::make_shared<fastgltf::Mesh>(std::move(mesh)));
    }

    for (auto& material : asset.materials) {
      loadedGltf.materials.emplace(
          material.name,
          std::make_shared<fastgltf::Material>(std::move(material)));
    }

    for (auto& texture : asset.textures) {
      loadedGltf.textures.emplace(
          texture.name,
          std::make_shared<fastgltf::Texture>(std::move(texture)));
    }

    for (auto& image : asset.images) {
      loadedGltf.images.emplace(
          image.name, std::make_shared<fastgltf::Image>(std::move(image)));
    }

    for (auto& sampler : asset.samplers) {
      loadedGltf.samplers.emplace(
          sampler.name,
          std::make_shared<fastgltf::Sampler>(std::move(sampler)));
    }

    for (auto& node : asset.nodes) {
      loadedGltf.nodes.emplace(
          node.name, std::make_shared<fastgltf::Node>(std::move(node)));
    }

    return loadedGltf;
  }
} // namespace keptech::vulkan::gltf
