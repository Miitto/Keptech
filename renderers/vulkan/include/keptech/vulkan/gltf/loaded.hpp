#pragma once

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <unordered_map>

namespace keptech::vulkan::gltf {
  struct LoadedGltf {

    std::unordered_map<std::string, std::shared_ptr<fastgltf::Mesh>> meshses;
    std::unordered_map<std::string, std::shared_ptr<fastgltf::Material>>
        materials;
    std::unordered_map<std::string, std::shared_ptr<fastgltf::Texture>>
        textures;
    std::unordered_map<std::string, std::shared_ptr<fastgltf::Image>> images;
    std::unordered_map<std::string, std::shared_ptr<fastgltf::Sampler>>
        samplers;
    std::unordered_map<std::string, std::shared_ptr<fastgltf::Node>> nodes;

    static std::expected<LoadedGltf, std::string>
    fromFile(std::string_view path);
  };
} // namespace keptech::vulkan::gltf
