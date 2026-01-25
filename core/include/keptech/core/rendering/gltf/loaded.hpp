#pragma once

#include "keptech/core/rendering/mesh.hpp"
#include <expected>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <string>
#include <unordered_map>

namespace keptech::gltf {

  struct LoadedGltf {
    std::unordered_map<std::string, std::shared_ptr<MeshData>> meshses;
    std::unordered_map<std::string, std::shared_ptr<fastgltf::Material>>
        materials;
    std::unordered_map<std::string, std::shared_ptr<fastgltf::Texture>>
        textures;
    std::unordered_map<std::string, std::shared_ptr<fastgltf::Image>> images;
    std::unordered_map<std::string, std::shared_ptr<fastgltf::Sampler>>
        samplers;
    std::unordered_map<std::string, std::shared_ptr<fastgltf::Node>> nodes;

    std::vector<std::shared_ptr<fastgltf::Node>> roots;

    static std::expected<LoadedGltf, std::string>
    fromFile(std::string_view path);
  };
} // namespace keptech::gltf
